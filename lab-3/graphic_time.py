import matplotlib.pyplot as plt
import pandas as pd

# 1. Загружаем данные из файла
#    Предполагается, что файл 'results.csv' лежит в той же папке,
#    и разделитель — точка с запятой ';'
try:
    df = pd.read_csv('in.txt', sep=';')
except FileNotFoundError:
    print("Ошибка: файл 'results.csv' не найден.")
    exit()

# 2. Проверим, что нужные колонки есть
required = ['NP', 'N', 'MinTime']
if not all(col in df.columns for col in required):
    print("Файл должен содержать колонки: NP, N, MinTime")
    exit()

# 3. Настройка графика
plt.figure(figsize=(12, 7))

# 4. Группируем по NP и рисуем отдельную линию для каждого NP
for np_val in sorted(df['NP'].unique()):
    subset = df[df['NP'] == np_val]
    # Сортируем по N, чтобы линия шла правильно
    subset = subset.sort_values('N')
    plt.plot(subset['N'], subset['MinTime'], 
             marker='o', linestyle='-', linewidth=2, markersize=8,
             label=f'NP = {np_val}')

# 5. Оформление
plt.title('Зависимость времени умножения матриц от размера и числа потоков', fontsize=14)
plt.xlabel('Размер матрицы N (N x N)', fontsize=12)
plt.ylabel('Минимальное время выполнения (сек)', fontsize=12)



# Сетка
plt.grid(True, linestyle='--', alpha=0.6)

# Логарифмическая шкала по обеим осям (часто полезна для таких данных)
# Если хотите обычную линейную шкалу — закомментируйте следующие две строки
plt.xscale('log')
plt.yscale('log')

# Легенда
plt.legend(title='Количество потоков', fontsize=10)

# Подписи значений у точек (опционально)
for np_val in sorted(df['NP'].unique()):
    subset = df[df['NP'] == np_val].sort_values('N')
    for _, row in subset.iterrows():
        plt.annotate(f"{row['MinTime']:.3f}", 
                     (row['N'], row['MinTime']),
                     textcoords="offset points", xytext=(5, 5), 
                     ha='left', fontsize=8, alpha=0.7)

plt.tight_layout()
plt.show()
