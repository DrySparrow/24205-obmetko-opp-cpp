import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# ========== 1. Загрузка данных ==========
try:
    # Если файл называется 'in.txt' (как в вашем коде), и разделитель ';'
    df = pd.read_csv('in.txt', sep=';')
except FileNotFoundError:
    print("Ошибка: не найден ни 'in.txt', ни 'results.csv'.")
    exit()

required = ['NP', 'N', 'MinTime']
if not all(col in df.columns for col in required):
    print("Файл должен содержать колонки: NP, N, MinTime")
    exit()

# ========== 2. Проверка: есть ли данные для NP=1 (базовое время) ==========
base_df = df[df['NP'] == 1]
if base_df.empty:
    print("Ошибка: нет данных для NP=1. Ускорение вычислить невозможно.")
    exit()

# Создаём словарь: N -> время на 1 процессе
base_time = {row['N']: row['MinTime'] for _, row in base_df.iterrows()}

# ========== 3. Вычисляем ускорение ==========
results = []
for _, row in df.iterrows():
    np_val = row['NP']
    n_val = row['N']
    time_val = row['MinTime']
    if n_val in base_time and base_time[n_val] > 0:
        speedup = base_time[n_val] / time_val
    else:
        speedup = None  # нет базового времени или базовое время = 0
    results.append({
        'NP': np_val,
        'N': n_val,
        'Time': time_val,
        'Speedup': speedup
    })

results_df = pd.DataFrame(results)

# ========== 4. Вывод таблицы ==========
print("\n=== Таблица времени выполнения и ускорения ===\n")
print(results_df.to_string(index=False, float_format='%.4f'))

# ========== 5. Построение графика ускорения ==========
plt.figure(figsize=(12, 7))

# Группируем по NP
for np_val in sorted(results_df['NP'].unique()):
    subset = results_df[results_df['NP'] == np_val].sort_values('N')
    # Убираем строки, где speedup = None
    subset_valid = subset.dropna(subset=['Speedup'])
    if not subset_valid.empty:
        plt.plot(subset_valid['N'], subset_valid['Speedup'],
                 marker='o', linestyle='-', linewidth=2, markersize=8,
                 label=f'NP = {np_val}')

# Добавляем линию идеального ускорения (S = p)
max_np = results_df['NP'].max()
ideal_n = np.linspace(min(results_df['N']), max(results_df['N']), 100)
for p in sorted(results_df['NP'].unique()):
    if p != 1:
        plt.plot(ideal_n, [p]*len(ideal_n), 'k--', alpha=0.3, linewidth=1, label=f'Идеал S = {p}' if p == max_np else "")

# Оформление
plt.title('Ускорение параллельного умножения матриц', fontsize=14)
plt.xlabel('Размер матрицы N (N x N)', fontsize=12)
plt.ylabel('Ускорение (Speedup)', fontsize=12)

# Настройка осей (линейные, как вы просили ранее)
xticks_vals = [800, 1600, 2400, 3200, 4000, 4800]
plt.xticks(xticks_vals)
plt.grid(True, linestyle='--', alpha=0.6)
plt.legend(title='Количество потоков', fontsize=10)

# Подписи значений ускорения у точек (опционально)
for np_val in sorted(results_df['NP'].unique()):
    subset = results_df[results_df['NP'] == np_val].sort_values('N')
    subset_valid = subset.dropna(subset=['Speedup'])
    for _, row in subset_valid.iterrows():
        plt.annotate(f"{row['Speedup']:.2f}",
                     (row['N'], row['Speedup']),
                     textcoords="offset points", xytext=(5, 5),
                     ha='left', fontsize=8, alpha=0.7)

plt.tight_layout()
plt.show()
