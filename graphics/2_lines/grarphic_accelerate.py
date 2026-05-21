import matplotlib.pyplot as plt

def get_speedup_data(file_path):
    """Читает файл и возвращает (n_sorted, speedup)"""
    n_values = []
    time_values = []

    with open(file_path, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 2:
                n_values.append(int(parts[0]))
                time_values.append(float(parts[1].replace(',', '.')))

    # Сортировка данных по числу ядер
    data = sorted(zip(n_values, time_values))
    n_sorted, time_sorted = zip(*data)

    # Вычисляем ускорение: S = T(1) / T(n)
    t1 = time_sorted[0]
    speedup = [t1 / tn for tn in time_sorted]
    
    return n_sorted, speedup

# --- Основной блок построения ---

plt.figure(figsize=(10, 6))

try:
    # Данные из первого файла
    n0, s0 = get_speedup_data('in0.txt')
    plt.plot(n0, s0, 'o-', color='crimson', label='Реализация 0 (in0.txt)', linewidth=2)

    # Данные из второго файла
    n1, s1 = get_speedup_data('in1.txt')
    plt.plot(n1, s1, 's-', color='darkorange', label='Реализация 1 (in1.txt)', linewidth=2)

    # Теоретическое идеальное ускорение (S = n)
    # Берем максимальное n из обоих файлов для построения эталона
    max_n = max(max(n0), max(n1))
    plt.plot([1, max_n], [1, max_n], '--', color='gray', label='Идеальное ускорение (S = n)', alpha=0.7)

    # Оформление
    plt.title('Сравнение ускорения (Speedup)', fontsize=14)
    plt.xlabel('Число ядер (n)', fontsize=12)
    plt.ylabel('Ускорение (S)', fontsize=12)
    
    # Сетка и деления
    all_n = sorted(list(set(list(n0) + list(n1))))
    plt.xticks(all_n)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend()

    # Автоматическое масштабирование с запасом
    plt.xlim(0.5, max_n + 0.5)
    plt.ylim(0, max(max(s0), max(s1), max_n) + 1)

    plt.show()

except Exception as e:
    print(f"Ошибка: {e}")
