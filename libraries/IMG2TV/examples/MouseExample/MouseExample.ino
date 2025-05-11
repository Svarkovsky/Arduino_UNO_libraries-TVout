#include <TVout.h>
#include <PS2uartKeyboard.h> // Используем вашу модифицированную библиотеку (с разными кодами для Backspace/Delete)
#include <fontALL.h>
#include <ctype.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <stdlib.h> // Для itoa
#include <avr/io.h> // Для SP и __brkval (для getFreeRam)
#include <string.h> // Для strcat

// --- Константы для TVout ---
#define VIDEO_SYSTEM NTSC
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 96
#define FONT_TYPE font4x6
#define CHAR_WIDTH 4
#define CHAR_HEIGHT 6
#define CHARS_PER_LINE (SCREEN_WIDTH / CHAR_WIDTH) // 32
#define MAX_LINE_LENGTH (CHARS_PER_LINE - 2) // 30 (оставляем место для ">" и курсора/пробела)
#define LINES_PER_SCREEN (SCREEN_HEIGHT / CHAR_HEIGHT) // 16 - Максимальное количество строк, которое может отобразить экран

// --- Константы для звуков ---
#define TONE_DURATION_MS_SHORT 30
#define TONE_DURATION_MS_LONG 50
#define TONE_FREQ_KEY 800
#define TONE_FREQ_BACKSP 400 // Звук для Backspace и Delete
#define TONE_FREQ_ENTER 1200
#define TONE_FREQ_BUFFER_FULL 200 // Частота для предупреждения о конце строки

// Константы для мелодии приветствия
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_G4  392
#define NOTE_C5  523
#define MELODY_NOTE_DURATION 100 // Длительность ноты в мс

// --- Константы для курсора ---
#define CURSOR_BLINK_INTERVAL 500  // 

// --- Константы для EEPROM ---
// Определяем блок EEPROM для хранения строк.
// В этом варианте кода отображается фиксированный блок EEPROM строк 0-15.
// Каждая строка занимает MAX_LINE_LENGTH + 1 байт (30 символов + 1 байт для длины).
// Всего хранится MAX_LINES строк.
#define MAX_LINES LINES_PER_SCREEN // Количество строк, которые отображаются и сохраняются (16)
#define EEPROM_BLOCK_B_START 0 // Начальный адрес блока в EEPROM
#define EEPROM_BLOCK_B_SIZE (MAX_LINES * (MAX_LINE_LENGTH + 1)) // Общий размер блока (16 * 31 = 496 байт)

// --- Строки в PROGMEM ---
const char welcome_msg[] PROGMEM = "UNO MOUSE"; // Изменено приветствие
const char separator[] PROGMEM = "---------";
const char ok_msg[] PROGMEM = "ok";

// --- Глобальные объекты ---
TVout TV;
PS2uartKeyboard keyboard;

// --- Глобальные переменные ---
int cursor_x = 0; // Позиция курсора по X (в символах, относительно начала строки, 1 = после ">")
int cursor_y = 0; // Позиция курсора по Y (в строках ЭКРАНА, от 0 до MAX_LINES-1)
bool cursor_visible = false;
unsigned long last_blink = 0;
char line_buffer[MAX_LINE_LENGTH + 1] = {0}; // Буфер для ТЕКУЩЕЙ редактируемой строки
int buffer_len = 0; // Текущая длина строки в line_buffer
uint8_t current_line = 0; // Индекс ТЕКУЩЕЙ строки EEPROM (от 0 до MAX_LINES-1), которая редактируется

// --- Вспомогательные функции ---
void playTone(int freq, int duration) {
  TV.tone(freq, duration);
  delay(duration + 5); // Небольшая задержка после тона
  TV.noTone();
}

// Мелодия приветствия
void playWelcomeMelody() {
  playTone(NOTE_C4, MELODY_NOTE_DURATION);
  playTone(NOTE_E4, MELODY_NOTE_DURATION);
  playTone(NOTE_G4, MELODY_NOTE_DURATION);
  playTone(NOTE_C5, MELODY_NOTE_DURATION * 2); // Последняя нота длиннее
}

