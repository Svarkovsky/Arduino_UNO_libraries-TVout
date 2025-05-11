#ifndef MOUSE_H
#define MOUSE_H

#include <TVout.h>
#include <PS2uartKeyboard.h>
#include <fontALL.h> // Используем fontALL для доступа к разным шрифтам
#include <avr/pgmspace.h>
#include <string.h> // Для memset, strcmp
#include <stdlib.h> // Для atoi, itoa, sprintf
#include <ctype.h>  // Для isdigit, isalpha
#include <avr/interrupt.h> // Для sei() если нужно, но обычно TVout и PS2 сами управляют

// --- Константы ---
#define VIDEO_SYSTEM NTSC // Или PAL
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 96
#define FONT_TYPE font4x6 // Выберите шрифт по умолчанию
#define MAX_STACK 32 // Увеличим стек немного
#define MAX_VARS 26  // Переменные A-Z
#define INPUT_BUFFER_SIZE 64 // Размер буфера для интерактивного ввода
#define CONTROL_STACK_SIZE 16 // Стек для управления циклами и условиями

// Звук
#define TONE_PIN 11 // Убедитесь, что это правильный пин для тона
#define TONE_DURATION 50 // Длительность тона в мс

// --- Перечисление для типов элементов на стеке управления ---
enum ControlFrameType {
    FRAME_LOOP,    // Для цикла '('
    FRAME_CONDITIONAL // Для условия '['
    // FRAME_MACRO // Для макросов (пока не реализовано)
};

// --- Структура для элементов стека управления ---
struct ControlFrame {
    ControlFrameType type;
    unsigned int pc; // Позиция в программе для возврата или пропуска
    // int offset; // Для локальных переменных макросов (пока не реализовано)
};


// --- Класс Mouse ---
class Mouse {
public:
  Mouse(); // Конструктор
  void begin(TVout &tv, PS2uartKeyboard &kb); // Инициализация
  void startInteractive(); // Запускает интерактивный режим (REPL)

private:
  TVout *TV;
  PS2uartKeyboard *KB;

  // --- Состояние интерпретатора ---
  int data[MAX_VARS]; // Переменные A-Z (Используем имя data, как в оригинале)
  int stack[MAX_STACK]; // Стек вычислений
  int sp; // Указатель стека вычислений

  ControlFrame control_stack[CONTROL_STACK_SIZE]; // Стек управления
  int csp; // Указатель стека управления

  const char *current_program; // Указатель на текущую программу (в PROGMEM или RAM)
  unsigned int pc; // Программный счетчик
  bool is_progmem; // Флаг: true если программа в PROGMEM, false если в RAM

  char input_buffer[INPUT_BUFFER_SIZE]; // Буфер для интерактивного ввода
  int input_pos; // Позиция в буфере ввода

  // --- Вспомогательные методы ---
  void push(int value); // Поместить значение на стек вычислений
  int pop(); // Извлечь значение со стека вычислений

  void pushControl(ControlFrameType type, unsigned int address); // Поместить фрейм на стек управления
  ControlFrame popControl(); // Извлечь фрейм со стека управления
  ControlFrame peekControl(); // Посмотреть верхний фрейм стека управления

  char readCharFromProgram(); // Читать следующий символ из текущей программы
  int readNumberFromProgram(); // Читать число из текущей программы
  void skipBlock(char open_char, char close_char); // Пропустить блок кода

  void printString(); // Печатать строку (команда ")
  int readNumberFromKeyboard(); // Читать число с клавиатуры (команда ?)
  int readCharFromKeyboard(); // Читать символ с клавиатуры (команда ?')
  void waitForAnyKey(); // Ждать нажатия любой клавиши (команда K)

  void beep(int freq); // Воспроизвести тон
  void clearScreen(); // Очистить экран
  void showSplash(); // Показать титульный экран
  void printPrompt(); // Печатать приглашение в интерактивном режиме

  void handleInteractiveCommand(); // Обработать команду, введенную в интерактивном режиме
  void runProgram(const char *program, bool progmem); // Запустить программу (из PROGMEM или RAM)

  // --- Графические команды (из оригинального кода) ---
  void setPixel(int x, int y);
  void drawLine(int x1, int y1, int x2, int y2);
  void drawRect(int x, int y, int w, int h);

  // --- Макросы (пока не реализованы полностью) ---
  // struct MacroDefinition { ... };
  // MacroDefinition macros[26];
  // void defineMacro(); // Команда $
  // void callMacro(); // Команда #
  // int getMacroParameter(char param_char); // Команда %
};

#endif

