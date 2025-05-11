#include "MouseUNO.h"

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h> // Для sprintf

Mouse::Mouse() : sp(0), pc(0), input_pos(0), current_program(nullptr), is_progmem(false), csp(0) {
  memset(data, 0, sizeof(data)); // Используем data
  memset(stack, 0, sizeof(stack));
  memset(control_stack, 0, sizeof(control_stack));
  memset(input_buffer, 0, sizeof(input_buffer));
}

void Mouse::begin(TVout &tv, PS2uartKeyboard &kb) {
  TV = &tv;
  KB = &kb;

  // Инициализация TVout
  TV->begin(VIDEO_SYSTEM, SCREEN_WIDTH, SCREEN_HEIGHT);
  TV->select_font(FONT_TYPE);
  clearScreen(); // Очистка после инициализации TVout

  // Инициализация клавиатуры
  KB->begin(); // Инициализируем клавиатуру
  // !!! ВАЖНО: Отключаем привязку клавиатуры к прерыванию TVout.
  // Это часто вызывает конфликты и приводит к отсутствию картинки.
  // TV->set_hbi_hook(KB->begin());

  // Инициализация переменных и буферов
  sp = 0; // Сброс указателя стека вычислений
  csp = 0; // Сброс указателя стека управления
  input_pos = 0; // Сброс позиции ввода
  memset(data, 0, sizeof(data)); // Используем data
  memset(stack, 0, sizeof(stack));
  memset(control_stack, 0, sizeof(control_stack));
  memset(input_buffer, 0, sizeof(input_buffer));

  showSplash(); // Показать титульный экран
}

void Mouse::showSplash() {
  clearScreen();
  TV->set_cursor(10, 10);
  TV->print("MOUSE UNO");
  TV->set_cursor(10, 30);
  TV->print("READY");
  beep(880); // Стартовый звук 1
  delay(200); // Короткая пауза
  beep(440); // Стартовый звук 2
  delay(500); // Пауза перед очисткой
  clearScreen();
}

void Mouse::beep(int freq) {
  TV->tone(freq, TONE_DURATION);
  delay(TONE_DURATION + 10); // Пауза чуть дольше тона
  TV->noTone();
}

void Mouse::clearScreen() {
  TV->clear_screen();
  TV->set_cursor(0, 0);
}

void Mouse::setPixel(int x, int y) {
  if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
    TV->set_pixel(x, y, WHITE);
  }
}

void Mouse::drawLine(int x1, int y1, int x2, int y2) {
  TV->draw_line(x1, y1, x2, y2, WHITE);
}

void Mouse::drawRect(int x, int y, int w, int h) {
  TV->draw_rect(x, y, w, h, WHITE);
}

void Mouse::push(int value) {
  if (sp < MAX_STACK) {
    stack[sp++] = value;
  } else {
    TV->print("STACK OVERFLOW!");
    beep(300);
    // В реальном интерпретаторе здесь, возможно, нужно остановить выполнение
  }
}

int Mouse::pop() {
  if (sp > 0) {
    return stack[--sp];
  } else {
    // TV->print("STACK UNDERFLOW!"); // Может быть слишком много вывода при ошибке
    // beep(300); // Сигнал об ошибке
    return 0; // Возвращаем 0 при опустошении стека
  }
}

void Mouse::pushControl(ControlFrameType type, unsigned int address) {
    if (csp < CONTROL_STACK_SIZE) {
        control_stack[csp].type = type;
        control_stack[csp].pc = address;
        csp++;
    } else {
        TV->print("CONTROL STACK OVERFLOW!");
        beep(300);
        // Остановка выполнения
    }
}

ControlFrame Mouse::popControl() {
    if (csp > 0) {
        return control_stack[--csp];
    } else {
        TV->print("CONTROL STACK UNDERFLOW!");
        beep(300);
        // Возвращаем пустой фрейм или индикатор ошибки
        return {FRAME_LOOP, 0}; // Возвращаем дефолтное значение
    }
}