void updateCursorPosition() {
  // Устанавливаем позицию курсора на экране в пикселях
  TV.set_cursor(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT);
}

void drawCursor(bool show) {
  updateCursorPosition();
  if (show) {
    TV.print('_'); // Рисуем курсор
  } else {
    // Восстанавливаем символ под курсором или пробел
    // Читаем из line_buffer, так как он содержит текущее состояние строки
    if (cursor_x - 1 < buffer_len) {
      TV.print(line_buffer[cursor_x - 1]);
    } else {
      TV.print(' ');
    }
    // Небольшая задержка, чтобы символ успел прорисоваться перед следующим действием
    delay(10);
  }
}

void handleCursorBlink() {
  unsigned long current_millis = millis();
  if (current_millis - last_blink >= CURSOR_BLINK_INTERVAL) {
    cursor_visible = !cursor_visible;
    drawCursor(cursor_visible);
    last_blink = current_millis;
  }
}

// Очищает буфер строки и сбрасывает его длину
void clearLineBuffer() {
  for (int i = 0; i < MAX_LINE_LENGTH + 1; i++) {
    line_buffer[i] = 0;
  }
  buffer_len = 0;
}

// Перерисовывает ОДНУ строку на экране по заданным экранным координатам Y,
// используя содержимое line_buffer. is_active определяет, рисовать ли ">".
void redrawScreenLine(uint8_t screen_y, bool is_active) {
  TV.set_cursor(0, screen_y * CHAR_HEIGHT);
  TV.print(is_active ? ">" : " ");
  // Выводим содержимое буфера
  for (int i = 0; i < buffer_len && i < MAX_LINE_LENGTH; i++) {
    TV.print(line_buffer[i]);
  }
  // Заполняем остаток строки пробелами до конца области ввода
  for (int i = buffer_len; i < MAX_LINE_LENGTH; i++) {
    TV.print(' ');
  }
  if (is_active) {
    updateCursorPosition(); // Возвращаем курсор на его позицию, если строка активна
  }
}


// --- Функции EEPROM ---

// Сохраняет содержимое line_buffer в указанную строку EEPROM
void saveLineToEEPROM(uint8_t line_idx) {
  if (line_idx >= MAX_LINES) return; // Проверка на выход за пределы допустимых строк
  uint16_t offset = EEPROM_BLOCK_B_START + line_idx * (MAX_LINE_LENGTH + 1);

  // Сохраняем символы строки
  for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
    // Используем update для уменьшения износа EEPROM
    EEPROM.update(offset + i, line_buffer[i]);
  }
  // Сохраняем длину строки
  EEPROM.update(offset + MAX_LINE_LENGTH, (uint8_t)buffer_len); // Приводим buffer_len к uint8_t
}

// Загружает строку из указанной позиции EEPROM в line_buffer
void loadLineFromEEPROM(uint8_t line_idx) {
  if (line_idx >= MAX_LINES) {
     // Если индекс вне диапазона EEPROM, очищаем буфер
     clearLineBuffer();
     return;
  }
  uint16_t offset = EEPROM_BLOCK_B_START + line_idx * (MAX_LINE_LENGTH + 1);

  // Загружаем символы строки
  for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
    line_buffer[i] = EEPROM.read(offset + i);
  }
  line_buffer[MAX_LINE_LENGTH] = 0; // Убеждаемся, что буфер null-терминирован

  // Загружаем длину строки
  buffer_len = EEPROM.read(offset + MAX_LINE_LENGTH);

  // Проверка на корректность загруженной длины
  if (buffer_len > MAX_LINE_LENGTH) {
    buffer_len = 0; // Если длина некорректна, считаем строку пустой
    // Очищаем буфер на всякий случай, если загрузились некорректные символы
     for (int i = 0; i < MAX_LINE_LENGTH; i++) {
        line_buffer[i] = 0;
     }
  }
}

