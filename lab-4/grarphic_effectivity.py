import matplotlib.pyplot as plt
import numpy as np

def get_efficiency_data(file_path):
    cores = []
    times = []
    with open(file_path, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 2:
                cores.append(int(parts[0]))
                times.append(float(parts[1]))
    
    p = np.array(cores)
    t = np.array(times)
    
    # S = T1 / Tp
    speedup = t[0] / t
    # E = S / p
    efficiency = speedup / p
    return p, efficiency

p, e = get_efficiency_data('in.txt')

plt.figure(figsize=(10, 10))
plt.plot(p, e, 'o-', color='blue', label='Эффективность E(p)')
plt.axhline(y=1.0, color='red', linestyle='--', label='Идеальная эффективность')

plt.title('График эффективности распараллеливания')
plt.xlabel('Число ядер (p)')
plt.ylabel('Эффективность E')
plt.xticks(p)
plt.grid(True)
plt.legend()

# Устанавливаем нижнюю границу оси Y в 0
plt.ylim(bottom=0)

plt.show()