ControlFrame Mouse::peekControl() {
     if (csp > 0) {
        return control_stack[csp - 1];
    } else {
        // TV->print("CONTROL STACK EMPTY!"); // Может быть слишком много вывода
        // beep(300);
        return {FRAME_LOOP, 0}; // Возвращаем дефолтное значение
    }
}


char Mouse::readCharFromProgram() {
    if (current_program && pc < (is_progmem ? strlen_P(current_program) : strlen(current_program))) {
        return is_progmem ? pgm_read_byte(current_program + pc++) : current_program[pc++];
    }
    return '\0'; // Конец программы
}

int Mouse::readNumberFromProgram() {
  int num = 0;
  bool negative = false;
  char c = is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc];

  if(c == '-') {
    negative = true;
    pc++;
    if(pc >= (is_progmem ? strlen_P(current_program) : strlen(current_program))) return 0;
    c = is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc];
  }

  while(c >= '0' && c <= '9') {
    num = num * 10 + (c - '0');
    pc++;
    if(pc >= (is_progmem ? strlen_P(current_program) : strlen(current_program))) break;
    c = is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc];
  }

  return negative ? -num : num;
}

void Mouse::skipBlock(char open_char, char close_char) {
  int cnt = 1;
  char c;
  while (current_program && pc < (is_progmem ? strlen_P(current_program) : strlen(current_program))) {
    c = readCharFromProgram(); // Используем readCharFromProgram для учета is_progmem
    if (c == open_char) cnt++;
    else if (c == close_char) cnt--;
    if (cnt == 0) break;
  }
  if (cnt != 0) {
      TV->print("UNMATCHED "); TV->print(open_char); TV->print("!");
      beep(300);
      // Остановка выполнения
  }
}


void Mouse::printString() {
  char c;
  while (current_program && pc < (is_progmem ? strlen_P(current_program) : strlen(current_program))) {
    c = readCharFromProgram(); // Используем readCharFromProgram
    if (c == '"') break;
    if (c == '!') TV->println(""); // '!' внутри строки - перевод строки
    else TV->print(c);
  }
}

int Mouse::readNumberFromKeyboard() {
  char buf[16] = {0}; // Буфер для числа (достаточно для int)
  int pos = 0;
  TV->print("> "); // Приглашение для ввода числа

  while(true) {
    if(KB->available()) {
      char c = KB->read();
      // beep(800); // Опциональный звук при нажатии клавиши

      if(c == '\r' || c == '\n') {
        buf[pos] = '\0';
        TV->println("");
        return atoi(buf); // Преобразуем буфер в число и возвращаем
      }

      if(c == '\b') {
        if(pos > 0) {
          pos--;
          buf[pos] = '\0'; // Удаляем символ из буфера
          TV->print("\b \b"); // Пытаемся стереть на экране
        }
      }
      else if(pos < sizeof(buf)-1 && (isdigit(c) || (c == '-' && pos == 0))) { // Разрешаем цифры и '-' в начале
        buf[pos++] = c;
        TV->print(c);
      }
      // Игнорируем другие символы (пробелы, буквы и т.д.)
    }
    // delay(10); // Небольшая задержка для стабильности, может влиять на TVout
  }
}

int Mouse::readCharFromKeyboard() {
    TV->print("?\' > "); // Приглашение для ввода символа
    while(true) {
        if(KB->available()) {
            char c = KB->read();
            // beep(800); // Опциональный звук
            TV->print(c); // Печатаем введенный символ
            TV->println("");
            return c; // Возвращаем ASCII-код символа
        }
        // delay(10);
    }
}


void Mouse::waitForAnyKey() {
  TV->print("PRESS ANY KEY...");
  while(!KB->available()) {
     // delay(10); // Опциональная задержка
  }
  KB->read(); // Считываем символ, чтобы очистить буфер клавиатуры
  TV->println(""); // Переход на новую строку после нажатия
}

void Mouse::printPrompt() {
  TV->print("] "); // Приглашение командной строки
}

