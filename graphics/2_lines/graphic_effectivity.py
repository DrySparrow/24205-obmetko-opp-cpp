import matplotlib.pyplot as plt

def get_efficiency_data(file_path):
    """Читает файл и возвращает (n_sorted, efficiency)"""
    n_values = []
    time_values = []

    with open(file_path, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 2:
                n_values.append(int(parts[0]))
                time_values.append(float(parts[1].replace(',', '.')))

    # Сортировка данных по количеству ядер
    data = sorted(zip(n_values, time_values))
    n_sorted, time_sorted = zip(*data)

    # Расчет эффективности: E = T1 / (Tn * n)
    t1 = time_sorted[0]
    efficiency = [(t1 / (tn * n)) for tn, n in zip(time_sorted, n_sorted)]
    
    return n_sorted, efficiency

# --- Основной блок построения графиков ---

plt.figure(figsize=(10, 6))

try:
    # Данные из первого файла (in0.txt)
    n0, eff0 = get_efficiency_data('in0.txt')
    plt.plot(n0, eff0, 'o-', color='forestgreen', label='Реализация 0 (in0.txt)', linewidth=2)

    # Данные из второго файла (in1.txt)
    n1, eff1 = get_efficiency_data('in1.txt')
    plt.plot(n1, eff1, 's-', color='royalblue', label='Реализация 1 (in1.txt)', linewidth=2)

    # Линия идеальной эффективности (E = 1.0)
    plt.axhline(y=1.0, color='red', linestyle='--', label='Идеал (100%)', alpha=0.5)

    # Оформление
    plt.title('Сравнение эффективности распараллеливания', fontsize=14)
    plt.xlabel('Число ядер (n)', fontsize=12)
    plt.ylabel('Эффективность (E)', fontsize=12)
    
    # Объединяем все n, чтобы шкала X была полной
    all_n = sorted(list(set(list(n0) + list(n1))))
    plt.xticks(all_n)
    
    current_values = eff0 + eff1
    plt.ylim(0, max(current_values) * 1.1)
    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend()

    plt.show()

except Exception as e:
    print(f"Произошла ошибка: {e}")
