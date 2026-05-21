import matplotlib.pyplot as plt

def get_data_from_file(file_path):
    """Функция только читает данные и возвращает отсортированные списки"""
    n_values = []
    time_values = []
    
    with open(file_path, 'r') as file:
        for line in file:
            parts = line.split()
            if len(parts) >= 2:
                n = int(parts[0])
                # Заменяем запятую на точку для корректного преобразования во float
                time = float(parts[1].replace(',', '.'))
                n_values.append(n)
                time_values.append(time)

    # Сортируем данные по n
    data = sorted(zip(n_values, time_values))
    return zip(*data) # Возвращает два кортежа: (n_sorted, time_sorted)

# 1. Создаем один холст для всех графиков
plt.figure(figsize=(10, 6))

try:
    # 2. Обрабатываем первый файл
    n1, t1 = get_data_from_file('in0.txt')
    plt.plot(n1, t1, 'o-', label='Реализация 1 (in0.txt)', linewidth=2, markersize=8)

    # 3. Обрабатываем второй файл
    n2, t2 = get_data_from_file('in1.txt')
    plt.plot(n2, t2, 's--', label='Реализация 2 (in1.txt)', linewidth=2, markersize=8)

    # 4. Оформление (делается один раз для всего холста)
    plt.title('Сравнение времени выполнения', fontsize=14)
    plt.xlabel('Число ядер (n)', fontsize=12)
    plt.ylabel('Время выполнения (сек)', fontsize=12)
    
    # Настраиваем деления на оси X (1, 2, 4, 8, 16...)
    all_n = sorted(list(set(list(n1) + list(n2))))
    plt.xticks(all_n) 

    plt.grid(True, linestyle='--', alpha=0.6)
    plt.legend() # Показывает названия графиков
    
    plt.show()

except FileNotFoundError as e:
    print(f"Ошибка: файл не найден - {e}")
except Exception as e:
    print(f"Произошла ошибка: {e}")
