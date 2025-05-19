/*
MIT License

Copyright (c) Ivan Svarkovsky - 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR_THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <TVout.h>
#include <fontALL.h> // Включаем файл со всеми шрифтами

// Определяем структуру для одной колонки
struct COLUMN {
  int Y;        // Текущая вертикальная позиция головы колонки
  int PREV_Y;   // Предыдущая вертикальная позиция головы колонки
  int LENGTH;   // Длина хвоста колонки
  // int SPEED;    // Скорость падения колонки (теперь всегда 1)
  int SPEED_COUNTER; // <-- Счетчик для скорости падения
  char CURRENT_CHAR; // Текущий символ для этой колонки
  int CHANGE_COUNTER; // Счетчик для смены символа
};

// Объект TVout
TVout TV;

// Строка символов для отображения
String CHARACTERS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+-=[]{}|;:',.<>/?\\\"";
// String CHARACTERS = "MIRON"; // Если хотите только "MIRON"

// Количество колонок. Оставляем 10.
const int NUM_COLUMNS = 10;
COLUMN COLUMNS[NUM_COLUMNS];

// Порог для смены символа. Чем больше значение, тем медленнее меняются символы.
const int CHARACTER_CHANGE_THRESHOLD = 10; // Меняйте это значение (например, 5, 15, 20)

// Порог для скорости падения. Чем больше значение, тем медленнее падение.
// Увеличьте это значение, чтобы замедлить падение.
const int FALL_SPEED_THRESHOLD = 2; // <-- Увеличено для замедления (например, 3, 4, 5)

void setup() {
  randomSeed(analogRead(5)); // Инициализация генератора случайных чисел

  // Инициализация TVout для NTSC, разрешение 128x96
  TV.begin(NTSC, 128, 96);
  // TV.begin(PAL, 128, 96); // Для PAL региона

  // --- Настройка горизонтального смещения ---
  TV.force_outstart(14); // Начальное значение для NTSC 128x96

  // Выбираем шрифт. 
  TV.select_font(font6x8);

  TV.clear_screen(); // Очистить экран (заполнить черным)

  // Инициализация колонок
  for (int C = 0; C < NUM_COLUMNS; C++) {
    COLUMNS[C].Y = random(-100, 0); // Начинаем выше экрана
    COLUMNS[C].PREV_Y = COLUMNS[C].Y; // Инициализируем PREV_Y
    COLUMNS[C].LENGTH = random(15, 40); // Случайная длина хвоста
    // COLUMNS[C].SPEED = random(1, 3); // Скорость теперь всегда 1
    COLUMNS[C].SPEED_COUNTER = random(0, FALL_SPEED_THRESHOLD); // <-- Инициализация счетчика скорости
    COLUMNS[C].CURRENT_CHAR = CHARACTERS.charAt(random(0, CHARACTERS.length())); // Инициализация символа
    COLUMNS[C].CHANGE_COUNTER = random(0, CHARACTER_CHANGE_THRESHOLD); // Инициализация счетчика смены символа
  }
}

void loop() {
  // Обновляем и отрисовываем каждую колонку
  for (int C = 0; C < NUM_COLUMNS; C++) {

    // --- Логика смены символа ---
    COLUMNS[C].CHANGE_COUNTER++; // Увеличиваем счетчик
    if (COLUMNS[C].CHANGE_COUNTER >= CHARACTER_CHANGE_THRESHOLD) {
      // Если счетчик достиг порога, выбираем новый символ и сбрасываем счетчик
      COLUMNS[C].CURRENT_CHAR = CHARACTERS.charAt(random(0, CHARACTERS.length()));
      COLUMNS[C].CHANGE_COUNTER = 0;
    }

    // --- Логика скорости падения ---
    COLUMNS[C].SPEED_COUNTER++; // Увеличиваем счетчик скорости
    bool should_move = false;
    if (COLUMNS[C].SPEED_COUNTER >= FALL_SPEED_THRESHOLD) {
        should_move = true;
        COLUMNS[C].SPEED_COUNTER = 0; // Сбрасываем счетчик скорости
    }

    // Обновляем позицию только если пришло время двигаться
    if (should_move) {
        // Сохраняем текущую позицию головы перед обновлением
        COLUMNS[C].PREV_Y = COLUMNS[C].Y;
        // Обновляем позицию головы колонки (сдвигаем вниз)
        COLUMNS[C].Y += 1; // <-- Скорость падения всегда 1 пиксель за FALL_SPEED_THRESHOLD кадров
    }

    // --- Логика стирания и отрисовки ---

    // 1. Стираем символ в предыдущей позиции головы колонки (если она была на экране)
    // Это нужно делать ТОЛЬКО если колонка СДВИНУЛАСЬ в этом кадре.
    if (should_move && COLUMNS[C].PREV_Y >= 0 && COLUMNS[C].PREV_Y < TV.vres()) {
      // Горизонтальная позиция: C * 12 (ИСПОЛЬЗУЕМ ЦЕЛОЕ ЧИСЛО)
      TV.print_char(C * 12, COLUMNS[C].PREV_Y, ' '); // Стираем символом пробела (черный)
    }

    // 2. Стираем символ в позиции хвоста колонки, если он ушел за пределы длины хвоста
    // Позиция хвоста = Текущая позиция головы - Длина хвоста
    int tailY = COLUMNS[C].Y - COLUMNS[C].LENGTH;
    // Проверяем, что позиция хвоста находится в пределах экрана
     if (tailY >= 0 && tailY < TV.vres()) {
         // Горизонтальная позиция: C * 12 (ИСПОЛЬЗУЕМ ЦЕЛОЕ ЧИСЛО)
         TV.print_char(C * 12, tailY, ' '); // Стираем символом пробела (черный)
     }

    // 3. Отрисовываем символ в текущей позиции головы колонки
    // Проверяем, что текущая позиция находится в пределах экрана
    if (COLUMNS[C].Y >= 0 && COLUMNS[C].Y < TV.vres()) {
      // Горизонтальная позиция: C * 12 (ИСПОЛЬЗУЕМ ЦЕЛОЕ ЧИСЛО)
      // Используем символ, сохраненный в CURRENT_CHAR
      TV.print_char(C * 12, COLUMNS[C].Y, COLUMNS[C].CURRENT_CHAR); // Отрисовываем символ (белый)
    }

    // --- Логика сброса колонки ---

    // Если голова колонки ушла за нижний край экрана + ее длина хвоста
    // (т.е. весь хвост ушел за экран), сбрасываем колонку наверх.
    if (COLUMNS[C].Y > TV.vres() + COLUMNS[C].LENGTH) {
      COLUMNS[C].Y = random(-100, 0); // Сброс Y в случайную позицию над экраном
      COLUMNS[C].PREV_Y = COLUMNS[C].Y; // Сбрасываем PREV_Y
      COLUMNS[C].LENGTH = random(15, 40); // Сброс случайной длины
      // COLUMNS[C].SPEED = random(1, 3); // Скорость теперь всегда 1
      COLUMNS[C].SPEED_COUNTER = random(0, FALL_SPEED_THRESHOLD); // <-- Новый начальный счетчик скорости
      COLUMNS[C].CURRENT_CHAR = CHARACTERS.charAt(random(0, CHARACTERS.length())); // <-- Новый начальный символ
      COLUMNS[C].CHANGE_COUNTER = random(0, CHARACTER_CHANGE_THRESHOLD); // <-- Новый начальный счетчик смены символа
    }
  }

  // Используем delay_frame(1) для синхронизации обновления анимации с частотой кадров ТВ.
  TV.delay_frame(1);
}
