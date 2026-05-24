#include <mpi.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

extern "C" {
    int PMPI_Mprobe(...) { return 0; }  
    int PMPI_Mrecv(...) { return 0; }
    int PMPI_Improbe(...) { return 0; }
    int PMPI_Imrecv(...) { return 0; }
    int PMPI_Comm_create_group(...) { return 0; }
    int PMPI_Comm_split_type(...) { return 0; }
    int PMPI_Comm_idup(...) { return 0; }
    int PMPI_Comm_dup_with_info(...) { return 0; }
    int PMPI_Comm_set_info(...) { return 0; }
    int PMPI_Comm_get_info(...) { return 0; }
}

class LoadBalancer {
public:
    LoadBalancer(int r, int s): rank(r), size(s) {
        tasks.reserve(START_TASKS_SIZE);
        responder_thread = std::thread(&LoadBalancer::responder_routine, this);
    }

    ~LoadBalancer() {
        is_running = false;
        int stop_sig = -1;
        // Сигнализируем фоновому потоку завершить работу
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

        // полная ошибка вычислений
        double total_imbalance_sum = 0;

        for (int iter = 0; iter < iterations; ++iter) {
            double cur_total_weight = 0;
            long iter_tasks_done = 0;
            int center_process = iter % size;

            // Расчет веса без потери точности (метод префиксных сумм)
            {
                std::lock_guard<std::mutex> lock(mtx);
                remaining = cur_task_count;
                tasks.resize(remaining);

                long long total_coeffs = 0;
                for (int i = 0; i < size; ++i) {
                    total_coeffs += (size - std::abs(i - center_process));
                }

                long long prefix_coeffs = 0;
                for (int i = 0; i < rank; ++i) {
                    prefix_coeffs += (size - std::abs(i - center_process));
                }
                long long my_coeff = size - std::abs(rank - center_process);

                // Границы весов для текущего процесса (исключают погрешности округления)
                long long my_start_weight = (TOTAL_WEIGHT * prefix_coeffs) / total_coeffs;
                long long my_end_weight = (TOTAL_WEIGHT * (prefix_coeffs + my_coeff)) / total_coeffs;
                long long initial_weight = my_end_weight - my_start_weight;

                // Распределение веса внутри процесса по задачам без потерь
                long long base_task_weight = initial_weight / remaining;
                long long task_remainder = initial_weight % remaining;

                tasks.assign(remaining, static_cast<int>(base_task_weight));
                for (long long i = 0; i < task_remainder; ++i) {
                    tasks[i]++;
                }
            }

            // Основной цикл выполнения и кражи задач
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
                        iter_tasks_done++;
                        total_done++;
                    }
                } else {
                    if (!steal_tasks()) {
                        break; // Задач больше нет ни у кого
                    }
                }
            }

            // ВЫВОД СТАТИСТИКИ ЗА ИТЕРАЦИЮ (Выполнение требования)
            std::cout << "[Iter " << iter << "][Rank " << rank 
                      << "] Executed tasks: " << iter_tasks_done 
                      << ", Total weight: " << cur_total_weight << std::endl;

            // Синхронизация и сбор статистики баланса
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
        if (rank == 0) {
            std::cout << "\n>>> Global Load Imbalance Factor: " << total_imbalance_sum / iterations << "\n";
        }
        std::cout << "Rank " << rank << ": Total done " << total_done << " tasks in " << end_time - start_time << "s\n";
    }

private:
    static constexpr int TAG_REQUEST = 1;
    static constexpr int TAG_ANSWER = 2;
    static constexpr int TAG_DATA = 3;
    static constexpr int MIN_TASKS_TO_SHARE = 2;
    static constexpr int START_TASKS_SIZE = 1000;
    static constexpr long TOTAL_TASKS = 10000000;
    static constexpr long TOTAL_WEIGHT = (TOTAL_TASKS * 10);

    int rank, size;
    std::vector<int> tasks;
    long remaining = 0;
    long total_done = 0;

    std::mutex mtx;
    std::atomic<bool> is_running{true};
    std::thread responder_thread;

    // функция нагрузки на процесс
    void do_work(int weight) {
        double res = 0;
        volatile double dummy = 0;
        for (int i = 0; i < weight; ++i) {
            res += std::sin(i) * std::tan(i);
        }
        dummy = res;
    }

    // функция, выполняющаяся в отдельном потоке
    void responder_routine() {
        while (is_running) {
            int request_from;
            MPI_Status status;

            // ждём пока другой процесс запросит задачи у текущего
            MPI_Recv(&request_from, 1, MPI_INT, MPI_ANY_SOURCE, TAG_REQUEST, MPI_COMM_WORLD, &status);
            if (request_from == -1) {
                break;
            }

            int to_share = 0;
            std::vector<int> tasks_to_send;

            {
                // если задач больше чем MIN_TASKS_TO_SHARE, то одтаём половину из них
                std::lock_guard<std::mutex> lock(mtx);
                if (remaining > MIN_TASKS_TO_SHARE) {
                    to_share = remaining / 2;
                    remaining -= to_share;
                    // Безопасно копируем кусок вектора во временный буфер внутри лока
                    tasks_to_send.assign(tasks.begin() + remaining, tasks.begin() + remaining + to_share);
                }
            }

            // Сетевые вызовы делаем свободно БЕЗ блокировки мьютекса!
            if (to_share > 0) {
                MPI_Send(&to_share, 1, MPI_INT, request_from, TAG_ANSWER, MPI_COMM_WORLD);
                MPI_Send(tasks_to_send.data(), to_share, MPI_INT, request_from, TAG_DATA, MPI_COMM_WORLD);
            } else {
                int zero = 0;
                MPI_Send(&zero, 1, MPI_INT, request_from, TAG_ANSWER, MPI_COMM_WORLD);
            }
        }
    }

    // ОПТИМИЗИРОВАНО: MPI_Recv вынесен за пределы критической секции
    bool steal_tasks() {
        // Проходим по всем процессам, расширяя радиус поиска: +1, -1, +2, -2...
        for (int step = 1; step < size; ++step) {
            // Формируем чередующееся смещение
            // Если step нечетный -> смещение вправо (+), если четный -> влево (-)
            int offset = (step % 2 != 0) ? (step / 2 + 1) : -(step / 2);
            
            // Вычисляем номер жертвы с учетом замыкания в кольцо
            int victim = (rank + offset + size) % size;

            // запрос на получение задач
            int tasks_received = 0;
            MPI_Send(&rank, 1, MPI_INT, victim, TAG_REQUEST, MPI_COMM_WORLD);
            MPI_Recv(&tasks_received, 1, MPI_INT, victim, TAG_ANSWER, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // если получили задачи
            if (tasks_received > 0) {
                std::vector<int> received_buffer(tasks_received);
                // Принимаем данные во временный буфер БЕЗ лока
                MPI_Recv(received_buffer.data(), tasks_received, MPI_INT, victim, TAG_DATA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                {
                    // Быстро обновляем состояние под локом через std::move
                    std::lock_guard<std::mutex> lock(mtx);
                    tasks = std::move(received_buffer);
                    remaining = tasks_received;
                }
                return true;
            }
            // если не получили, то делаем запрос другому потоку
        }
        return false;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <iterations>\n";
        return 1;
    }
    // количество итераций (одинаковых прогонов)
    int iterations = std::stoi(argv[1]);
    int provided;
    
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided != MPI_THREAD_MULTIPLE) {
        std::cerr << "MPI_THREAD_MULTIPLE not supported\n";
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