#!/bin/bash
#PBS -l select=2:ncpus=8:mpiprocs=8:mem=2gb
#PBS -l walltime=00:30:00
#PBS -N Jacobi

cd $PBS_O_WORKDIR

# Компиляция с максимальной оптимизацией
mpicxx -O3 main.cpp -o main.out

# Список ядер для тестов
CORES_LIST="1 2 4 8 16"

echo "Running 3D Jacobi Test"
echo "Cores | Best Time (sec)"
echo "-----------------------"

for cores in $CORES_LIST
do
    min_time=99999.0
    
    # Делаем 3 замера для стабильности
    for i in {1..5}
    do
        # Запускаем программу. В переменную попадет только число.
        # 2>/dev/null подавляет возможные системные предупреждения MPI
        current_run=$(mpirun -np $cores ./main.out 2>/dev/null)
        
        # Проверка: если вывод пустой (программа упала), пропускаем
        if [ -z "$current_run" ]; then continue; fi

        # Сравнение дробных чисел через bc
        is_less=$(echo "$current_run < $min_time" | bc -l)
        if [ "$is_less" -eq 1 ]; then
            min_time=$current_run
        fi
    done

    # Вывод итоговой строки для этого количества ядер
    echo "$cores : $min_time"
done
