#include <mpi.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

// последовательное умножение матрицы на вектор
std::vector<double> matrix_vector_mult(const std::vector<double>& matrix,
                                       const std::vector<double>& vec,
                                       int cols, int rows) {
    std::vector<double> result(rows, 0.0);
    for (int i = 0; i < rows; ++i) {
        double acc = 0.0;
        for (int j = 0; j < cols; ++j) {
            acc += matrix[i * cols + j] * vec[j];
        }
        result[i] = acc;
    }
    return result;
}

void init_system(std::vector<double>& matrix, std::vector<double>& rhs, int n) {
    std::vector<double> exact(n);
    for (int i = 0; i < n; ++i) {
        exact[i] = std::sin(2 * M_PI * i / n);
        for (int j = 0; j < n; ++j) {
            matrix[i * n + j] = (i == j) ? 2.0 : 1.0;
        }
    }
    rhs = matrix_vector_mult(matrix, exact, n, n);
}

double squared_norm(const std::vector<double>& v) {
    return std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
}

double vector_norm(const std::vector<double>& v) {
    return std::sqrt(squared_norm(v));
}

// Вариант 1: с дублированием
std::vector<double> solve_dup(double eps, double tau, int n,
                              int rank, int size,
                              const std::vector<int>& counts,
                              const std::vector<int>& offsets,
                              const std::vector<double>& local_A,
                              const std::vector<double>& global_b) {
    std::vector<double> x(n, 0.0);
    std::vector<double> x_new(n);
    double b_norm = vector_norm(global_b);
    double rel_res = 1.0;
    while (rel_res >= eps) {
        double local_res_sq = 0;
        for (int i = 0; i < counts[rank]; ++i) {
            double Ax = 0;
            int row = offsets[rank] + i;
            for (int j = 0; j < n; ++j) {
                Ax += local_A[i * n + j] * x[j];
            }
            double res = Ax - global_b[row];
            x[row] -= tau * res;
            local_res_sq += res * res;
        }
        double global_res_sq;
        MPI_Allreduce(&local_res_sq, &global_res_sq, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        rel_res = std::sqrt(global_res_sq) / b_norm;
        MPI_Allgatherv(&x[offsets[rank]], counts[rank], MPI_DOUBLE,
                       x_new.data(), const_cast<int*>(counts.data()), const_cast<int*>(offsets.data()),
                       MPI_DOUBLE, MPI_COMM_WORLD);
        x.swap(x_new);
    }
    return x;
}

// Вариант 2: с разрезанием
std::vector<double> solve_dist(double eps, double tau, int n,
                               int rank, int size,
                               const std::vector<int>& counts,
                               const std::vector<int>& offsets,
                               const std::vector<double>& local_A,
                               const std::vector<double>& local_b) {
    std::vector<double> x_local(counts[rank], 0.0);
    std::vector<double> Ax_local(counts[rank], 0.0);
    double global_b_sq;
    double local_b_sq = squared_norm(local_b);
    MPI_Allreduce(&local_b_sq, &global_b_sq, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    double b_norm = std::sqrt(global_b_sq);
    if (b_norm < 1e-15) b_norm = 1.0;
    double rel_res = 1.0;
    int next = (rank + 1) % size;
    int prev = (rank - 1 + size) % size;
    int max_cnt = *std::max_element(counts.begin(), counts.end());
    std::vector<double> x_block(max_cnt, 0.0);
    std::vector<double> x_block_tmp(max_cnt, 0.0);
    while (rel_res >= eps) {
        std::fill(Ax_local.begin(), Ax_local.end(), 0.0);
        for (int i = 0; i < counts[rank]; ++i) {
            x_block[i] = x_local[i];
        }
        for (int step = 0; step < size; ++step) {
            int owner = (rank - step + size) % size;
            int owner_cnt = counts[owner];
            int owner_off = offsets[owner];
            for (int i = 0; i < counts[rank]; ++i) {
                int row_idx = i * n + owner_off;
                double sum = 0.0;
                for (int j = 0; j < owner_cnt; ++j) {
                    sum += local_A[row_idx + j] * x_block[j];
                }
                Ax_local[i] += sum;
            }
            if (size > 1 && step < size - 1) {
                MPI_Sendrecv(x_block.data(), owner_cnt, MPI_DOUBLE, next, 0,
                             x_block_tmp.data(), counts[(owner - 1 + size) % size], MPI_DOUBLE,
                             prev, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                x_block.swap(x_block_tmp);
            }
        }
        double local_res_sq = 0.0;
        for (int i = 0; i < counts[rank]; ++i) {
            double res = Ax_local[i] - local_b[i];
            x_local[i] -= tau * res;
            local_res_sq += res * res;
        }
        double global_res_sq = 0.0;
        MPI_Allreduce(&local_res_sq, &global_res_sq, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        rel_res = std::sqrt(global_res_sq) / b_norm;
    }
    std::vector<double> x_global(n, 0.0);
    MPI_Allgatherv(x_local.data(), counts[rank], MPI_DOUBLE,
                   x_global.data(), const_cast<int*>(counts.data()), const_cast<int*>(offsets.data()),
                   MPI_DOUBLE, MPI_COMM_WORLD);
    return x_global;
}

void print_vector(const std::vector<double>& v) {
    for (auto val : v) {
        std::cout << val << std::endl;
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if (argc < 3) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0]
                      << " <matrix_size> <mode> \n"
                      << "  mode 0: duplicated vectors\n"
                      << "  mode 1: distributed vectors" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }
    int n = std::atoi(argv[1]);
    int mode = std::atoi(argv[2]);
    double eps = 1e-5;
    double tau = 2.0 / (n + 2.0);
    std::vector<int> counts(size), offsets(size);
    int base = n / size;
    int rem = n % size;
    int pos = 0;
    for (int p = 0; p < size; ++p) {
        counts[p] = base + (p < rem ? 1 : 0);
        offsets[p] = pos;
        pos += counts[p];
    }
    std::vector<double> local_A(counts[rank] * n, 1.0);
    double local_sum = 0.0;
    for (int i = 0; i < counts[rank]; ++i) {
        int global_idx = offsets[rank] + i;
        local_A[i * n + global_idx] += 1.0;
        local_sum += std::sin(2.0 * M_PI * global_idx / n);
    }
    double global_sum = 0.0;
    MPI_Allreduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    std::vector<double> local_b(counts[rank], 0.0);
    for (int i = 0; i < counts[rank]; ++i) {
        int global_idx = offsets[rank] + i;
        local_b[i] = global_sum + std::sin(2.0 * M_PI * global_idx / n);
    }
    std::vector<double> global_b;
    if (mode == 0) {
        global_b.resize(n);
        for (int k = 0; k < n; ++k) {
            global_b[k] = global_sum + std::sin(2.0 * M_PI * k / n);
        }
        MPI_Bcast(global_b.data(), n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();
    std::vector<double> x;
    if (mode == 0) {
        x = solve_dup(eps, tau, n, rank, size, counts, offsets, local_A, global_b);
    } else {
        x = solve_dist(eps, tau, n, rank, size, counts, offsets, local_A, local_b);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();
    if (rank == 0) {
        std::cout << "Time: " << end_time - start_time << " seconds" << std::endl;
        std::cout << "First 5 components:" << std::endl;
        for (int i = 0; i < std::min(5, n); ++i) {
            std::cout << "x[" << i << "] = " << x[i] << std::endl;
        }
        std::cout << "\n----- Checking solution accuracy -----" << std::endl;
        std::vector<double> exact(n);
        for (int i = 0; i < n; ++i) {
            exact[i] = std::sin(2 * M_PI * i / n);
        }
        double error_sq = 0.0;
        double max_error = 0.0;
        int max_error_idx = 0;
        for (int i = 0; i < n; ++i) {
            double diff = x[i] - exact[i];
            error_sq += diff * diff;
            if (std::abs(diff) > max_error) {
                max_error = std::abs(diff);
                max_error_idx = i;
            }
        }
        double error = std::sqrt(error_sq);
        double exact_norm = vector_norm(exact);
        double relative_error = error / exact_norm;
        std::cout << "Absolute error norm: " << error << std::endl;
        std::cout << "Relative error norm: " << relative_error << std::endl;
        std::cout << "Max error: " << max_error << " at index " << max_error_idx << std::endl;
        if (relative_error < eps) {
            std::cout << "✓ Solution meets required tolerance (eps = " << eps << ")" << std::endl;
        }
    }
    MPI_Finalize();
    return 0;
}