// Читает строку из клавиатуры для интерактивного режима
void Mouse::startInteractive() {
  while(true) {
    printPrompt(); // Печатаем приглашение командной строки
    input_pos = 0; // Сбрасываем позицию в буфере ввода
    memset(input_buffer, 0, sizeof(input_buffer)); // Очищаем буфер ввода

    while(true) {
      if(KB->available()) {
        char c = KB->read();
        // beep(600); // Опциональный звук при нажатии клавиши

        // Enter - завершение ввода команды
        if(c == '\r' || c == '\n') {
          input_buffer[input_pos] = '\0'; // Завершаем строку
          TV->println(""); // Переход на новую строку
          handleInteractiveCommand(); // Обрабатываем введенную команду
          break; // Выходим из цикла ввода строки
        }

        // Backspace - удаление символа
        if(c == '\b') {
          if(input_pos > 0) {
            input_pos--;
            input_buffer[input_pos] = '\0'; // Удаляем символ из буфера
            TV->print("\b \b"); // Пытаемся стереть на экране
          }
        }
        // Печатаемые символы
        else if(input_pos < INPUT_BUFFER_SIZE-1 && c >= ' ' && c <= '~') {
          input_buffer[input_pos++] = c; // Добавляем символ в буфер
          TV->print(c); // Печатаем символ на экране
        }
        // Игнорируем другие символы
      }
      // delay(10); // Опциональная задержка
    }
  }
}

// Обрабатывает команды, введенные в интерактивном режиме
void Mouse::handleInteractiveCommand() {
  if(strcmp(input_buffer, "HELP") == 0) {
    TV->println("COMMANDS:");
    TV->println("HELP - This info");
    TV->println("RUN  - Demo program");
    TV->println("CLS  - Clear screen");
    TV->println("RESET - Restart");
    TV->println("MOUSE CMD: + - * / \\ : . ? ?' ! !' ' < = > [ ] ( ) ^ P C K ~ $$"); // Полный список команд
  }
  else if(strcmp(input_buffer, "CLS") == 0) {
    clearScreen();
  }
  else if(strcmp(input_buffer, "RUN") == 0) {
    // Демо-программа на языке Mouse (в PROGMEM)
    static const char demo_program[] PROGMEM =
      "C" // Очистить экран
      "\"MOUSE UNO DEMO\"!" // Заголовок
      "\"ENTER NUMBER:\"!" // Приглашение
      "? A :" // Читаем число с клавиатуры (команда '?'), кладем на стек. Кладем индекс A (0) на стек. Команда ':' сохраняет число в переменную A.
      "\"SQUARE:\" A. A. * !" // Получаем значение A (A.), кладем на стек. Получаем значение A еще раз (A.), кладем на стек. Умножаем (*) два верхних числа. Печатаем результат (!).
      "\" DRAWING AT \" A. 30 !" // Печатаем строку. Получаем значение A (A.), кладем на стек. Кладем число 30 на стек. Печатаем 30 (!).
      "A. 30 P" // Получаем значение A (A.), кладем на стек. Кладем 30 на стек. Команда P рисует пиксель (x y P), т.т. (A, 30).
      "K" // Ждем нажатия любой клавиши
      "C" // Очистить экран
      "$$"; // Конец программы Mouse
    runProgram(demo_program, true); // Запускаем программу из PROGMEM
  }
   else if(strcmp(input_buffer, "RESET") == 0) {
      TV->println("Resetting...");
      delay(100); // Дать время на вывод
      asm volatile ("jmp 0"); // Переход на адрес 0 (вектор сброса)
   }
  else {
    // Попытаться интерпретировать введенную строку как программу Mouse
    runProgram(input_buffer, false); // Запускаем программу из RAM (input_buffer)
  }
}

