#!/bin/bash
#PBS -l select=2:ncpus=8:mpiprocs=8:mem=4gb
#PBS -l walltime=02:00:00
#PBS -N mpi_bench_full
#PBS -j oe

cd $PBS_O_WORKDIR

# Компиляция
mpicxx -O3 main.cpp -o main.out

# Количество процессов – только те, для которых есть данные
PROCS=(1 2 4 8)
# Размеры матриц – все
SIZES=(800 1600 2400 3200 4000 4800)

echo "NP;N;MinTime"

for NP in "${PROCS[@]}"; do
    for N in "${SIZES[@]}"; do
        MIN_T=9999.9
        # Делаем 5 прогонов
        for i in {1..5}; do
            T=$(mpirun -machinefile $PBS_NODEFILE -np $NP ./main.out $N 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -n 1)
            if [ -z "$T" ]; then
                echo "Ошибка: NP=$NP N=$N прогон $i не дал результата" >&2
                continue
            fi
            if (( $(echo "$T < $MIN_T" | bc -l) )); then
                MIN_T=$T
            fi
        done
        if [ "$MIN_T" = "9999.9" ]; then
            echo "Ошибка: для NP=$NP N=$N нет успешных прогонов" >&2
            continue
        fi
        echo "$NP;$N;$MIN_T"
    done
done

