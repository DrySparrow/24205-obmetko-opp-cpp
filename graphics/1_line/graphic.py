import matplotlib.pyplot as plt

def get_data_from_file(file_path):
    """Читает данные из файла, пропуская заголовок, и возвращает отсортированные списки"""
    n_values = []
    time_values = []
    
    with open(file_path, 'r') as file:
        for line in file:
            line = line.strip()
            if not line or line.startswith('N'):  # Пропускаем пустые строки и заголовок
                continue
                
            # Пробуем разделить по ';' (как в вашем выводе) или по пробелу
            parts = line.split(';') if ';' in line else line.split()
            
            if len(parts) >= 2:
                try:
                    n = int(parts[0])
                    # Заменяем запятую на точку для корректного преобразования
                    time = float(parts[1].replace(',', '.'))
                    n_values.append(n)
                    time_values.append(time)
                except ValueError:
                    continue

    # Сортируем данные по N для корректного отображения графика
    data = sorted(zip(n_values, time_values))
    return zip(*data) if data else ([], [])

# Настройка холста
plt.figure(figsize=(10, 6))

try:
    # 1. Читаем данные из одного файла in.txt
    n_data, t_data = get_data_from_file('in.txt')
    
    if n_data:
        # 2. Отрисовка графика
        plt.plot(n_data, t_data, 'o-', color='blue', label='MPI 2D (16 ядер)', linewidth=2, markersize=8)

        # 3. Оформление согласно заданию
        plt.title('Зависимость времени выполнения от размера матрицы N', fontsize=14)
        plt.xlabel('Размер матрицы (N)', fontsize=12)
        plt.ylabel('Время выполнения (сек)', fontsize=12)
        
        # Настройка сетки и легенды
        plt.grid(True, linestyle='--', alpha=0.7)
        plt.legend()
        
        # Настраиваем деления на оси X (все встреченные значения N)
        plt.xticks(n_data)
        
        plt.tight_layout()
        plt.show()
    else:
        print("Данные в файле in.txt не найдены или имеют неверный формат.")

except FileNotFoundError:
    print("Ошибка: файл in.txt не найден.")
except Exception as e:
    print(f"Произошла ошибка: {e}")
