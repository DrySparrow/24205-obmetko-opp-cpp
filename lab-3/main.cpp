#include <iostream>
#include <vector>
#include <mpi.h>
#include <algorithm>

void localMultiply(const std::vector<double>& A, const std::vector<double>& B, 
                   std::vector<double>& C, int rowsA, int common, int colsB) {
    for (int i = 0; i < rowsA; ++i) {
        for (int k = 0; k < common; ++k) {
						for (int j = 0; j < colsB; ++j) {
                C[i * colsB + j] += A[i * common + k] * B[k * colsB + j];
            }
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Читаем размер N из аргументов
    if (argc < 2) {
        if (rank == 0) std::cerr << "Usage: " << argv[0] << " <N>\n";
        MPI_Finalize(); return 1;
    }
    int N = std::atoi(argv[1]);
    const int n1 = N, n2 = N, n3 = N;
    const int p1 = 4, p2 = 4; 

    if (p1 * p2 != size) {
        if (rank == 0) std::cerr << "Error: Grid 4x4 requires 16 processes\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int dims[2] = {p1, p2}, periods[2] = {0, 0}, coords[2];
    MPI_Comm grid_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &grid_comm);
    MPI_Cart_coords(grid_comm, rank, 2, coords);

    MPI_Comm row_comm, col_comm;
    MPI_Comm_split(grid_comm, coords[0], coords[1], &row_comm);
    MPI_Comm_split(grid_comm, coords[1], coords[0], &col_comm);

    int local_n1 = n1 / p1;
    int local_n3 = n3 / p2;

    std::vector<double> local_A(local_n1 * n2), local_B(n2 * local_n3), local_C(local_n1 * local_n3);
    std::vector<double> A, B;

    if (rank == 0) {
        A.assign(n1 * n2, 1.0); B.assign(n2 * n3, 2.0);
    }

    // Распределение данных
    if (coords[1] == 0) 
        MPI_Scatter(A.data(), local_n1 * n2, MPI_DOUBLE, local_A.data(), local_n1 * n2, MPI_DOUBLE, 0, col_comm);
    
    if (coords[0] == 0) {
        if (rank == 0) {
            for (int i = 0; i < p2; ++i) {
                std::vector<double> tB(n2 * local_n3);
                for (int r = 0; r < n2; ++r)
                    for (int c = 0; c < local_n3; ++c) tB[r * local_n3 + c] = B[r * n3 + (i * local_n3 + c)];
                if (i == 0) local_B = tB;
                else {
                    int tr; int tc[2] = {0, i}; MPI_Cart_rank(grid_comm, tc, &tr);
                    MPI_Send(tB.data(), n2 * local_n3, MPI_DOUBLE, tr, 1, grid_comm);
                }
            }
        } else MPI_Recv(local_B.data(), n2 * local_n3, MPI_DOUBLE, 0, 1, grid_comm, MPI_STATUS_IGNORE);
    }

    MPI_Bcast(local_A.data(), local_n1 * n2, MPI_DOUBLE, 0, row_comm);
    MPI_Bcast(local_B.data(), n2 * local_n3, MPI_DOUBLE, 0, col_comm);

    // Замер времени только самого вычисления
    double startTime = MPI_Wtime();
    localMultiply(local_A, local_B, local_C, local_n1, n2, local_n3);
    double endTime = MPI_Wtime();

    if (rank == 0) {
        // Выводим только число, чтобы удобно было обрабатывать скриптом
        std::cout << endTime - startTime << std::endl;
    }

    MPI_Finalize();
    return 0;
}