// Перерисовывает ВСЕ отображаемые строки (EEPROM 0-15 на экране 0-15)
// Загружает каждую строку из EEPROM и рисует ее.
void redrawAllLines(uint8_t active_eeprom_line) {
  TV.clear_screen(); // Очищаем весь экран перед перерисовкой

  // Перерисовываем строки EEPROM 0-15 на экранных строках 0-15
  for (uint8_t i = 0; i < MAX_LINES; i++) { // i = индекс строки EEPROM (0-15)
    loadLineFromEEPROM(i); // Загружаем строку i из EEPROM в line_buffer
    // Рисуем line_buffer на экранной строке i.
    // Активной будет строка, чей индекс EEPROM равен active_eeprom_line.
    redrawScreenLine(i, i == active_eeprom_line);
  }
  // После цикла line_buffer содержит последнюю загруженную строку (EEPROM 15).
  // Нужно снова загрузить активную строку в line_buffer для редактирования.
  loadLineFromEEPROM(active_eeprom_line);

  // Устанавливаем глобальные переменные курсора и текущей строки
  current_line = active_eeprom_line; // Индекс EEPROM активной строки
  cursor_y = active_eeprom_line; // Экранная позиция активной строки (теперь совпадает с индексом EEPROM)
  // cursor_x устанавливается в loop или при загрузке/переходе
  updateCursorPosition(); // Обновляем позицию курсора на экране
}

// Команда SAVEPBR: Сохраняет *текущее* содержимое line_buffer в EEPROM по текущему индексу.
// Затем выводит "ok" по центру и перерисовывает экран.
void saveCurrentLineAndRedraw() {
  // Сохраняем текущую строку из line_buffer в EEPROM перед перерисовкой
  saveLineToEEPROM(current_line);

  // Выводим сообщение "ok" по центру
  TV.set_cursor(((CHARS_PER_LINE - strlen(ok_msg)) / 2) * CHAR_WIDTH, ((LINES_PER_SCREEN - 1) / 2) * CHAR_HEIGHT);
  char buffer[3]; // Буфер для "ok" + null-терминатор
  strcpy_P(buffer, ok_msg);
  TV.print(buffer);

  // Увеличиваем задержку, чтобы сообщение "ok" было видно
  delay(1500); // Задержка 1.5 секунды

  // Очищаем буфер (содержал команду "SAVEPBR")
  clearLineBuffer();
  cursor_x = 1; // Сбрасываем курсор в начало строки

  // Перерисовываем все строки, показывая их состояние из EEPROM
  // Активной остается та же строка, что и была.
  redrawAllLines(current_line);
}

// Команда ERSEPBR: Стирает (заполняет нулями) весь блок EEPROM для строк (0-15).
void eraseBlockB() {
  // Заполняем блок нулями
  for (uint16_t i = EEPROM_BLOCK_B_START; i < EEPROM_BLOCK_B_START + EEPROM_BLOCK_B_SIZE; i++) {
    // Используем update для уменьшения износа EEPROM
    EEPROM.update(i, 0);
  }

  // Выводим сообщение "ok" по центру
  TV.set_cursor(((CHARS_PER_LINE - strlen(ok_msg)) / 2) * CHAR_WIDTH, ((LINES_PER_SCREEN - 1) / 2) * CHAR_HEIGHT);
  char buffer[3]; // Буфер для "ok" + null-терминатор
  strcpy_P(buffer, ok_msg);
  TV.print(buffer);

  // Увеличиваем задержку, чтобы сообщение "ok" было видно
  delay(1500); // Задержка 1.5 секунды

  // Очищаем текущий буфер
  clearLineBuffer();
  cursor_x = 1; // Сбрасываем курсор в начало строки
  current_line = 0; // Переходим на первую строку EEPROM
  cursor_y = 0; // Переходим на первую отображаемую строку экрана

  // Перерисовываем все строки (теперь они пустые)
  redrawAllLines(current_line);
}

// Функция для получения количества свободной оперативной памяти (RAM)
// Использует низкоуровневые указатели стека и кучи для экономии RAM.
extern int __heap_start, *__brkval; // Объявляем символы начала кучи и ее конца

