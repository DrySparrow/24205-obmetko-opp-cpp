#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <omp.h>

void init_system(int n, std::vector<double>& A, std::vector<double>& b) {
    std::vector<double> exact(n);
    for (int i = 0; i < n; ++i) {
        exact[i] = std::sin(2.0 * M_PI * i / n);
        for (int j = 0; j < n; ++j) {
            A[i * n + j] = (i == j) ? 2.0 : 1.0;
        }
    }
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        for (int j = 0; j < n; ++j) sum += A[i * n + j] * exact[j];
        b[i] = sum;
    }
}

// Вариант 1: Отдельные секции parallel for
void solve_v1(int n, const std::vector<double>& A, const std::vector<double>& b, std::vector<double>& x, double tau, double eps) {
    double b_norm_sq = 0;
    #pragma omp parallel for reduction(+:b_norm_sq)
    for (int i = 0; i < n; i++) b_norm_sq += b[i] * b[i];
    double b_norm = std::sqrt(b_norm_sq);

    double rel_res = 1.0;
    std::vector<double> x_new(n);

    while (rel_res >= eps) {
        double res_sq = 0;
        #pragma omp parallel for reduction(+:res_sq)
        for (int i = 0; i < n; i++) {
            double Ax = 0;
            for (int j = 0; j < n; j++) Ax += A[i * n + j] * x[j];
            double r = Ax - b[i];
            x_new[i] = x[i] - tau * r;
            res_sq += r * r;
        }
        rel_res = std::sqrt(res_sq) / b_norm;
        std::swap(x, x_new);
    }
}

// Вариант 2: Одна параллельная секция на весь алгоритм
void solve_v2(int n, const std::vector<double>& A, const std::vector<double>& b, std::vector<double>& x, double tau, double eps) {
    double b_norm_sq = 0;
    double current_res_sq = 0;
    double rel_res = 1.0;
    std::vector<double> x_new(n);

    #pragma omp parallel
    {
        // Считаем норму b один раз
        #pragma omp for schedule(runtime) reduction(+:b_norm_sq)
        for (int i = 0; i < n; i++) b_norm_sq += b[i] * b[i];

        while (rel_res >= eps) {
            // Обнуляем общую сумму невязки перед циклом
            #pragma omp single
            current_res_sq = 0;

            // Считаем итерацию и локальную сумму квадратов невязки
            #pragma omp for schedule(runtime) reduction(+:current_res_sq)
            for (int i = 0; i < n; i++) {
                double Ax = 0;
                for (int j = 0; j < n; j++) Ax += A[i * n + j] * x[j];
                double r = Ax - b[i];
                x_new[i] = x[i] - tau * r;
                current_res_sq += r * r;
            }

            // Обновляем параметры выхода только в одном потоке
            #pragma omp single
            {
                rel_res = std::sqrt(current_res_sq) / std::sqrt(b_norm_sq);
                std::swap(x, x_new);
            }
            // Неявный барьер в конце single синхронизирует все потоки для следующей итерации
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    int n = std::atoi(argv[1]);
    int mode = std::atoi(argv[2]);
    double eps = 1e-5, tau = 2.0 / (n + 2.0);

    std::vector<double> A(n * n), b(n), x(n, 0.0);
    init_system(n, A, b);

    double start = omp_get_wtime();
    if (mode == 0) solve_v1(n, A, b, x, tau, eps);
    else solve_v2(n, A, b, x, tau, eps);
    double end = omp_get_wtime();

    std::cout << "Time: " << end - start << " s. x[0] = " << x[0] << std::endl;

    return 0;
}