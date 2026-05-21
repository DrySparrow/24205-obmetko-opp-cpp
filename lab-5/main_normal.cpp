#include <mpi.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <stdio.h>
#include <stdlib.h>

class LoadBalancer {
public:
    LoadBalancer(int r, int s): rank(r), size(s) {
        tasks.reserve(START_TASKS_SIZE);
        responder_thread = std::thread(&LoadBalancer::responder_routine, this);
    }

    ~LoadBalancer() {
        is_running = false;
        int stop_sig = -1;
        MPI_Send(&stop_sig, 1, MPI_INT, rank, TAG_REQUEST, MPI_COMM_WORLD);
        if (responder_thread.joinable()) {
            responder_thread.join();
        }
    }

    void run(int iterations) {
        double start_time = MPI_Wtime();

        long cur_task_count = TOTAL_TASKS / size;

        if (rank == size - 1) {
            cur_task_count += TOTAL_TASKS % size;
        }

        double total_imbalance_sum = 0;

        for (int iter = 0; iter < iterations; ++iter) {
            double cur_total_weight = 0;
            // pyramid
            {
                std::lock_guard<std::mutex> lock(mtx);
                remaining = cur_task_count;
                tasks.resize(remaining);

                int center_process = iter % size;

                double total_coeffs = 0;
                for (int i = 0; i < size; ++i) {
                    total_coeffs += (size - std::abs(i - center_process));
                }

                double cur_coeff = size - std::abs(rank - center_process);
                double initial_weight = (TOTAL_WEIGHT * cur_coeff / total_coeffs);

                tasks.assign(remaining, static_cast<int>(initial_weight / remaining));
            }
            // all task on one
            /*{
                std::lock_guard<std::mutex> lock(mtx);

                if (rank == 0) {
                    remaining = TOTAL_TASKS;
                    tasks.resize(remaining);
                    tasks.assign(remaining, static_cast<int>(TOTAL_WEIGHT / remaining));
                } else {
                    remaining = 0;
                    tasks.clear();
                }
            }*/
            // all to all
            /*{
                std::lock_guard<std::mutex> lock(mtx);

                remaining = cur_task_count;
                tasks.resize(remaining);

                int task_weight = static_cast<int>(TOTAL_WEIGHT / TOTAL_TASKS);

                tasks.assign(remaining, task_weight);
            }*/

            while (true) {
                std::vector<int> local_chunk;
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    if (remaining > 0) {
                        long chunk_size = std::min(remaining, (long)1000);
                        for (int i = 0; i < chunk_size; ++i) {
                            local_chunk.push_back(tasks[remaining - 1]);
                            remaining--;
                        }
                    }
                }

                if (!local_chunk.empty()) {
                    for (int weight : local_chunk) {
                        do_work(weight);
                        cur_total_weight += weight;
                        total_done++;
                    }
                } else {
                    if (!steal_tasks()) {
                        break;
                    }
                }
            }

            double local_load = cur_total_weight;
            double max_load_on_iter = 0;
            double sum_load_on_iter = 0;

            MPI_Allreduce(&local_load, &max_load_on_iter, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
            MPI_Allreduce(&local_load, &sum_load_on_iter, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

            double avg_load_on_iter = sum_load_on_iter / size;
            double iter_imbalance = max_load_on_iter / avg_load_on_iter;
            total_imbalance_sum += iter_imbalance;
        }
        double end_time = MPI_Wtime();

        double load_imbalance_factor = total_imbalance_sum / iterations;
        if (rank == 0) {
            std::cout << "Load Imbalance Factor: " << load_imbalance_factor << "\n";
        }

        std::cout << "Rank " << rank << ": Done " << total_done << " tasks in " << end_time - start_time << "s\n";
    }

private:
    static constexpr int TAG_REQUEST = 10;
    static constexpr int TAG_ANSWER = 20;
    static constexpr int TAG_DATA = 30;
    static constexpr int MIN_TASKS_TO_SHARE = 2;
    static constexpr int START_TASKS_SIZE = 1000;
    static constexpr long TOTAL_TASKS = 50000000;
    static constexpr long TOTAL_WEIGHT = (TOTAL_TASKS * 10);

    int rank, size;
    std::vector<int> tasks;
    long remaining = 0;
    long total_done = 0;

    std::mutex mtx;
    std::atomic<bool> is_running{true};
    std::thread responder_thread;

    void do_work(int weight) {
        double res = 0;
        volatile double dummy = 0;
        for (int i = 0; i < weight; ++i) {
            res += std::sin(i) * std::tan(i);
        }
        dummy = res;
    }

    void responder_routine() {
        while (is_running) {
            int request_from;
            MPI_Status status;

            MPI_Recv(&request_from, 1, MPI_INT, MPI_ANY_SOURCE, TAG_REQUEST, MPI_COMM_WORLD, &status);
            if (request_from == -1) {
                break;
            }

            std::lock_guard<std::mutex> lock(mtx);
            if (remaining > MIN_TASKS_TO_SHARE) {
                int to_share = remaining / 2;
                remaining -= to_share;

                MPI_Send(&to_share, 1, MPI_INT, request_from, TAG_ANSWER, MPI_COMM_WORLD);
                MPI_Send(&tasks[remaining], to_share, MPI_INT, request_from, TAG_DATA, MPI_COMM_WORLD);
            } else {
                int zero = 0;
                MPI_Send(&zero, 1, MPI_INT, request_from, TAG_ANSWER, MPI_COMM_WORLD);
            }
        }
    }

    bool steal_tasks() {
        static bool seeded = false;
        if (!seeded) {
            srand(time(NULL));
            seeded = true;
        }

        /*int start_victim = rand() % size;
        int max_attempts = std::min(size - 1, 8);*/

        int start_victim = 0;
        int max_attempts = size;

        for (int i = 0; i < max_attempts; ++i) {
            int victim = (start_victim + i) % size;
            if (victim == rank) {
                continue;
            }

            int tasks_received = 0;
            MPI_Send(&rank, 1, MPI_INT, victim, TAG_REQUEST, MPI_COMM_WORLD);
            MPI_Recv(&tasks_received, 1, MPI_INT, victim, TAG_ANSWER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            if (tasks_received > 0) {
                std::lock_guard<std::mutex> lock(mtx);
                tasks.resize(tasks_received);
                MPI_Recv(tasks.data(), tasks_received, MPI_INT, victim, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                remaining = tasks_received;
                return true;
            }
        }
        return false;
    }
};

int main(int argc, char** argv) {
    int iterations = std::stoi(argv[1]);
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    if (provided != MPI_THREAD_MULTIPLE) {
        std::cerr << "MPI_THREAD_MULTIPLE not supported" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    {
        LoadBalancer lb(rank, size);
        lb.run(iterations);
    }

    MPI_Finalize();
    return 0;
}