int getFreeRam() {
  int free_memory;
  if ((int)__brkval == 0) {
    // Куча не использовалась, свободная память = указатель стека - начало кучи
    free_memory = SP - (int)&__heap_start;
  } else {
    // Свободная память = указатель стека - конец кучи
    free_memory = SP - (int)__brkval;
  }
  return free_memory;
}

// --- Setup ---
void setup() {
  delay(500); // Небольшая задержка для стабилизации
  TV.begin(VIDEO_SYSTEM, SCREEN_WIDTH, SCREEN_HEIGHT);
  TV.force_outstart(14); // Возможно, требуется для вашего железа
  TV.select_font(FONT_TYPE);
  TV.clear_screen();
  TV.set_cursor(0, 0);
  TV.set_hbi_hook(keyboard.begin()); // Инициализация клавиатуры

  // Вывод приветственного сообщения по центру
  char buffer[10]; // Буфер достаточного размера
  strcpy_P(buffer, welcome_msg);
  int welcome_len = strlen(buffer);
  // Позиция для приветствия: по центру горизонтально, чуть выше центра вертикально
  TV.set_cursor(((CHARS_PER_LINE - welcome_len) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2 - 2) * CHAR_HEIGHT);
  TV.println(buffer);

  // Вывод разделителя по центру
  strcpy_P(buffer, separator);
  int separator_len = strlen(buffer);
  // Позиция для разделителя: по центру горизонтально, под приветствием
  TV.set_cursor(((CHARS_PER_LINE - separator_len) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2 - 1) * CHAR_HEIGHT);
  TV.println(buffer);

  // Задержка после приветствия
  delay(2000); // Задержка 2 секунды

  // Проигрываем мелодию
  playWelcomeMelody();

  // Очистим экран после приветствия и начнем отображение EEPROM с первой строки экрана
  TV.clear_screen();
  current_line = 0; // Начинаем с первой строки EEPROM (индекс 0)
  cursor_y = 0; // Первая отображаемая строка на экране (индекс 0)

  // Загружаем первую строку EEPROM (EEPROM[0]) в буфер для редактирования
  loadLineFromEEPROM(current_line);

  // Устанавливаем курсор в конец загруженной строки
  cursor_x = buffer_len + 1;

  // Перерисовываем все отображаемые строки (EEPROM 0-15 на экране 0-15)
  redrawAllLines(current_line);
}