// Основной интерпретатор языка Mouse
void Mouse::runProgram(const char* program, bool progmem) {
  current_program = program;
  is_progmem = progmem;
  pc = 0; // Сброс программного счетчика
  sp = 0; // Сброс стека вычислений
  csp = 0; // Сброс стека управления

  unsigned int program_len = is_progmem ? strlen_P(current_program) : strlen(current_program);

  while(current_program && pc < program_len) {
    char c = readCharFromProgram(); // Читаем следующий символ

    // Игнорируем пробельные символы
    if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
        continue;
    }

    // Обработка однострочного комментария (~)
    if (c == '~') {
        while(pc < program_len && (is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]) != '\n') {
            pc++; // Пропускаем символы до конца строки
        }
        continue; // Продолжаем выполнение после перевода строки
    }
    // Обработка однострочного комментария (') - альтернативный синтаксис
     if (c == '\'') {
        while(pc < program_len && (is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]) != '\n') {
            pc++; // Пропускаем символы до конца строки
        }
        continue; // Продолжаем выполнение после перевода строки
    }


    // Обработка маркера конца программы $$
    if (c == '$') {
        if (pc < program_len && (is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]) == '$') {
            return; // Нашли $$ - завершаем выполнение программы
        }
        // Если это одиночный $, в этой реализации он игнорируется.
        // В оригинальном Mouse $X используется для определения макросов.
        // Пока не реализуем макросы, одиночный $ просто пропускаем.
        continue;
    }

    // Обработка чисел и переменных (букв)
    if(isdigit(c) || (c == '-' && pc < program_len && isdigit(is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]))) {
      pc--; // Уменьшаем pc, так как readNumberFromProgram ожидает начать с первого символа числа
      push(readNumberFromProgram());
      continue; // Переходим к следующему символу после чтения числа
    }
    else if(c>='A' && c<='Z') {
      push(c-'A'); // Положить индекс переменной на стек
      continue; // Переходим к следующему символу
    }


    // Обработка команд
    switch(c) {
      case '"': printString(); break; // Печать строки
      case '?': // Ввод
        {
            char next_c = pc < program_len ? (is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]) : '\0';
            if (next_c == '\'') { // ?' - ввод символа
                pc++;
                push(readCharFromKeyboard());
            } else { // ? - ввод числа
                push(readNumberFromKeyboard());
            }
        }
        break;
      case '!': // Вывод
        {
            char next_c = pc < program_len ? (is_progmem ? pgm_read_byte(current_program + pc) : current_program[pc]) : '\0';
            if (next_c == '\'') { // !' - вывод символа
                pc++;
                TV->print((char)pop()); // Выводим как символ
            } else { // ! - вывод числа
                int val = pop();
                char buf[10]; // Буфер для преобразования числа в строку
                sprintf(buf, "%d", val); // Преобразовать int в строку
                TV->print(buf); // Печатаем строку
            }
        }
        break;

      // Арифметика
      case '+': push(pop() + pop()); break; // Сложение
      case '-': { int b=pop(); push(pop()-b); } break; // Вычитание (второй со стека - первый со стека)
      case '*': push(pop() * pop()); break; // Умножение
      case '/': { int b=pop(); if(b!=0) push(pop()/b); else { TV->print("DIVIDE BY ZERO!"); beep(300); } } break; // Деление (второй со стека / первый со стека), с проверкой на 0
      case '\\': { int b=pop(); if(b!=0) push(pop()%b); else { TV->print("MODULO BY ZERO!"); beep(300); } } break; // Остаток от деления (второй со стека % первый со стека), с проверкой на 0

      // Переменные
      case ':': { int val=pop(); int idx=pop(); if(idx >= 0 && idx < MAX_VARS) data[idx]=val; else { TV->print("BAD VAR INDEX!"); beep(300); } } break; // Присваивание (значение индекс :) - ИСПРАВЛЕНО: vars -> data
      case '.': { int idx=pop(); if(idx >= 0 && idx < MAX_VARS) push(data[idx]); else { TV->print("BAD VAR INDEX!"); beep(300); push(0); } } break; // Получить значение переменной (индекс .) - ИСПРАВЛЕНО: vars -> data

      // Сравнения
      case '<': { int b=pop(); int a=pop(); push(a < b ? 1 : 0); } break; // a < b
      case '=': { int b=pop(); int a=pop(); push(a == b ? 1 : 0); } break; // a == b
      case '>': { int b=pop(); int a=pop(); push(a > b ? 1 : 0); } break; // a > b

      // Управление потоком (циклы и условия)
      case '[': // Начало условия IF
        {
            int condition = pop();
            if (condition <= 0) { // Если условие ложно (<= 0), пропускаем до ']'
                skipBlock('[', ']');
            } else { // Если условие истинно (> 0), выполняем блок, но запоминаем, что мы внутри условия
                 pushControl(FRAME_CONDITIONAL, pc); // Запоминаем позицию после '[' (для отладки или будущих расширений)
            }
        }
        break;
      case ']': // Конец условия IF
         if (csp > 0 && peekControl().type == FRAME_CONDITIONAL) {
             popControl(); // Выходим из фрейма условия
         } else {
             // Это может быть конец условия, которое было пропущено, или ошибка
             // Если стек управления пуст или верхний фрейм не условие, просто игнорируем ']'
         }
         break;
      case '(': // Начало цикла LOOP
         pushControl(FRAME_LOOP, pc); // Запоминаем позицию для возврата
         break;
      case ')': // Конец цикла LOOP
         if (csp > 0 && peekControl().type == FRAME_LOOP) {
             pc = peekControl().pc; // Возвращаемся к началу цикла
         } else {
             TV->print("UNMATCHED )!");
             beep(300);
             // Остановка выполнения
         }
         break;
      case '^': // Выход из цикла LOOP, если условие ложно (<= 0)
         {
             int condition = pop();
             if (condition <= 0) { // Если условие ложно (<= 0), выходим из цикла
                 if (csp > 0 && peekControl().type == FRAME_LOOP) {
                     popControl(); // Удаляем фрейм цикла со стека управления
                     skipBlock('(', ')'); // Пропускаем оставшуюся часть цикла до ')'
                 } else {
                     TV->print("BREAK outside LOOP!");
                     beep(300);
                     // Остановка выполнения
                 }
             }
             // Если условие истинно (> 0), просто продолжаем выполнение внутри цикла
         }
         break;

      // Графика
      case 'P': { int y=pop(); int x=pop(); setPixel(x,y); } break; // Рисовать пиксель (x y P)
      case 'L': { int y2=pop(); int x2=pop(); int y1=pop(); int x1=pop(); drawLine(x1,y1,x2,y2); } break; // Рисовать линию (x1 y1 x2 y2 L)
      case 'R': { int h=pop(); int w=pop(); int y=pop(); int x=pop(); drawRect(x,y,w,h); } break; // Рисовать прямоугольник (x y w h R)
      case 'C': clearScreen(); break; // Очистить экран

      // Добавленные команды (не из стандартного Mouse, но полезные)
      case 'K': waitForAnyKey(); break; // Ждать нажатия любой клавиши

      // Макросы (заглушки)
      case '#': // Вызов макроса
      case '@': // Выход из макроса
      case '%': // Параметр макроса
      case ',': // Разделитель параметров
      case ';': // Конец вызова макроса
         TV->print("MACRO COMMAND ("); TV->print(c); TV->print(") NOT IMPLEMENTED!");
         beep(300);
         // Пропустить блок макроса или остановить выполнение
         // Для #X; нужно пропустить до ;
         // Для $X ... @ нужно пропустить до @
         // Пока просто пропускаем символ
         break;

      // Отладка (заглушки)
      case '{': // Начало трассировки
      case '}': // Конец трассировки
         TV->print("DEBUG COMMAND ("); TV->print(c); TV->print(") NOT IMPLEMENTED!");
         beep(300);
         break;


      // Если символ не распознан
      default:
        TV->print("?UNKNOWN COMMAND: ");
        TV->print(c); // Выводим неизвестный символ
        TV->println("");
        beep(300);
        // Можно добавить логику для остановки или пропуска части программы при ошибке
        break;
    }
  }
   // Если программа завершилась без $$ (достигнут конец строки)
   // TV->println("?PROGRAM ENDED UNEXPECTEDLY"); // Опционально
   // beep(300); // Опционально
}
