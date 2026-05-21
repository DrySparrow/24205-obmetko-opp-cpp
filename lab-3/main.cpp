#include <iostream>
#include <vector>
#include <mpi.h>
#include <algorithm>
#include <cstdlib>

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

    // --- ДИНАМИЧЕСКОЕ ВЫЧИСЛЕНИЕ РАЗМЕРОВ РЕШЕТКИ ---
    int dims[2] = {0, 0}; // Инициализируем нулями
    // MPI_Dims_create подберет p1 и p2 так, чтобы p1 * p2 == size
    MPI_Dims_create(size, 2, dims);
    int p1 = dims[0];
    int p2 = dims[1];

    // Проверка: матрица должна делиться на блоки без остатка
    if (N % p1 != 0 || N % p2 != 0) {
        if (rank == 0) {
            std::cerr << "Error: Matrix size N=" << N 
                      << " is not perfectly divisible by grid " 
                      << p1 << "x" << p2 << "\n";
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    int periods[2] = {0, 0}, coords[2];
    MPI_Comm grid_comm;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &grid_comm);
    MPI_Cart_coords(grid_comm, rank, 2, coords);

    MPI_Comm row_comm, col_comm;
    MPI_Comm_split(grid_comm, coords[0], coords[1], &row_comm);
    MPI_Comm_split(grid_comm, coords[1], coords[0], &col_comm);

    int local_n1 = n1 / p1;
    int local_n3 = n3 / p2;

    std::vector<double> local_A(local_n1 * n2), local_B(n2 * local_n3), local_C(local_n1 * local_n3, 0.0);
    std::vector<double> A, B;

    if (rank == 0) {
        A.assign(n1 * n2, 1.0); B.assign(n2 * n3, 2.0);
    }

		std::vector<double> C;
    if (rank == 0) C.resize(n1 * n3, 0.0); 

    // --- НАЧАЛО ЗАМЕРА ВРЕМЕНИ ---
    // Синхронизируем процессы перед стартом
    MPI_Barrier(MPI_COMM_WORLD);
    double startTime = MPI_Wtime();
		
    // Распределение данных
    if (coords[1] == 0) 
        MPI_Scatter(A.data(), local_n1 * n2, MPI_DOUBLE, local_A.data(), local_n1 * n2, MPI_DOUBLE, 0, col_comm);
    
    std::vector<double> B_packed;
		if (rank == 0) {
			B_packed.resize(n2 * n3);
			for (int p = 0; p < p2; ++p) { // Идем по номеру будущей части (процесса в строке)
				for (int i = 0; i < n2; ++i) { // По строкам
					for (int j = 0; j < local_n3; ++j) { // По столбцам внутри полосы
							// Копируем из оригинальной B в линейный буфер для Scatter
							B_packed[p * (n2 * local_n3) + i * local_n3 + j] = B[i * n3 + (p * local_n3 + j)];
					}
				}
			}
		}
		
		if (coords[0] == 0) {
			MPI_Scatter(
					B_packed.data(), n2 * local_n3, MPI_DOUBLE, // Что и сколько посылаем (только на rank 0)
					local_B.data(),  n2 * local_n3, MPI_DOUBLE, // Куда принимаем (на всех в строке)
					0, row_comm                                 // Корень и коммуникатор строки
			);
		}
		
		MPI_Bcast(local_B.data(), n2 * local_n3, MPI_DOUBLE, 0, col_comm);
    MPI_Bcast(local_A.data(), local_n1 * n2, MPI_DOUBLE, 0, row_comm);
    
    localMultiply(local_A, local_B, local_C, local_n1, n2, local_n3);
		
		// --- 1. Сбор всех подматриц в один линейный буфер ---
		std::vector<double> C_packed;
		if (rank == 0) {
			C_packed.resize(n1 * n3); // размер всей матрицы
		}

		// Собираем данные со всех процессов в MPI_COMM_WORLD
		MPI_Gather(
			local_C.data(), local_n1 * local_n3, MPI_DOUBLE, // Что посылаем
			C_packed.data(), local_n1 * local_n3, MPI_DOUBLE, // Где собираем (только на rank 0)
			0, MPI_COMM_WORLD                                // Корень и общий коммуникатор
		);

		// --- 2. Распаковка (только на 0 процессе) ---
		if (rank == 0) {
			// В C_packed блоки лежат в порядке рангов: 0, 1, 2...
			// Нам нужно переложить их в матрицу C, учитывая 2D координаты
			for (int k = 0; k < size; ++k) {
				int r = k / p2; // Индекс строки в решетке процессоров
				int c = k % p2; // Индекс столбца в решетке процессоров
				
				for (int i = 0; i < local_n1; ++i) {
					for (int j = 0; j < local_n3; ++j) {
						// Глобальные индексы в итоговой матрице C
						int global_i = r * local_n1 + i;
						int global_j = c * local_n3 + j;
							
					// Копируем из линейного буфера в итоговую структуру
					C[global_i * n3 + global_j] = C_packed[k * (local_n1 * local_n3) + i * local_n3 + j];
					}
				}
			}
		}

    // Обязательная синхронизация перед остановкой таймера!
    MPI_Barrier(MPI_COMM_WORLD); 
    double endTime = MPI_Wtime();

    if (rank == 0) {
        // Выводим только число для скрипта
        std::cout << endTime - startTime << std::endl;
    }
		
		// --- ПРОВЕРКА РЕЗУЛЬТАТА ---
    if (rank == 0) {
        double expected = (double)n2 * 2.0;
        std::cout << "C[0] - " << expected << " = " << std::fixed << std::setprecision(10) << (std::abs(C[0] - expected));
    }

    // Очистка коммуникаторов
    MPI_Comm_free(&row_comm);
    MPI_Comm_free(&col_comm);
    MPI_Comm_free(&grid_comm);

    MPI_Finalize();
    return 0;
}