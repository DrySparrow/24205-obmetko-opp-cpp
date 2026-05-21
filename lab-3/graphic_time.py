import matplotlib.pyplot as plt
import pandas as pd
import matplotlib.ticker as ticker
import io

# Чтение данных (используем io для примера, замените на pd.read_csv('in.txt', sep=';'))
df = pd.read_csv("in.txt", sep=';')

# Подготовка индексов для оси X
# Получаем уникальные N, чтобы сопоставить их с индексами 0, 1, 2...
unique_n = sorted(df['N'].unique())
n_to_index = {val: i for i, val in enumerate(unique_n)}

plt.figure(figsize=(12, 7))

# Рисуем графики
for np_val in sorted(df['NP'].unique()):
    subset = df[df['NP'] == np_val].sort_values('N')
    
    # Преобразуем значения N в их индексы (0, 1, 2...)
    indices = [n_to_index[x] for x in subset['N']]
    
    plt.plot(indices, subset['MinTime'], 
             marker='o', linestyle='-', linewidth=2, markersize=8,
             label=f'NP = {np_val}')

    # Аннотации (подписи значений)
    for i, row in subset.iterrows():
        plt.annotate(f"{row['MinTime']:.2f}", 
                     (n_to_index[row['N']], row['MinTime']),
                     textcoords="offset points", xytext=(0, 10), 
                     ha='center', fontsize=9)

# --- Настройка осей ---

# Ось X: устанавливаем индексы и подписываем их соответствующими значениями N
plt.xticks(range(len(unique_n)), unique_n)

# Ось Y: интервал 5 секунд
plt.gca().yaxis.set_major_locator(ticker.MultipleLocator(5))
plt.ylabel('Минимальное время выполнения (сек)', fontsize=12)

plt.title('Зависимость времени от N', fontsize=14)
plt.grid(True, linestyle='--', alpha=0.7)
plt.legend(title='Потоки (NP)')

plt.tight_layout()
plt.show()
