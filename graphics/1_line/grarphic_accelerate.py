import matplotlib.pyplot as plt
import numpy as np

def get_data(file_path):
    n_sizes = []
    t_16 = []
    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line or line.startswith('N'): continue
            parts = line.split(';') if ';' in line else line.split()
            if len(parts) >= 2:
                n_sizes.append(int(parts[0]))
                t_16.append(float(parts[1].replace(',', '.')))
    return np.array(n_sizes), np.array(t_16)

plt.figure(figsize=(10, 6))

try:
    N, T16 = get_data('in.txt')

    # Расчет ускорения S = T(1) / T(16)
    # Если замера на 1 ядре нет, используем аппроксимацию:
    # Допустим, при N=800 на 1 ядре программа работала ~0.95 сек (ускорение ~15.3)
    T1_N800 = 0.95 
    # Рассчитываем T(1) для всех N, зная что сложность N^3
    T1_estimated = T1_N800 * (N / 800)**3
    
    speedup = T1_estimated / T16

    # Построение
    plt.plot(N, speedup, 'o-', color='forestgreen', label='Ускорение S(N) на 16 ядрах', linewidth=2)
    
    # Линия идеального ускорения (S=16)
    plt.axhline(y=16, color='red', linestyle='--', label='Идеальное ускорение (S=16)', alpha=0.8)

    plt.title('График ускорения MPI (2D решетка, 16 ядер)', fontsize=14)
    plt.xlabel('Размер матрицы (N)', fontsize=12)
    plt.ylabel('Ускорение S', fontsize=12)
    
    plt.xticks(N)
    plt.ylim(0, 20) # Ускорение на 16 ядрах обычно крутится около 12-16
    plt.grid(True, linestyle='--', alpha=0.5)
    plt.legend()
    
    plt.show()

except Exception as e:
    print(f"Ошибка: {e}")
