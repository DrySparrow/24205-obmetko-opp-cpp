import matplotlib.pyplot as plt
import numpy as np

def show_speedup(file_path):
    cores = []
    times = []
    
    # Чтение данных
    with open(file_path, 'r') as f:
        for line in f:
            parts = line.split()
            if len(parts) >= 2:
                cores.append(int(parts[0]))
                times.append(float(parts[1]))

    p = np.array(cores)
    t = np.array(times)
    
    # Расчет ускорения S = T1 / Tp
    speedup = t[0] / t

    # Построение графика
    plt.figure(figsize=(10, 6))
    plt.plot(p, speedup, 'o-', label='Реальное ускорение $S(p)$')
    plt.plot(p, p, '--', color='red', label='Идеальное ускорение')
    
    plt.title('График ускорения (Speedup)')
    plt.xlabel('Число ядер (p)')
    plt.ylabel('Ускорение S')
    plt.xticks(p)
    plt.grid(True)
    plt.legend()
    
    # Вывод окна на экран
    plt.show()

if __name__ == "__main__":
    show_speedup('in.txt')
