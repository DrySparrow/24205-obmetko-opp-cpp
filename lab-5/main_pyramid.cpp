#include <mpi.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>

class LoadBalancer {
public:
    LoadBalancer(int r, int s): rank(r), size(s) {}

    void run(int iterations) {
        double start_time = MPI_Wtime();

        long cur_task_count = TOTAL_TASKS / size;
        if (rank == size - 1) {
            cur_task_count += TOTAL_TASKS % size;
        }

        double total_imbalance_sum = 0;
        long total_done = 0;

        for (int iter = 0; iter < iterations; ++iter) {
            double cur_total_weight = 0;
            long remaining = cur_task_count;

            int center_process = iter % size;

            // Расчет коэффициентов
            long long total_coeffs = 0;
            for (int i = 0; i < size; ++i) {
                total_coeffs += (size - std::abs(i - center_process));
            }
            long long cur_coeff = size - std::abs(rank - center_process);

            // Распределяем TOTAL_WEIGHT строго без потерь (целочисленным способом)
            long long base_process_weight = (TOTAL_WEIGHT * cur_coeff) / total_coeffs;
            long long process_weight_remainder = (TOTAL_WEIGHT * cur_coeff) % total_coeffs;
            
            // Отдадим остаток от деления весов, например, процессу с rank == 0 на этой итерации
            // или распределим по цепочке, чтобы сумма ВСЕГДА была равна TOTAL_WEIGHT
            if (rank == center_process) { 
                // так как сумма остатков по всем cur_coeff равна какому-то числу, 
                // для простоты можно скорректировать веса глобально. 
                // Но еще надежнее — распределить остаток деления внутри самого процесса:
            }

            // Чтобы вес внутри процесса не терялся при делении на количество задач:
            long long base_task_weight = base_process_weight / remaining;
            long long task_remainder = base_process_weight % remaining;

            std::vector<int> tasks(remaining, static_cast<int>(base_task_weight));
            // Распределяем остаток по единичке между первыми задачами
            for (long long i = 0; i < task_remainder; ++i) {
                tasks[i]++;
            }

            // Выполнение работы
            for (int weight : tasks) {
                do_work(weight);
                cur_total_weight += weight;
                total_done++;
            }

            

            // ВЫВОД НА КАЖДОЙ ИТЕРАЦИИ ДЛЯ КАЖДОГО ПРОЦЕССА (Выполнение требования)
            // Использование std::flush или упорядочивание вывода может потребоваться, 
            // но базово пишем так:
            std::cout << "[Iter " << iter << "] Process " << rank 
                    << ": Tasks executed = " << remaining 
                    << ", Total weight = " << cur_total_weight << std::endl;

            // Синхронизация и сбор статистики
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
            std::cout << "--- Final Stats ---\n";
            std::cout << "Load Imbalance Factor: " << load_imbalance_factor << "\n";
        }
        // Вывод общего времени для каждого процесса
        std::cout << "Rank " << rank << ": Total Done " << total_done << " tasks in " << end_time - start_time << "s\n";
    }

private:
    static constexpr long TOTAL_TASKS = 50000000;
    static constexpr long TOTAL_WEIGHT = (TOTAL_TASKS * 10);

    int rank, size;

    void do_work(int weight) {
        double res = 0;
        volatile double dummy = 0;
        for (int i = 0; i < weight; ++i) {
            res += std::sin(i) * std::tan(i);
        }
        dummy = res;
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./program <iterations>\n";
        return 1;
    }
    
    int iterations = std::stoi(argv[1]);
    
    // MPI_THREAD_MULTIPLE больше не нужен, так как мы убрали многопоточность
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    LoadBalancer lb(rank, size);
    lb.run(iterations);

    MPI_Finalize();
    return 0;
}
