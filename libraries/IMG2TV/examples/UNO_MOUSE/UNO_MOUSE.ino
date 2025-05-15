// UNO MOUSE v0.1 (test)
/*
MIT License

Copyright (c) Ivan Svarkovsky - 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/



#include <TVout.h>
#include <PS2uartKeyboard.h>
#include <fontALL.h>
#include <ctype.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include <stdlib.h>
#include <avr/io.h>
#include <string.h>

// --- Константы для TVout ---
#define VIDEO_SYSTEM NTSC
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 96
#define FONT_TYPE font4x6
#define CHAR_WIDTH 4
#define CHAR_HEIGHT 6
#define CHARS_PER_LINE (SCREEN_WIDTH / CHAR_WIDTH) // 32
#define MAX_LINE_LENGTH (CHARS_PER_LINE - 2) // 30
#define LINES_PER_SCREEN (SCREEN_HEIGHT / CHAR_HEIGHT) // 16

// --- Константы для звуков ---
#define TONE_DURATION_MS_SHORT 30
#define TONE_DURATION_MS_LONG 50
#define TONE_FREQ_KEY 800
#define TONE_FREQ_BACKSP 400
#define TONE_FREQ_ENTER 1200 // Используется для звука Enter и стрелки вниз
#define TONE_FREQ_BUFFER_FULL 200

// Константы для мелодии приветствия
#define NOTE_C4  262
#define NOTE_E4  330
#define NOTE_G4  392
#define NOTE_C5  523
#define MELODY_NOTE_DURATION 100

// --- Константы для курсора ---
#define CURSOR_BLINK_INTERVAL 500

// --- Константы для EEPROM ---
// Уменьшаем количество строк для программы, чтобы освободить место для 16-битных адресов макросов
#define MAX_LINES 31 // 31 * 31 = 961 байт для программы.
#define EEPROM_BLOCK_START 0
// Размер блока программы: MAX_LINES * (MAX_LINE_LENGTH + 1)
#define EEPROM_BLOCK_SIZE (MAX_LINES * (MAX_LINE_LENGTH + 1)) // 961 байт

// Адрес в EEPROM для хранения определений макросов (16-битные адреса)
// Начинаем сразу после блока программы
#define EEPROM_MACRO_LINES_START (EEPROM_BLOCK_START + EEPROM_BLOCK_SIZE) // 961

// Магическое число для проверки инициализации области макросов в EEPROM
#define EEPROM_MACRO_MAGIC_ADDR EEPROM_MACRO_LINES_START // 961
#define EEPROM_MACRO_MAGIC_VALUE 0xAF // Произвольное значение

// Адрес начала данных макросов (после магического числа)
#define EEPROM_MACRO_DATA_START (EEPROM_MACRO_MAGIC_ADDR + 1) // 962
// Размер области макросов: MAX_MACROS = 26 * 2 байта = 52 байта.
// Общий размер EEPROM: 961 (программа) + 1 (магия) + 52 (макросы) = 1014 байт.
// На Arduino Uno 1024 байта EEPROM, так что все помещается.

// --- Константы для Mouse ---
#define STACK_SIZE 20
#define MAX_NESTING 3 // Используется для call_stack (для циклов и макросов)
#define MAX_MACROS 26 // A-Z

// --- Строки в PROGMEM ---
const char welcome_msg[] PROGMEM = "UNO MOUSE";
const char separator[] PROGMEM = "---------";
const char ok_msg[] PROGMEM = "OK";
const char syntax_error_msg[] PROGMEM = "SYNTAX ERROR";
const char program_ended_msg[] PROGMEM = "PROGRAM ENDED";
const char press_any_key_msg[] PROGMEM = "PRESS ANY KEY...";
const char unmatched_bracket_msg[] PROGMEM = "UNMATCHED BRACKET";
const char stack_overflow_msg[] PROGMEM = "STACK OVERFLOW";
const char call_stack_overflow_msg[] PROGMEM = "CALL STACK OVERFLOW";
const char call_stack_underflow_msg[] PROGMEM = "CALL STACK UNDERFLOW";
const char break_outside_loop_msg[] PROGMEM = "BREAK OUTSIDE LOOP";
const char undefined_macro_msg[] PROGMEM = "UNDEFINED MACRO";
// const char invalid_macro_def_msg[] PROGMEM = "INVALID MACRO DEF"; // Not needed with $X
const char invalid_macro_call_msg[] PROGMEM = "INVALID MACRO CALL";
const char test_loaded_msg[] PROGMEM = "TEST LOADED";


// --- TEST PROGRAM STORED IN PROGMEM ---
// USE \N AS LINE DELIMITER
// MAX LINE LENGTH IN EEPROM IS 30 CHARS.
const char test_program_flash[] PROGMEM =
"\"--- UNO MOUSE SELF-TEST ---\" ! !\n" // HEADER
"\"T1!\"! 10 12 + 22 #C@\n" // 10 + 12 = 22
"\"T2!\"! 5 10 - -5 #C@\n" // 5 - 10 = -5
"\"T3!\"! 3 4 * 12 #C@\n" // 3 * 4 = 12
"\"T4!\"! 15 3 / 5 #C@\n" // 15 / 3 = 5
"\"T5!\"! 17 3 \\\\ 2 #C@\n" // 17 % 3 = 2 (NOTE: \\\\ FOR \)
"\"T6!\"! 5 0 / 0 #C@\n" // 5 / 0 = 0
"\"T7!\"! 5 0 \\\\ 0 #C@\n" // 5 % 0 = 0
"\"T8!\"! A 10 = A. 10 #C@\n" // VAR A ASSIGN/GET (A=10)
"\"T9!\"! B -20 = B. -20 #C@\n" // VAR B ASSIGN/GET (B=-20)
"\"T10!\"! N 13 #C@\n" // VAR N ADDRESS ('N'-'A'=13)
"\"T11!\"! 5 3 > 1 #C@\n" // 5 > 3 = 1
"\"T12!\"! 3 5 > 0 #C@\n" // 3 > 5 = 0
"\"T13!\"! 3 3 > 0 #C@\n" // 3 > 3 = 0
"\"T14!\"! 3 5 < 1 #C@\n" // 3 < 5 = 1
"\"T15!\"! 5 3 < 0 #C@\n" // 5 < 3 = 0
"\"T16!\"! 3 3 < 0 #C@\n" // 3 < 3 = 0
"\"T17!\"! 5 5 - 0 #C@\n" // 5 == 5 (USING SUBTRACTION)
"\"T18!\"! 5 3 - 2 #C@\n" // 5 != 3 (DIFF > 0)
"\"T19!\"! 3 5 - -2 #C@\n" // 3 != 5 (DIFF < 0)
"\"T20!\"! 1 [\" T20 O!\"!] \"O!\"!\n" // CONDITIONAL TRUE (PRINTS O IF TRUE BLOCK RUNS)
"\"T21!\"! 0 [\" T21 F!\"!] \"O!\"!\n" // CONDITIONAL FALSE (PRINTS O IF FALSE BLOCK SKIPPED)
"\"T22!\"! L 1 = (L. 4 - ^ L L. 1 + = ) L. 4 #C@\n" // LOOP (COUNT 1 TO 3, CHECK L=4 AFTER EXIT)
"\"T23!\"! #M@ \"O!\"!\n" // MACRO CALL (PRINTS O IF MACRO RUNS AND RETURNS)
"\"T24!\"! \" T24 O!\"!\" NEWLINE!\"!\n" // STRING/NEWLINE
"\"T25!\"! \" T25 ENTER 42: \"! ? 42 #C@\n" // INPUT (INTERACTIVE)
"\"T26!\"! \" T26 TRACE { } \"! { \"ON!\"!} \"TRACING!\"!} \"OFF!\"!\"O!\"!\n" // TRACE (INTERACTIVE/VISUAL)
"\"---DONE!\"!!\n" // DONE MESSAGE
"$$\n" // END MARKER
"\n" // ENSURE NEWLINE BEFORE MACRO DEFINITIONS
"$C - [\"F!\"!] 0 - [\"F!\"!] \"O!\"!@\n" // COMPACT CHECK MACRO (F=FAIL, O=OK)
"$M \" T23 O!\"!@\n" // MACRO M DEFINITION
;




// --- Глобальные объекты ---
TVout TV;
PS2uartKeyboard keyboard;

// --- Глобальные переменные ---
struct {
  uint8_t cursor_visible : 1;
  uint8_t running : 1;
  uint8_t tracing : 1;
  uint8_t stack_overflow : 1; // Флаг переполнения стека
} flags = {0, 0, 0, 0}; // Инициализируем tracing и stack_overflow в 0
unsigned long last_blink = 0;
char line_buffer[MAX_LINE_LENGTH + 1] = {0};
uint8_t buffer_len = 0;
uint8_t current_line = 0; // Текущая строка в редакторе
uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

// Input buffer for '?' command
char input_buffer[10]; // Sufficient for -128 to 127 + sign + null
uint8_t input_buffer_ptr = 0;


// --- Mouse интерпретатор ---
int8_t stack[STACK_SIZE];
uint8_t stack_ptr = 0;
int8_t vars[26];
// call_stack[i][0] = line_idx, call_stack[i][1] = pc (для циклов и макросов)
uint8_t call_stack[MAX_NESTING][2];
uint8_t call_stack_ptr = 0;
// macro_addresses now stored in EEPROM (16-bit)

// --- Таблица команд в PROGMEM ---
typedef void (*cmd_func_t)();
#define CMD(c, f) {c, f}
void cmd_add(); void cmd_sub(); void cmd_mul(); void cmd_div(); void cmd_mod();
void cmd_assign(); void cmd_var_get(); void cmd_lt(); void cmd_gt();
void cmd_print_num(); void cmd_input();
void cmd_trace_on(); void cmd_trace_off();

const struct {
  char cmd; // Команда
  cmd_func_t func;
} cmd_table[] PROGMEM = {
  CMD('+', cmd_add),
  CMD('-', cmd_sub),
  CMD('*', cmd_mul),
  CMD('/', cmd_div),
  CMD('\\', cmd_mod),
  CMD('=', cmd_assign),
  CMD('.', cmd_var_get),
  CMD('<', cmd_lt),
  CMD('>', cmd_gt),
  // CMD('~', cmd_equal_cmp), // Removed - use subtraction for equality check
  CMD('!', cmd_print_num),
  CMD('?', cmd_input),
  CMD('{', cmd_trace_on),
  CMD('}', cmd_trace_off)
};

// --- Функции команд ---
void push(int8_t v) {
    if (stack_ptr < STACK_SIZE) {
        stack[stack_ptr++] = v;
    } else { // Переполнение стека
        flags.stack_overflow = 1; // Устанавливаем флаг ошибки
        // Stack overflow handling is done in runProgram
    }
}
int8_t pop() { return stack_ptr > 0 ? stack[--stack_ptr] : 0; }

void cmd_add() { if (stack_ptr >= 2) { int8_t b = pop(); push(pop() + b); } else { push(0); } }
void cmd_sub() { if (stack_ptr >= 2) { int8_t b = pop(); push(pop() - b); } else { push(0); } }
void cmd_mul() { if (stack_ptr >= 2) { int8_t b = pop(); push(pop() * b); } else { push(0); } }
void cmd_div() { if (stack_ptr >= 2) { int8_t b = pop(); if (b != 0) push(pop() / b); else push(0); } else { push(0); } }
void cmd_mod() { if (stack_ptr >= 2) { int8_t b = pop(); if (b != 0) push(pop() % b); else push(0); } else { push(0); } }

void cmd_assign() {
  if (stack_ptr >= 2) {
    int8_t v = pop();
    int8_t a = pop();
    if (a >= 0 && a < 26) {
        vars[a] = v;
    }
  } else if (stack_ptr == 1) {
      int8_t a = pop();
      if (a >= 0 && a < 26) vars[a] = 0;
  }
}

void cmd_var_get() { if (stack_ptr > 0) { int8_t a = pop(); if (a >= 0 && a < 26) push(vars[a]); else push(0); } else { push(0); } }

void cmd_lt() { if (stack_ptr >= 2) { int8_t b = pop(); push(pop() < b); } else { push(0); } }
void cmd_gt() { if (stack_ptr >= 2) { int8_t b = pop(); push(pop() > b); } else { push(0); } }

// cmd_equal_cmp removed - use subtraction for equality check

void cmd_print_num() {
  if (stack_ptr > 0) {
    int8_t value = pop();
    char num_str[5];
    itoa(value, num_str, 10);
    TV.print(num_str);

    // Trace output for '!' is handled here
    if (flags.tracing) {
      TV.println();
      print_stack_content();
      TV.print(" (after !)");
    }
  }
}

void cmd_input() {
    // Blocking number input
    TV.print("? "); // Prompt
    input_buffer_ptr = 0;
    input_buffer[0] = 0;
    bool reading = true;
    while(reading) {
        while(!keyboard.available());
        int k = keyboard.read();

        if (k == PS2_ENTER || k == '\r' || k == '\n') {
            reading = false;
            playTone(TONE_FREQ_ENTER, TONE_DURATION_MS_LONG);
        } else if (k == PS2_BACKSPACE) {
            if (input_buffer_ptr > 0) {
                input_buffer_ptr--;
                input_buffer[input_buffer_ptr] = 0;
                TV.print('\b'); // Backspace
                TV.print(' ');  // Overwrite char
                TV.print('\b'); // Move cursor back
                playTone(TONE_FREQ_BACKSP, TONE_DURATION_MS_SHORT);
            } else {
                 playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT); // Indicate cannot backspace
            }
        } else if (isprint(k)) {
            if (input_buffer_ptr < sizeof(input_buffer) - 1) {
                // Allow '-' only at the beginning
                if (k == '-' && input_buffer_ptr > 0) {
                     playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT); // Indicate invalid char
                     continue;
                }
                // Allow digits
                if (isdigit(k) || k == '-') {
                    input_buffer[input_buffer_ptr++] = k;
                    input_buffer[input_buffer_ptr] = 0; // Null terminate
                    TV.print((char)k);
                    playTone(TONE_FREQ_KEY, TONE_DURATION_MS_SHORT);
                } else {
                     playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT); // Indicate invalid char
                }
            } else {
                playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT); // Indicate buffer full
            }
        }
    }
    TV.println(); // Newline after input

    // Convert and push
    long value = atol(input_buffer);
    // Check if value fits in int8_t
    if (value >= -128 && value <= 127) {
        push((int8_t)value);
    } else {
        // Value out of range, push 0 (or handle as error?) - Push 0 for simplicity like pop on empty stack
        push(0);
        // Optional: Indicate error? TV.println("Value out of range, pushed 0");
    }
}


void cmd_trace_on() { flags.tracing = 1; }
void cmd_trace_off() { flags.tracing = 0; }

void print_stack_content() {
    TV.print('[');
    for (uint8_t i = 0; i < stack_ptr; ++i) {
        char num_str[5];
        itoa(stack[i], num_str, 10);
        TV.print(num_str);
        if (i < stack_ptr - 1) {
            TV.print(' ');
        }
    }
    TV.print(']');
}

// --- Вспомогательные функции ---
void playTone(int freq, int duration) {
  TV.tone(freq, duration);
  delay(duration + 5);
  TV.noTone();
}

void playWelcomeMelody() {
  playTone(NOTE_C4, MELODY_NOTE_DURATION);
  playTone(NOTE_E4, MELODY_NOTE_DURATION);
  playTone(NOTE_G4, MELODY_NOTE_DURATION);
  playTone(NOTE_C5, MELODY_NOTE_DURATION * 2);
}

void updateCursorPosition() {
  TV.set_cursor(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT);
}

void drawCursor(bool show) {
  updateCursorPosition();
  if (show) {
    TV.print('_');
  } else {
    if (cursor_x > 0 && cursor_x - 1 < buffer_len) {
      TV.print(line_buffer[cursor_x - 1]);
    } else {
      TV.print(' ');
    }
  }
}

void handleCursorBlink() {
  unsigned long current_millis = millis();
  if (current_millis - last_blink >= CURSOR_BLINK_INTERVAL) {
    flags.cursor_visible = !flags.cursor_visible;
    drawCursor(flags.cursor_visible);
    last_blink = millis();
  }
}

void clearLineBuffer() {
  memset(line_buffer, 0, MAX_LINE_LENGTH + 1);
  buffer_len = 0;
}

void redrawScreenLine(uint8_t screen_y, bool is_active) {
  TV.set_cursor(0, screen_y * CHAR_HEIGHT);
  TV.print(is_active ? ">" : " ");
  for (int i = 0; i < buffer_len && i < MAX_LINE_LENGTH; i++) {
    TV.print(line_buffer[i]);
  }
  for (int i = buffer_len; i < MAX_LINE_LENGTH; i++) {
    TV.print(' ');
  }
  if (is_active) {
    updateCursorPosition();
  }
}

// --- Функции EEPROM для программы ---
void saveLineToEEPROM(uint8_t line_idx) {
  if (line_idx >= MAX_LINES) return;
  uint16_t offset = EEPROM_BLOCK_START + line_idx * (MAX_LINE_LENGTH + 1);
  for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
    EEPROM.update(offset + i, line_buffer[i]);
  }
  EEPROM.update(offset + MAX_LINE_LENGTH, buffer_len);
}

void loadLineFromEEPROM(uint8_t line_idx) {
  if (line_idx >= MAX_LINES) {
    clearLineBuffer();
    return;
  }
  uint16_t offset = EEPROM_BLOCK_START + line_idx * (MAX_LINE_LENGTH + 1);
  for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
    line_buffer[i] = EEPROM.read(offset + i);
  }
  line_buffer[MAX_LINE_LENGTH] = 0; // Null terminate the buffer
  buffer_len = EEPROM.read(offset + MAX_LINE_LENGTH);
  if (buffer_len > MAX_LINE_LENGTH) { // Sanity check
    buffer_len = 0;
    memset(line_buffer, 0, MAX_LINE_LENGTH);
  }
}

void redrawAllLines(uint8_t active_line) {
  TV.clear_screen();
  uint8_t start_line = (active_line / LINES_PER_SCREEN) * LINES_PER_SCREEN;
  for (uint8_t i = 0; i < LINES_PER_SCREEN && (start_line + i) < MAX_LINES; i++) {
    loadLineFromEEPROM(start_line + i);
    redrawScreenLine(i, (start_line + i) == active_line);
  }
  // Reload the active line into buffer after redrawing others
  loadLineFromEEPROM(active_line);
  current_line = active_line;
  cursor_y = current_line % LINES_PER_SCREEN;
  updateCursorPosition();
}

// --- Функции EEPROM для макросов ---
// Инициализация области макросов в EEPROM
void initMacroEEPROM() {
    uint8_t magic = EEPROM.read(EEPROM_MACRO_MAGIC_ADDR);
    if (magic != EEPROM_MACRO_MAGIC_VALUE) {
        // Область не инициализирована, заполняем 0xFFFF (неопределен)
        for (uint8_t i = 0; i < MAX_MACROS; i++) {
            EEPROM.put(EEPROM_MACRO_DATA_START + i * 2, (uint16_t)0xFFFF);
        }
        // Записываем магическое число
        EEPROM.update(EEPROM_MACRO_MAGIC_ADDR, EEPROM_MACRO_MAGIC_VALUE);
    }
    // Если магическое число совпадает, предполагаем, что данные валидны
}

// Сброс определений макросов в EEPROM (заполнение 0xFFFF)
void resetMacroEEPROM() {
    for (uint8_t i = 0; i < MAX_MACROS; i++) {
        EEPROM.put(EEPROM_MACRO_DATA_START + i * 2, (uint16_t)0xFFFF);
    }
    // Магическое число оставляем
}

// Получение 16-битного адреса макроса из EEPROM
uint16_t getMacroAddress(uint8_t macro_idx) {
    if (macro_idx >= MAX_MACROS) return 0xFFFF; // Неверный индекс
    uint16_t address;
    EEPROM.get(EEPROM_MACRO_DATA_START + macro_idx * 2, address);
    return address;
}

// Установка 16-битного адреса макроса в EEPROM (используется при сканировании)
void setMacroAddress(uint8_t macro_idx, uint16_t address) {
    if (macro_idx >= MAX_MACROS) return; // Неверный индекс
    EEPROM.put(EEPROM_MACRO_DATA_START + macro_idx * 2, address);
}

// --- Сканирование программы для определения макросов ($X) ---
void scanForMacros() {
    // Сбрасываем все определения макросов перед сканированием
    resetMacroEEPROM();

    char scan_buffer[MAX_LINE_LENGTH + 1];
    uint8_t scan_buffer_len;

    for (uint8_t line_idx = 0; line_idx < MAX_LINES; line_idx++) {
        uint16_t offset = EEPROM_BLOCK_START + line_idx * (MAX_LINE_LENGTH + 1);
        // Load line directly into scan_buffer
        for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
            scan_buffer[i] = EEPROM.read(offset + i);
        }
        scan_buffer[MAX_LINE_LENGTH] = 0;
        scan_buffer_len = EEPROM.read(offset + MAX_LINE_LENGTH);
        if (scan_buffer_len > MAX_LINE_LENGTH) scan_buffer_len = 0; // Sanity check

        uint8_t scan_pc = 0;
        // Skip leading spaces
        while(scan_pc < scan_buffer_len && isspace(scan_buffer[scan_pc])) {
            scan_pc++;
        }

        // Check for $X pattern
        if (scan_pc + 1 < scan_buffer_len && scan_buffer[scan_pc] == '$') {
            char macro_letter = scan_buffer[scan_pc + 1];
            if (macro_letter >= 'A' && macro_letter <= 'Z') {
                uint8_t macro_idx = macro_letter - 'A';
                // Calculate the EEPROM address of the character *after* $X
                uint16_t macro_address = offset + scan_pc + 2;
                setMacroAddress(macro_idx, macro_address);
                // Optional: Print debug info? TV.print("Macro $"); TV.print(macro_letter); TV.print(" at "); TV.println(macro_address);
            }
        }
    }
}

// --- Загрузка тестовой программы из Flash в EEPROM ---
void loadTestProgramFromFlash(const char* test_program_P) {
    // Clear EEPROM program area
    for (uint16_t i = EEPROM_BLOCK_START; i < EEPROM_BLOCK_START + EEPROM_BLOCK_SIZE; i++) {
        EEPROM.update(i, 0);
    }
    // Reset macro definitions (scanForMacros will redefine them)
    resetMacroEEPROM();

    uint16_t flash_ptr = 0;
    uint8_t current_eeprom_line = 0;
    char temp_line_buffer[MAX_LINE_LENGTH + 1];
    uint8_t temp_buffer_ptr = 0;

    while (current_eeprom_line < MAX_LINES) {
        char c = pgm_read_byte(test_program_P + flash_ptr);
        if (c == 0) break; // End of PROGMEM string

        if (c == '\n' || temp_buffer_ptr >= MAX_LINE_LENGTH) {
            // Save the current line to EEPROM
            uint16_t eeprom_offset = EEPROM_BLOCK_START + current_eeprom_line * (MAX_LINE_LENGTH + 1);
            for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
                EEPROM.update(eeprom_offset + i, temp_line_buffer[i]);
            }
            EEPROM.update(eeprom_offset + MAX_LINE_LENGTH, temp_buffer_ptr);

            // Move to the next EEPROM line
            current_eeprom_line++;
            temp_buffer_ptr = 0;
            memset(temp_line_buffer, 0, MAX_LINE_LENGTH + 1);

            // If the character was newline, advance flash_ptr. If it was just buffer full,
            // the character that caused overflow will be the first char of the next line.
            if (c == '\n') {
                 flash_ptr++;
            }
            // If we filled the buffer exactly and the next char is newline, skip it in the next iteration
            // This prevents empty lines being created just because MAX_LINE_LENGTH ended right before \n
            if (temp_buffer_ptr == 0 && current_eeprom_line < MAX_LINES && pgm_read_byte(test_program_P + flash_ptr) == '\n') {
                 flash_ptr++;
            }


        } else {
            // Add character to the temporary buffer
            temp_line_buffer[temp_buffer_ptr++] = c;
            flash_ptr++;
        }
    }

    // Clear any remaining lines in EEPROM after the loaded program
     for (uint8_t i = current_eeprom_line; i < MAX_LINES; i++) {
        uint16_t eeprom_offset = EEPROM_BLOCK_START + i * (MAX_LINE_LENGTH + 1);
        for (uint8_t j = 0; j < MAX_LINE_LENGTH + 1; j++) {
            EEPROM.update(eeprom_offset + j, 0);
        }
    }

    showProgmemMessageAndRedraw(test_loaded_msg, 1000);
}


// --- Команды оболочки ---
void showProgmemMessageAndRedraw(const char* message_P, int duration_ms) {
  drawCursor(false);
  TV.clear_screen();
  uint8_t msg_len = strlen_P(message_P);
  TV.set_cursor(((CHARS_PER_LINE - msg_len) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2) * CHAR_HEIGHT);
  const char* ptr = message_P;
  while(char c = pgm_read_byte(ptr++)) {
      TV.print(c);
  }
  delay(duration_ms);
  clearLineBuffer();
  cursor_x = 1;
  loadLineFromEEPROM(current_line);
  cursor_x = buffer_len + 1;
  redrawAllLines(current_line);
  last_blink = millis();
  flags.cursor_visible = true;
}

void saveAllLines() {
  // Все строки уже сохраняются в EEPROM при переходе между ними или запуске RUN
  // Эта команда по сути просто подтверждает, что все в порядке
  showProgmemMessageAndRedraw(ok_msg, 500);
}

void eraseNonEmptyLines() {
  for (uint8_t i = 0; i < MAX_LINES; i++) {
    loadLineFromEEPROM(i);
    if (buffer_len > 0) {
      uint16_t offset = EEPROM_BLOCK_START + i * (MAX_LINE_LENGTH + 1);
      for (uint8_t j = 0; j < MAX_LINE_LENGTH + 1; j++) {
        EEPROM.update(offset + j, 0);
      }
    }
  }
  // Сбрасываем определения макросов в EEPROM при очистке программы
  resetMacroEEPROM();

  current_line = 0;
  cursor_y = 0;
  showProgmemMessageAndRedraw(ok_msg, 500);
}

int getFreeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

void handleFreeRamCommand() {
  int free_ram = getFreeRam();
  char ram_buffer[8];
  itoa(free_ram, ram_buffer, 10);
  strcat(ram_buffer, "b");
  drawCursor(false);
  TV.clear_screen();
  TV.set_cursor(((CHARS_PER_LINE - strlen(ram_buffer)) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2) * CHAR_HEIGHT);
  TV.print(ram_buffer);
  delay(3000);
  clearLineBuffer();
  cursor_x = 1;
  loadLineFromEEPROM(current_line);
  cursor_x = buffer_len + 1;
  redrawAllLines(current_line);
  last_blink = millis();
  flags.cursor_visible = true;
}

// --- Функция парсинга и выполнения одной команды/literala ---
// Принимает текущую позицию pc по ссылке и обновляет ее.
// Возвращает true, если команда/литерал успешно обработан.
// Возвращает false, если встречена синтаксическая ошибка.
bool executeCommand(uint8_t &pc, uint8_t line_idx) {
  if (pc >= buffer_len) return false;

  char c = line_buffer[pc];

  // 1. Игнорируем пробелы
  if (isspace(c)) {
    pc++;
    return true; // Пробел успешно обработан
  }

  // 2. Обработка строк в кавычках
  if (c == '"') {
    pc++;
    while (pc < buffer_len && line_buffer[pc] != '"') {
      //if (line_buffer[pc] == '!') TV.println(); // ! inside string is newline
      if (line_buffer[pc] == '!') TV.print('\n');
      else TV.print(line_buffer[pc]);
      pc++;
    }
    if (pc < buffer_len && line_buffer[pc] == '"') {
      pc++;
      return true; // Строка успешно обработана
    } else {
      // Непарная кавычка
      return false; // Синтаксическая ошибка
    }
  }

  // 3. Обработка чисел (включая отрицательные)
  // Проверяем, начинается ли с цифры или с минуса, за которым следует цифра
  if (isdigit(c) || (c == '-' && pc + 1 < buffer_len && isdigit(line_buffer[pc + 1]))) {
    char num_buf[5] = {0}; // Буфер для числа (достаточно для int8_t: -128\0 = 5 байт)
    uint8_t i = 0;
    uint8_t start_pc = pc; // Запоминаем начальную позицию

    // Обрабатываем знак минус
    if (c == '-') {
      num_buf[i++] = c;
      pc++;
      // Если после минуса нет цифры, это не число, а команда '-'
      if (pc >= buffer_len || !isdigit(line_buffer[pc])) {
          pc = start_pc; // Восстанавливаем pc
          // Продолжаем проверку как команду
      }
    }

    if (pc < buffer_len && isdigit(line_buffer[pc])) {
        while (pc < buffer_len && isdigit(line_buffer[pc])) {
          // Проверяем, чтобы не выйти за пределы буфера num_buf
          if (i < sizeof(num_buf) - 1) {
             num_buf[i++] = line_buffer[pc];
             pc++;
          } else {
             // Число слишком длинное для буфера - потенциальная ошибка или очень большое число
             // For simplicity, we'll just stop reading digits if buffer is full
             break; // Stop reading digits
          }
        }

        // If we read at least one digit (after potential minus)
        bool is_valid_number_start = (num_buf[0] != 0 && !(i == 1 && num_buf[0] == '-'));

        if (is_valid_number_start) {
            long value = atol(num_buf); // Используем long для чтения числа

            // Проверяем, помещается ли число в int8_t
            if (value >= -128 && value <= 127) {
                 push((int8_t)value); // Приводим к int8_t и помещаем в стек
                 return true; // Число успешно обработано
            } else {
                 // Число вне диапазона int8_t - синтаксическая ошибка
                 pc = start_pc; // Указываем на начало числа
                 return false; // Число вне диапазона
            }
        } else {
             // This was just a minus without digits, or something else
             pc = start_pc; // Restore pc
             // Continue checking as a command
        }
    } else {
        // This was just a minus without digits
        pc = start_pc; // Restore pc
        // Continue checking as a command
    }
  }

  // 4. Обработка переменных (A-Z)
  if (c >= 'A' && c <= 'Z') {
    push(c - 'A'); // Помещаем индекс переменной в стек (0 для A, 1 для B, ...)
    pc++;
    return true; // Переменная успешно обработана
  }

  // 5. Обработка команд из таблицы cmd_table
  for (uint8_t i = 0; i < sizeof(cmd_table) / sizeof(cmd_table[0]); i++) {
    if (c == pgm_read_byte(&cmd_table[i].cmd)) {
      cmd_func_t func = (cmd_func_t)pgm_read_ptr(&cmd_table[i].func);
      func();
      pc++; // Advance pc after executing command
      return true; // Команда успешно обработана
    }
  }

  // 6. Syntax error if not processed
  return false; // Неизвестный символ или синтаксическая ошибка
}

// Helper function to find the matching closing bracket
// Scans forward from start_line, start_pc
// Returns true and updates end_line, end_pc if found
// Returns false if not found (syntax error)
bool find_matching_bracket(uint8_t start_line, uint8_t start_pc, char open_char, char close_char, uint8_t &end_line, uint8_t &end_pc) {
    int nesting_level = 1;
    uint8_t current_line_idx = start_line;
    uint8_t current_pc = start_pc;

    char scan_buffer[MAX_LINE_LENGTH + 1]; // Use a local buffer for scanning
    uint8_t scan_buffer_len;

    while (current_line_idx < MAX_LINES) {
        // Load the current line into scan_buffer
        uint16_t offset = EEPROM_BLOCK_START + current_line_idx * (MAX_LINE_LENGTH + 1);
        for (uint8_t i = 0; i < MAX_LINE_LENGTH; i++) {
            scan_buffer[i] = EEPROM.read(offset + i);
        }
        scan_buffer[MAX_LINE_LENGTH] = 0;
        scan_buffer_len = EEPROM.read(offset + MAX_LINE_LENGTH);
        if (scan_buffer_len > MAX_LINE_LENGTH) scan_buffer_len = 0; // Sanity check


        // Start scanning from current_pc + 1 (or 0 if new line)
        uint8_t scan_pc = (current_line_idx == start_line) ? start_pc + 1 : 0;

        while (scan_pc < scan_buffer_len) {
            char c = scan_buffer[scan_pc];
            // Ignore comments within bracket search
            if (c == '\'') {
                scan_pc = scan_buffer_len; // Skip rest of line
                continue;
            }
            // Ignore $X markers during scan
             if (c == '$' && scan_pc + 1 < scan_buffer_len && scan_buffer[scan_pc+1] >= 'A' && scan_buffer[scan_pc+1] <= 'Z') {
                scan_pc += 2;
                continue;
            }
            // Ignore $$ during scan
             if (c == '$' && scan_pc + 1 < scan_buffer_len && scan_buffer[scan_pc+1] == '$') {
                scan_pc += 2;
                continue;
            }


            if (c == open_char) {
                nesting_level++;
            } else if (c == close_char) {
                nesting_level--;
                if (nesting_level == 0) {
                    end_line = current_line_idx;
                    end_pc = scan_pc;
                    return true; // Found matching bracket
                }
            }
            scan_pc++;
        }
        current_line_idx++; // Move to the next line
    }

    return false; // Matching bracket not found (syntax error)
}


// --- Функция выполнения программы ---
void runProgram() {
  TV.set_cursor(0, 0); // Program output starts on line 0
  stack_ptr = 0; // Сброс стека данных
  call_stack_ptr = 0; // Сброс стека вызовов (для циклов/макросов)
  memset(vars, 0, sizeof(vars)); // Сброс переменных

  flags.running = 1;
  flags.tracing = 0; // Сбрасываем флаг трассировки в начале выполнения
  flags.stack_overflow = 0; // Сбрасываем флаг переполнения стека
  bool error = false;
  uint8_t error_line = 0; // Note: Error line might not be accurate for errors inside macro calls
  const char* error_msg_P = nullptr; // Указатель на сообщение об ошибке в PROGMEM

  uint8_t exec_line_idx = 0;
  uint8_t pc = 0;

  // --- Сканирование программы для определения макросов ($X) ---
  scanForMacros();

  // Очищаем область вывода перед началом выполнения
  // (Строки 0 до LINES_PER_SCREEN - 4, т.к. 3 нижние зарезервированы для фин. стека и сообщений)
  for(uint8_t i = 0; i < LINES_PER_SCREEN - 3; ++i) { // Очищаем строки 0-12
      TV.set_cursor(0, i * CHAR_HEIGHT);
      for(int j=0; j<CHARS_PER_LINE; ++j) TV.print(' ');
  }
  TV.set_cursor(0, 0); // Возвращаем курсор в начало области вывода (строка 0)


  // Основной цикл выполнения программы
  while (exec_line_idx < MAX_LINES && !error && flags.running) {
    loadLineFromEEPROM(exec_line_idx);

    // Handle empty line
    if (buffer_len == 0) {
      exec_line_idx++;
      pc = 0;
      TV.println(); // Move to the start of the next output line
      continue;
    }

    // Process characters in the current line
    while (pc < buffer_len && !error && flags.running) {

      // --- Handle $X and $$ ---
        
      if (line_buffer[pc] == '$') {
          if (pc + 1 < buffer_len) {
              char next_char = line_buffer[pc+1];
              if (next_char == '$') {
                  flags.running = 0; // $$ Program end
                  pc += 2;
                  break; // Exit inner loop
              } else if (next_char >= 'A' && next_char <= 'Z') {
                  // $X Macro definition marker - skip the rest of the line
                  pc = buffer_len; // <-- CORRECTED: Skip to end of line
                  continue; // Go to next iteration (which will exit inner loop)
              }
          }
          // Single $ or $ followed by invalid char - ignore $
          pc++;
          continue; // Process next char in line
      }


      // --- Handle Inline Comments ---
      // If a single quote is found, skip the rest of the line
      if (line_buffer[pc] == '\'') {
          pc = buffer_len; // Move pc to the end of the buffer
          continue; // Go to the next iteration of the inner loop (which will now exit)
      }

      // Store the character *before* execution and tracing state
      char current_char = line_buffer[pc];
      bool tracing_was_on_before = flags.tracing;

      // --- Handle Control Flow and Macro Commands BEFORE executeCommand ---
      // These commands modify pc/exec_line_idx directly
      bool handled_special_command = false; // Flag if a control flow or macro command was handled

      if (current_char == '[') {
          handled_special_command = true;
          int8_t condition = pop(); // Pop condition value
          if (condition <= 0) {
              // Condition is false, skip to matching ']'
              uint8_t end_line, end_pc;
              if (find_matching_bracket(exec_line_idx, pc, '[', ']', end_line, end_pc)) {
                  exec_line_idx = end_line;
                  pc = end_pc + 1; // Set pc to the character AFTER the matching ']'
              } else {
                  error = true; // Unmatched '['
                  error_line = exec_line_idx;
                  error_msg_P = unmatched_bracket_msg;
              }
          } else {
              // Condition is true, continue execution normally
              pc++; // Move past '['
          }
      } else if (current_char == ']') {
          handled_special_command = true;
          // End of conditional block. Just continue execution.
          pc++; // Move past ']'
      } else if (current_char == '(') {
          handled_special_command = true;
          // Start of loop. Push current position onto call stack.
          if (call_stack_ptr < MAX_NESTING) {
              call_stack[call_stack_ptr][0] = exec_line_idx;
              call_stack[call_stack_ptr][1] = pc; // Store the position *of* the '('
              call_stack_ptr++;
              pc++; // Move past '('
          } else {
              error = true; // Call stack overflow
              error_line = exec_line_idx;
              error_msg_P = call_stack_overflow_msg;
          }
      } else if (current_char == ')') {
          handled_special_command = true;
          // End of loop. Pop loop start position from call stack and jump back.
          if (call_stack_ptr > 0) {
              // Assuming the top of the stack is the loop frame
              call_stack_ptr--; // Pop the return address
              exec_line_idx = call_stack[call_stack_ptr][0]; // Jump back to the line of '('
              pc = call_stack[call_stack_ptr][1]; // Jump back to the pc of '('
              // Load the target line immediately
              loadLineFromEEPROM(exec_line_idx);
          } else {
              error = true; // Unmatched ')'
              error_line = exec_line_idx;
              error_msg_P = unmatched_bracket_msg; // Or specific unmatched ')' msg
          }
      } else if (current_char == '^') {
          handled_special_command = true;
          // Conditional loop exit. Pop value.
          int8_t condition = pop();
          if (condition <= 0) {
              // Condition is false, exit the loop
              if (call_stack_ptr > 0) {
                  // Assuming the top of the stack is the loop frame
                  uint8_t loop_start_line = call_stack[call_stack_ptr-1][0];
                  uint8_t loop_start_pc = call_stack[call_stack_ptr-1][1];
                  uint8_t end_line, end_pc;
                  if (find_matching_bracket(loop_start_line, loop_start_pc, '(', ')', end_line, end_pc)) {
                       call_stack_ptr--; // Pop the loop start address (discard it)
                       exec_line_idx = end_line;
                       pc = end_pc + 1; // Jump to the character AFTER the matching ')'
                       // Load the target line immediately
                       loadLineFromEEPROM(exec_line_idx);
                  } else {
                       error = true;
                       error_line = exec_line_idx;
                       error_msg_P = unmatched_bracket_msg; // Or specific unmatched ')' msg
                  }
              } else {
                  error = true;
                  error_line = exec_line_idx;
                  error_msg_P = break_outside_loop_msg;
              }
          } else {
              // Condition is true, continue loop normally
              pc++; // Move past '^'
          }
      } else if (current_char == '#') {
          handled_special_command = true;
          if (pc + 1 < buffer_len) {
              char macro_letter = line_buffer[pc + 1];
              if (macro_letter >= 'A' && macro_letter <= 'Z') {
                  uint8_t macro_idx = macro_letter - 'A';
                  // Read the 16-bit macro address from EEPROM
                  uint16_t target_address = getMacroAddress(macro_idx);

                  if (target_address != 0xFFFF) { // 0xFFFF indicates undefined
                      if (call_stack_ptr < MAX_NESTING) {
                          // Save current position (after #X) for return
                          call_stack[call_stack_ptr][0] = exec_line_idx;
                          call_stack[call_stack_ptr][1] = pc + 2; // Position *after* #X
                          call_stack_ptr++;

                          // Calculate target line and pc from the 16-bit address
                          exec_line_idx = target_address / (MAX_LINE_LENGTH + 1);
                          pc = target_address % (MAX_LINE_LENGTH + 1);

                          // Load the target line immediately
                          loadLineFromEEPROM(exec_line_idx);
                          // The inner while loop will continue processing from the new pc
                      } else {
                          error = true;
                          error_line = exec_line_idx;
                          error_msg_P = call_stack_overflow_msg;
                      }
                  } else {
                      error = true;
                      error_line = exec_line_idx;
                      error_msg_P = undefined_macro_msg;
                  }
              } else {
                  error = true;
                  error_line = exec_line_idx;
                  error_msg_P = invalid_macro_call_msg;
              }
          } else {
              error = true;
              error_line = exec_line_idx;
              error_msg_P = invalid_macro_call_msg;
          }
           // If a special command (like # or @) was handled and no error,
           // we might have jumped lines or within the line. Continue the inner loop.
           if (error) break; // Exit inner loop on error
           if (handled_special_command) continue; // Continue inner loop from new pc (if not error)

      } else if (current_char == '@') {
          handled_special_command = true;
          if (call_stack_ptr > 0) {
              call_stack_ptr--; // Pop return address
              exec_line_idx = call_stack[call_stack_ptr][0];
              pc = call_stack[call_stack_ptr][1];
              // Load the return line immediately
              loadLineFromEEPROM(exec_line_idx);
              // The inner while loop will continue processing from the new pc
          } else {
              error = true;
              error_line = exec_line_idx;
              error_msg_P = call_stack_underflow_msg;
          }
          if (error) break; // Exit inner loop on error
          if (handled_special_command) continue; // Continue inner loop from new pc (if not error)
      }
      // Add cases for % , ; if implementing parameters (skipped for now)
      // Add case for !' if implementing char output (skipped for now)
      // Add case for ?' if implementing char input (skipped for now - ? now reads number)

      else {
          // --- Handle Other Commands using executeCommand ---
          // This includes numbers, variables, arithmetic, <, >, !, ?, {, }
          if (!executeCommand(pc, exec_line_idx)) {
            error = true;
            error_line = exec_line_idx;
            error_msg_P = syntax_error_msg;
          }
      }

      // Check stack overflow flag after *any* command execution (including push inside executeCommand or pop inside control flow)
      if (flags.stack_overflow) {
          error = true;
          error_line = exec_line_idx; // Use current line as error line
          error_msg_P = stack_overflow_msg;
          flags.running = 0; // Stop execution
      }

      // If an error occurred, break the inner loop
      if (error) break;

      // --- Trace output AFTER command execution ---
      // Trace only if flags.tracing is ON *after* the command,
      // OR if it was ON *before* and the command was '}' (to show state before turning off),
      // OR if it was OFF *before* and the command was '{' (to show state after turning on).
      // Exclude space, $, ', and ! (handled inside cmd_print_num).
      bool should_print_trace_line = (flags.tracing && !isspace(current_char) && current_char != '!' && current_char != '\'' && current_char != '$') ||
                                     (current_char == '}' && tracing_was_on_before) ||
                                     (current_char == '{' && !tracing_was_on_before);


      if (should_print_trace_line) {
          TV.println(); // Always move to a new line.
          print_stack_content(); // Prints stack

          // Print the label if needed
          if (current_char == '}') {
              TV.print(" (after })");
          } else if (current_char == '{') {
              TV.print(" (after {)");
          } else {
              // Generic label for other traced commands
              TV.print(" (after ");
              TV.print(current_char); // Print the character that was just executed
              TV.print(")");
          }
      }
      // --- End trace output ---

    } // End of inner while loop (processing characters in a line)

    // If an error occurred in the line, exit the main execution loop
    if (error) {
      break;
    }

    // If the inner loop finished without error or $$, move to next line
    // This block is reached if pc >= buffer_len (end of line)
    // OR if a special command like # or @ caused a jump (handled by 'continue' in the special command block)
    // If pc < buffer_len, it means a special command jumped within the line or to a new line,
    // and the 'continue' or 'break' handled it.
    // If pc >= buffer_len, we finished the line.
    if (flags.running && pc >= buffer_len) {
         exec_line_idx++;
         pc = 0; // Reset pc for the next line
         TV.println(); // Move to the start of the next output line for the next program line
    }
    // If flags.running is false, the outer loop will terminate.
    // If pc < buffer_len, a jump occurred, the outer loop will load the new line.

  } // End of outer while loop (processing lines)

  // --- Обработка завершения программы или ошибки ---
  flags.running = 0; // Убедимся, что флаг выполнения сброшен
  flags.tracing = 0; // Выключаем трассировку при завершении
  flags.stack_overflow = 0; // Сбрасываем флаг переполнения стека

  if (error) {
    // При ошибке очищаем весь экран и выводим сообщение об ошибке
    drawCursor(false);
    TV.clear_screen();

    TV.set_cursor(0, 15 * CHAR_HEIGHT);
    TV.print("LINE ");
    TV.print(error_line);
    TV.print(" ");
    const char* msg_ptr = error_msg_P ? error_msg_P : syntax_error_msg;
    while (char c = pgm_read_byte(msg_ptr++)) {
      TV.print(c);
    }

    delay(1500);

  } else {
    // Программа завершилась успешно (достигнут $$ или конец EEPROM)
    // Программа уже вывела свой результат (включая вывод по команде !) и трассировку на экран.
    // Теперь выводим финальное состояние стека и сообщения о завершении, НЕ очищая весь экран.

    TV.set_cursor(0, 12 * CHAR_HEIGHT);
    for(int i=0; i<CHARS_PER_LINE; ++i) TV.print(' ');
    TV.set_cursor(0, 12 * CHAR_HEIGHT);
    TV.print("Stack: ");
    print_stack_content();

    TV.set_cursor(0, 13 * CHAR_HEIGHT);
    for(int i=0; i<CHARS_PER_LINE; ++i) TV.print(' ');
    TV.set_cursor(0, 14 * CHAR_HEIGHT);
    for(int i=0; i<CHARS_PER_LINE; ++i) TV.print(' ');

    TV.set_cursor(0, 13 * CHAR_HEIGHT);
    const char* msg_ptr = program_ended_msg;
    while (char c = pgm_read_byte(msg_ptr++)) {
        TV.print(c);
    }

    TV.set_cursor(0, 14 * CHAR_HEIGHT);
    msg_ptr = press_any_key_msg;
    while (char c = pgm_read_byte(msg_ptr++)) {
        TV.print(c);
    }

    // --- Ожидание нажатия клавиши перед возвратом в редактор ---
    while (keyboard.available()) keyboard.read();
    while (!keyboard.available()) {}
    keyboard.read();
  }

  // --- Возврат в режим редактора ---
  clearLineBuffer();
  cursor_x = 1;
  current_line = 0;
  cursor_y = current_line % LINES_PER_SCREEN;
  redrawAllLines(current_line);
  last_blink = millis();
  flags.cursor_visible = true;
  flags.tracing = 0;
}


// --- Setup ---
void setup() {
  delay(500);
  TV.begin(VIDEO_SYSTEM, SCREEN_WIDTH, SCREEN_HEIGHT);
  TV.force_outstart(14);
  TV.select_font(FONT_TYPE);
  TV.clear_screen();
  TV.set_cursor(0, 0);
  TV.set_hbi_hook(keyboard.begin());

  // Инициализация области макросов в EEPROM при первом запуске
  initMacroEEPROM();

  TV.set_cursor(((CHARS_PER_LINE - 9) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2 - 2) * CHAR_HEIGHT);
  const char* ptr = welcome_msg;
  while (char c = pgm_read_byte(ptr++)) TV.print(c);
  TV.println();

  ptr = separator;
  TV.set_cursor(((CHARS_PER_LINE - 9) / 2) * CHAR_WIDTH, (LINES_PER_SCREEN / 2 - 1) * CHAR_HEIGHT);
  while (char c = pgm_read_byte(ptr++)) TV.print(c);
  TV.println();

  delay(2000);
  playWelcomeMelody();

  TV.clear_screen();
  current_line = 0;
  cursor_y = 0;
  loadLineFromEEPROM(current_line);
  cursor_x = buffer_len + 1;
  redrawAllLines(current_line);
}

// --- Loop ---
void loop() {
  if (!flags.running) {
    handleCursorBlink();
  }

  if (keyboard.available()) {
    int c = keyboard.read();
    int current_freq = TONE_FREQ_KEY;
    int current_duration = TONE_DURATION_MS_SHORT;

    if (flags.cursor_visible && c != PS2_LEFTARROW && c != PS2_RIGHTARROW && c != PS2_UPARROW && c != PS2_DOWNARROW && c != PS2_DELETE && c != PS2_HOME && c != PS2_END) {
      drawCursor(false);
      flags.cursor_visible = false;
    }

    if (c == PS2_ENTER || c == '\r' || c == '\n') {
      current_freq = TONE_FREQ_ENTER;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);

      if (strcmp(line_buffer, "SAV") == 0) {
        saveAllLines();
      } else if (strcmp(line_buffer, "ERS") == 0) {
        eraseNonEmptyLines();
        // resetMacroEEPROM() is called inside eraseNonEmptyLines()
      } else if (strcmp(line_buffer, "FREERAM") == 0) {
        handleFreeRamCommand();
      } else if (strcmp(line_buffer, "RUN") == 0) {
        saveLineToEEPROM(current_line);
        runProgram(); // scanForMacros is called inside runProgram
      } else if (strcmp(line_buffer, "TEST") == 0) { // --- Handle TEST command ---
        saveLineToEEPROM(current_line); // Save current line before loading test
        loadTestProgramFromFlash(test_program_flash); // Load test program from PROGMEM
        runProgram(); // Run the loaded test program
      }
      // Removed MAC command handling
      else {
        saveLineToEEPROM(current_line);
        clearLineBuffer();
        cursor_x = 1;
        current_line++;
        if (current_line >= MAX_LINES) {
          current_line = 0;
        }
        cursor_y = current_line % LINES_PER_SCREEN;
        loadLineFromEEPROM(current_line);
        cursor_x = buffer_len + 1;
        redrawAllLines(current_line);
      }
    } else if (c == PS2_BACKSPACE) {
      current_freq = TONE_FREQ_BACKSP;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);
      if (cursor_x > 1 && buffer_len > 0) {
        for (int i = cursor_x - 2; i < buffer_len - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }
        buffer_len--;
        line_buffer[buffer_len] = 0;
        cursor_x--;
        redrawScreenLine(cursor_y, true);
        last_blink = millis();
        flags.cursor_visible = true;
      }
    } else if (c == PS2_DELETE) {
      current_freq = TONE_FREQ_BACKSP;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);
      if (cursor_x - 1 < buffer_len) {
        for (int i = cursor_x - 1; i < buffer_len - 1; i++) {
          line_buffer[i] = line_buffer[i + 1];
        }
        buffer_len--;
        line_buffer[buffer_len] = 0;
        redrawScreenLine(cursor_y, true);
        last_blink = millis();
        flags.cursor_visible = true;
      }
    } else if (c == PS2_LEFTARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      if (cursor_x > 1) {
        cursor_x--;
        updateCursorPosition();
        last_blink = millis();
        flags.cursor_visible = true;
      }
    } else if (c == PS2_RIGHTARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      if (cursor_x < buffer_len + 1 && cursor_x < MAX_LINE_LENGTH + 1) {
        cursor_x++;
        updateCursorPosition();
        last_blink = millis();
        flags.cursor_visible = true;
      }
    } else if (c == PS2_UPARROW) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      saveLineToEEPROM(current_line);
      int desired_cursor_x = cursor_x;
      if (current_line == 0) {
        current_line = MAX_LINES - 1;
      } else {
        current_line--;
      }
      cursor_y = current_line % LINES_PER_SCREEN;
      loadLineFromEEPROM(current_line);
      cursor_x = min(desired_cursor_x, buffer_len + 1);
      if (cursor_x < 1) cursor_x = 1;
      redrawAllLines(current_line);
      last_blink = millis();
      flags.cursor_visible = true;
    } else if (c == PS2_DOWNARROW) {
      current_freq = TONE_FREQ_ENTER;
      current_duration = TONE_DURATION_MS_LONG;
      playTone(current_freq, current_duration);
      saveLineToEEPROM(current_line);
      int desired_cursor_x = cursor_x;
      current_line++;
      if (current_line >= MAX_LINES) {
        current_line = 0;
      }
      cursor_y = current_line % LINES_PER_SCREEN;
      loadLineFromEEPROM(current_line);
      cursor_x = min(desired_cursor_x, buffer_len + 1);
      if (cursor_x < 1) cursor_x = 1;
      redrawAllLines(current_line);
      last_blink = millis();
      flags.cursor_visible = true;
    } else if (c == PS2_HOME) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      cursor_x = 1;
      updateCursorPosition();
      last_blink = millis();
      flags.cursor_visible = true;
    } else if (c == PS2_END) {
      current_freq = TONE_FREQ_KEY;
      current_duration = TONE_DURATION_MS_SHORT;
      playTone(current_freq, current_duration);
      cursor_x = buffer_len + 1;
      updateCursorPosition();
      last_blink = millis();
      flags.cursor_visible = true;
    } else if (isprint(c)) {
      if (buffer_len < MAX_LINE_LENGTH) {
        current_freq = TONE_FREQ_KEY;
        current_duration = TONE_DURATION_MS_SHORT;
        playTone(current_freq, current_duration);
        for ( int i = buffer_len; i >= cursor_x - 1; i--) {
          line_buffer[i + 1] = line_buffer[i];
        }
        line_buffer[cursor_x - 1] = toupper(c);
        buffer_len++;
        cursor_x++;
        redrawScreenLine(cursor_y, true);
        last_blink = millis();
        flags.cursor_visible = true;
      } else {
        playTone(TONE_FREQ_BUFFER_FULL, TONE_DURATION_MS_SHORT);
      }
    }
  }
}
