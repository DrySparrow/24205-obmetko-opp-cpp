#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mpi.h>

const double X_START = -1.0, X_END = 1.0;
const double Y_START = -1.0, Y_END = 1.0;
const double Z_START = -1.0, Z_END = 1.0;
const double A = 1.0e5;
const double EPS = 1.0e-8;

// Размер сетки
const int NX = 512;
const int NY = 512;
const int NZ = 512;

double target_phi(double x, double y, double z) {
    return x * x + y * y + z * z;
}

double get_rho(double x, double y, double z) {
    return 6.0 - A * target_phi(x, y, z);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // Начало замера времени
    double start_time = MPI_Wtime();

    double hx = (X_END - X_START) / (NX - 1);
    double hy = (Y_END - Y_START) / (NY - 1);
    double hz = (Z_END - Z_START) / (NZ - 1);

    int local_nx = NX / size;
    int remainder = NX % size;
    int start_i = rank * local_nx + std::min(rank, remainder);
    if (rank < remainder) local_nx++;
    
    int total_local_rows = local_nx + 2;
    std::vector<double> phi(total_local_rows * NY * NZ, 0.0);
    std::vector<double> phi_new(total_local_rows * NY * NZ, 0.0);
    std::vector<double> rho(total_local_rows * NY * NZ);

    auto idx = [&](int i, int j, int k) { return i * NY * NZ + j * NZ + k; };

    for (int i = 0; i < total_local_rows; ++i) {
        int global_i = start_i + i - 1;
        double x = X_START + global_i * hx;
        for (int j = 0; j < NY; ++j) {
            double y = Y_START + j * hy;
            for (int k = 0; k < NZ; ++k) {
                double z = Z_START + k * hz;
                int curr_idx = idx(i, j, k);
                rho[curr_idx] = get_rho(x, y, z);
                if (global_i >= 0 && global_i < NX && (global_i == 0 || global_i == NX - 1 || j == 0 || j == NY - 1 || k == 0 || k == NZ - 1)) {
                    phi[curr_idx] = target_phi(x, y, z);
                    phi_new[curr_idx] = phi[curr_idx];
                }
            }
        }
    }

    double inv_h2 = 1.0 / (2.0 / (hx * hx) + 2.0 / (hy * hy) + 2.0 / (hz * hz) + A);
    double max_diff = 0;
    int iter = 0;

    do {
        iter++;
        max_diff = 0;
        MPI_Request requests[4] = {MPI_REQUEST_NULL, MPI_REQUEST_NULL, MPI_REQUEST_NULL, MPI_REQUEST_NULL};

        // 1. Обмены
        if (rank > 0) {
            MPI_Irecv(&phi[idx(0, 0, 0)], NY * NZ, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, &requests[0]);
            MPI_Isend(&phi[idx(1, 0, 0)], NY * NZ, MPI_DOUBLE, rank - 1, 1, MPI_COMM_WORLD, &requests[1]);
        }
        if (rank < size - 1) {
            MPI_Irecv(&phi[idx(local_nx + 1, 0, 0)], NY * NZ, MPI_DOUBLE, rank + 1, 1, MPI_COMM_WORLD, &requests[2]);
            MPI_Isend(&phi[idx(local_nx, 0, 0)], NY * NZ, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD, &requests[3]);
        }

        // 2. Внутренние точки
        for (int i = 2; i < local_nx; ++i) {
            for (int j = 1; j < NY - 1; ++j) {
                for (int k = 1; k < NZ - 1; ++k) {
                    double val = (phi[idx(i + 1, j, k)] + phi[idx(i - 1, j, k)]) / (hx * hx) +
                                 (phi[idx(i, j + 1, k)] + phi[idx(i, j - 1, k)]) / (hy * hy) +
                                 (phi[idx(i, j, k + 1)] + phi[idx(i, j, k - 1)]) / (hz * hz) -
                                 rho[idx(i, j, k)];
                    phi_new[idx(i, j, k)] = val * inv_h2;
                    max_diff = std::max(max_diff, std::abs(phi_new[idx(i, j, k)] - phi[idx(i, j, k)]));
                }
            }
        }

        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);

        // 3. Граничные слои
        int borders[] = {1, local_nx};
        for (int i : borders) {
            if (i < 1 || i > local_nx) continue;
            int global_i = start_i + i - 1;
            if (global_i <= 0 || global_i >= NX - 1) continue;

            for (int j = 1; j < NY - 1; ++j) {
                for (int k = 1; k < NZ - 1; ++k) {
                    double val = (phi[idx(i + 1, j, k)] + phi[idx(i - 1, j, k)]) / (hx * hx) +
                                 (phi[idx(i, j + 1, k)] + phi[idx(i, j - 1, k)]) / (hy * hy) +
                                 (phi[idx(i, j, k + 1)] + phi[idx(i, j, k - 1)]) / (hz * hz) -
                                 rho[idx(i, j, k)];
                    phi_new[idx(i, j, k)] = val * inv_h2;
                    max_diff = std::max(max_diff, std::abs(phi_new[idx(i, j, k)] - phi[idx(i, j, k)]));
                }
            }
        }

        phi.swap(phi_new);
        double global_max_diff;
        MPI_Allreduce(&max_diff, &global_max_diff, 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
        max_diff = global_max_diff;

    } while (max_diff > EPS && iter < 10000);

    // Конец замера времени
    double end_time = MPI_Wtime();

    if (rank == 0) {
        // Вывод только числа
        std::cout << (end_time - start_time) << std::endl;
    }

    MPI_Finalize();
    return 0;
}