#!/bin/bash
#PBS -l select=2:ncpus=8:mpiprocs=8
#PBS -l walltime=00:10:00
#PBS -N mpi_mpe_profiling

cd $PBS_O_WORKDIR

# Пути к MPE
export MPE_HOME=/mnt/storage/home/hpcusers/hpcuser51/mpe2_installed_nusc/mpe2-nusc-built
export PATH=$MPE_HOME/bin:$PATH

# Удаляем старые логи, чтобы они не мешали
rm -f *.clog2 *.slog2

# --- ПРАВИЛЬНАЯ РУЧНАЯ КОМПИЛЯЦИЯ С MPE ---
# Линкуем библиотеки -llmpe и -lmpe (именно они отвечают за создание лога)
mpicxx -O3 main.cpp -o main_mpe.out \
    -I$MPE_HOME/include -L$MPE_HOME/lib -llmpe -lmpe

if [ ! -f ./main_mpe.out ]; then
    echo "Ошибка: Программа не скомпилировалась с MPE!"
    exit 1
fi

MPI_NP=$(wc -l < $PBS_NODEFILE)
echo "Скомпилировано успешно. Запуск на $MPI_NP ядрах..."

# Запуск программы (создаст .clog2 файл)
mpirun -machinefile $PBS_NODEFILE -np $MPI_NP ./main_mpe.out 2400

# Конвертируем все найденные .clog2 в .slog2
clog2TOslog2 *.clog2

echo "Готово! Ищи файл .slog2 в папке."