// --- Loop ---
void loop() {
  handleCursorBlink(); // Обработка мигания курсора

  if (keyboard.available()) {
 //   char c = keyboard.read(); 
 // Читаем символ с клавиатуры
    int c = keyboard.read();
    int current_freq = TONE_FREQ_KEY;
    int current_duration = TONE_DURATION_MS_SHORT;

    // Скрываем курсор при нажатии любой клавиши, кроме стрелок, Delete, Home, End
    if (cursor_visible && c != PS2_LEFTARROW && c != PS2_RIGHTARROW && c != PS2_UPARROW && c != PS2_DOWNARROW && c != PS2_DELETE && c != PS2_HOME && c != PS2_END) {
      drawCursor(false);
      cursor_visible = false;
    }

    // Обработка нажатия Enter
    if (c == PS2_ENTER || c == '\r' || c == '\n') {
      current_freq = TONE_FREQ_ENTER;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);

      // Проверка на команды
      // ВНИМАНИЕ: Команды должны быть введены ТОЧНО как "SAVEPBR", "ERSEPBR" или "FREERAM" (заглавными)
      // без лишних пробелов.
      if (strcmp(line_buffer, "SAVEPBR") == 0) {
        saveCurrentLineAndRedraw(); // Выполняем команду сохранения
      }
      else if (strcmp(line_buffer, "ERSEPBR") == 0) {
        eraseBlockB(); // Выполняем команду стирания
      }
      else if (strcmp(line_buffer, "FREERAM") == 0) {
        // Команда FREERAM
        // Сохраняем текущую строку перед выполнением команды (на всякий случай)
        saveLineToEEPROM(current_line);
        // Очищаем буфер после выполнения команды
        clearLineBuffer();
        cursor_x = 1; // Сбрасываем курсор в начало строки
        // Перерисовываем текущую строку, чтобы убрать команду
        redrawScreenLine(cursor_y, true);

        // Получаем свободную RAM
        int free_ram = getFreeRam();
        char ram_buffer[15]; // Буфер для числа + " bytes" + null-терминатор (например, "1024 bytes")
        itoa(free_ram, ram_buffer, 10); // Преобразуем число в строку
        strcat(ram_buffer, " b"); // Добавляем " bytes"

        // Выводим сообщение по центру экрана
        int msg_len = strlen(ram_buffer);
        TV.set_cursor(((CHARS_PER_LINE - msg_len) / 2) * CHAR_WIDTH, ((LINES_PER_SCREEN - 1) / 2) * CHAR_HEIGHT);
        TV.print(ram_buffer);

        // Задержка для отображения сообщения
        delay(3000); // 3 секунды

        // Перерисовываем весь экран, чтобы вернуться к редактору
        redrawAllLines(current_line);

      }
      else {
        // Если это не команда, сохраняем текущую строку в EEPROM
        saveLineToEEPROM(current_line);

        // Переходим на следующую строку EEPROM и экрана
        clearLineBuffer(); // Очищаем буфер для новой строки
        cursor_x = 1; // Сбрасываем позицию курсора X

        // Переход к следующей строке EEPROM (с 0 на 15 и обратно)
        current_line++;
        if (current_line >= MAX_LINES) {
          current_line = 0;
        }
        // Экранная позиция курсора соответствует индексу EEPROM
        cursor_y = current_line;

        // Загружаем новую текущую строку из EEPROM (она может быть пустой)
        loadLineFromEEPROM(current_line);

        // Устанавливаем курсор в конец загруженной строки
        cursor_x = buffer_len + 1;

        // Перерисовываем все строки, показывая новую активную
        redrawAllLines(current_line);
      }
    }
    // Обработка Backspace (удаление символа слева)
    // Проверяем на PS2_BACKSPACE (127)
    else if (c == PS2_BACKSPACE) {
      current_freq = TONE_FREQ_BACKSP;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);
      // Если курсор не в начале строки (после ">") и буфер не пустой
      if (cursor_x > 1 && buffer_len > 0) {
        // Сдвигаем символы влево, начиная с позиции ПЕРЕД курсором (cursor_x - 2)
        for (int i = cursor_x - 2; i < buffer_len - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }
        // Очищаем последний символ и уменьшаем длину
        buffer_len--;
        line_buffer[buffer_len] = 0;
        cursor_x--; // Сдвигаем курсор влево
        redrawScreenLine(cursor_y, true); // Перерисовываем только текущую строку
      }
    }
    // Обработка Delete (удаление символа справа)
    // Проверяем на PS2_DELETE (255)
    else if (c == PS2_DELETE) {
      current_freq = TONE_FREQ_BACKSP; // Используем тот же звук, что и для Backspace
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);
      // Если курсор находится перед символом, который можно удалить (т.e. не в конце введенного текста)
      if (cursor_x - 1 < buffer_len) {
        // Сдвигаем символы влево, начиная с позиции СРАЗУ после курсора (cursor_x - 1)
        for (int i = cursor_x - 1; i < buffer_len - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }
        // Очищаем последний символ и уменьшаем длину
        buffer_len--;
        line_buffer[buffer_len] = 0;
        // Курсор остается на месте - НЕ СДВИГАЕМ cursor_x
        redrawScreenLine(cursor_y, true); // Перерисовываем только текущую строку
      }
    }
    // Обработка стрелки Влево
    else if (c == PS2_LEFTARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      // Если курсор не в начале строки (после ">")
      if (cursor_x > 1) {
        cursor_x--; // Сдвигаем курсор влево
        // При движении курсора влево/вправо, просто обновляем позицию курсора
        updateCursorPosition();
      }
    }
    // Обработка стрелки Вправо
    else if (c == PS2_RIGHTARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      // Если курсор не в конце введенного текста и не за пределами MAX_LINE_LENGTH
      if (cursor_x < buffer_len + 1 && cursor_x < MAX_LINE_LENGTH + 1) {
        cursor_x++; // Сдвигаем курсор вправо
        // При движении курсора влево/вправо, просто обновляем позицию курсора
        updateCursorPosition();
      }
    }
    // Обработка стрелки Вверх
    else if (c == PS2_UPARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);

      // Сохраняем текущую строку перед переключением
      saveLineToEEPROM(current_line);

      // Переходим к предыдущей строке EEPROM (с 0 на 15 и обратно)
      if (current_line == 0) {
        current_line = MAX_LINES - 1;
      } else {
        current_line--;
      }
      // Экранная позиция курсора соответствует индексу EEPROM
      cursor_y = current_line;

      // Загружаем новую текущую строку из EEPROM в буфер
      loadLineFromEEPROM(current_line);

      // Сохраняем желаемую горизонтальную позицию курсора
      int desired_cursor_x = cursor_x;
      // Устанавливаем курсор в конец загруженной строки или сохраняем позицию
      cursor_x = min(desired_cursor_x, buffer_len + 1);
      if (cursor_x < 1) cursor_x = 1; // Курсор не может быть левее ">"

      // Перерисовываем все строки, показывая новую активную
      redrawAllLines(current_line);
    }
    // Обработка стрелки Вниз
    else if (c == PS2_DOWNARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);

      // Сохраняем текущую строку перед переключением
      saveLineToEEPROM(current_line);

      // Переходим к следующей строке EEPROM (с 0 на 15 и обратно)
      current_line++;
      if (current_line >= MAX_LINES) {
        current_line = 0;
      }
      // Экранная позиция курсора соответствует индексу EEPROM
      cursor_y = current_line;

      // Загружаем новую текущую строку из EEPROM в буфер
      loadLineFromEEPROM(current_line);

      // Сохраняем желаемую горизонтальную позицию курсора
      int desired_cursor_x = cursor_x;
      // Устанавливаем курсор в конец загруженной строки или сохраняем позицию
      cursor_x = min(desired_cursor_x, buffer_len + 1);
      if (cursor_x < 1) cursor_x = 1; // Курсор не может быть левее ">"

      // Перерисовываем все строки, показывая новую активную
      redrawAllLines(current_line);
    }
    // Обработка Home
    else if (c == PS2_HOME) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      cursor_x = 1; // Курсор после ">"
      updateCursorPosition(); // Просто обновляем позицию курсора
    }
    // Обработка End
    else if (c == PS2_END) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      cursor_x = buffer_len + 1; // Курсор после последнего символа
      updateCursorPosition(); // Просто обновляем позицию курсора
    }
    // Обработка печатных символов
    else if (isprint(c)) {
      // Если в буфере есть место (не достигнут MAX_LINE_LENGTH)
      if (buffer_len < MAX_LINE_LENGTH) {
        current_freq = TONE_FREQ_KEY;
        current_duration = TONE_DURATION_MS_SHORT;
        playTone(current_freq, current_duration);

        // Сдвигаем символы вправо, чтобы вставить новый символ на позицию курсора
        for (int i = buffer_len; i >= cursor_x - 1; i--) {
          line_buffer[i] = line_buffer[i + 1]; // Исправлено: сдвигаем line_buffer[i] = line_buffer[i+1]
        }
        // Вставляем новый символ (преобразуем в верхний регистр)
        line_buffer[cursor_x - 1] = toupper(c);
        buffer_len++; // Увеличиваем длину буфера
        cursor_x++; // Сдвигаем курсор вправо
        redrawScreenLine(cursor_y, true); // Перерисовываем только текущую строку
      } else {
        // Если буфер полон, издаем звуковое предупреждение
        playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT);
      }
    }
    // Обработка остальных символов (непечатных, кроме обработанных выше)
    else {
      // Можно добавить обработку других спец. клавиш, если нужно
      // playTone(current_freq, current_duration); // Убрал звук для необработанных клавиш, чтобы не мешал
    }
  }
}

// Макрос min уже определен в Arduino.h, свою функцию не нужно создавать.
// int min(int a, int b) {
//     return (a < b) ? a : b;
// }
