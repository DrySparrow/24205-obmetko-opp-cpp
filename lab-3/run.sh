#!/bin/bash
#PBS -l select=2:ncpus=8:mpiprocs=8
#PBS -l walltime=00:30:00
#PBS -N mpi_bench

cd $PBS_O_WORKDIR

# Компиляция
mpicxx -O3 main.cpp -o main.out

# Число ядер (всегда 16 для этого теста) [cite: 41]
MPI_NP=16

# Размеры матриц для исследования [cite: 40]
# Важно: N должно делиться на 4 (размер решетки)
SIZES=(8000)  	 

echo "N;MinTime"

for N in "${SIZES[@]}"
do
    MIN_T=9999.9
    # прогоняем по 6 раз для каждого N	
    for i in {1..1}
    do
        # Запуск и получение времени (берем вывод 0-го процесса)
        T=$(mpirun -machinefile $PBS_NODEFILE -np $MPI_NP ./main.out $N)
        
        # Сравнение для поиска минимума
        if (( $(echo "$T < $MIN_T" | bc -l) )); then
            MIN_T=$T
        fi
    done
    echo "$N;$MIN_T"
done