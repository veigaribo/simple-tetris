#include <iostream>
#include <pthread.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

const int field_width = 12;
const int field_height = 18;

unsigned char *field = nullptr;

char drawing_buffer[field_width * field_height];
struct termios orig_termios;

// left, bottom, right, rotate (z)
bool inputs[4];

wstring tetromino[7];

// y, x, c
int rotation_coefficients[4][3] = {
    {4, 1, 0}, {1, -4, 12}, {-4, -1, 15}, {-1, 4, 3}};

// rotation in degrees = r * 90
int Rotate(int px, int py, int r) {
  int *coefficients = rotation_coefficients[r % 4];

  return coefficients[0] * py + coefficients[1] * px + coefficients[2] * 1;
}

bool DoesPieceFit(int tetromino_i, int px, int py, int r) {
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int tetromino_j = Rotate(x, y, r);
      int field_i = (py + y) * field_width + (px + x);

      if (px + x >= 0 && px + x < field_width)
        if (py + y >= 0 && py + y < field_height)
          if (tetromino[tetromino_i][tetromino_j] == L'X' &&
              field[field_i] != 0)
            return false;
    }
  }

  return true;
}

// enable & disable reading the terminal byte-by-byte (instead of by line)
// https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

void DisableRawMode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }

void EnableTtyRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(DisableRawMode);

  struct termios raw = orig_termios;

  raw.c_lflag &= ~(ECHO | ICANON);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void HandleInput() {
  while (true) {
    char c;
    read(STDIN_FILENO, &c, 1);

    switch (c) {
    case 'a':
      inputs[0] = true;
      break;
    case 's':
      inputs[1] = true;
      break;
    case 'd':
      inputs[2] = true;
      break;
    case 'z':
      inputs[3] = true;
      break;
    }
  }
}

int main() {
  EnableTtyRawMode();

  // create assets
  tetromino[0].append(L"..X.");
  tetromino[0].append(L"..X.");
  tetromino[0].append(L"..X.");
  tetromino[0].append(L"..X.");

  tetromino[1].append(L"..X.");
  tetromino[1].append(L".XX.");
  tetromino[1].append(L".X..");
  tetromino[1].append(L"....");

  tetromino[2].append(L".X..");
  tetromino[2].append(L".XX.");
  tetromino[2].append(L"..X.");
  tetromino[2].append(L"....");

  tetromino[3].append(L"....");
  tetromino[3].append(L".XX.");
  tetromino[3].append(L".XX.");
  tetromino[3].append(L"....");

  tetromino[4].append(L"..X.");
  tetromino[4].append(L".XX.");
  tetromino[4].append(L"..X.");
  tetromino[4].append(L"....");

  tetromino[5].append(L".X..");
  tetromino[5].append(L".XX.");
  tetromino[5].append(L".X..");
  tetromino[5].append(L"....");

  tetromino[6].append(L"....");
  tetromino[6].append(L".XX.");
  tetromino[6].append(L"..X.");
  tetromino[6].append(L"..X.");

  tetromino[7].append(L"....");
  tetromino[7].append(L".XX.");
  tetromino[7].append(L".X..");
  tetromino[7].append(L".X..");

  // setup input handling thread
  thread input_handler(HandleInput);

  field = new unsigned char[field_width * field_height];

  for (int y = 0; y < field_height; y++) {
    for (int x = 0; x < field_width; x++) {
      field[y * field_width + x] =
          (x == 0 || x == field_width - 1 || y == field_height - 1) ? 10 : 0;
    }
  }

  bool game_over = false;

  int current_piece = 1;
  int piece_count = 0;

  int current_rotation = 0;
  int current_x = field_width / 2;
  int current_y = 0;

  bool pressed_key[4];

  int speed = 20;
  int speed_counter = 0;
  bool force_down = false;

  int score = 0;

  vector<int> lines_to_clean;

  while (!game_over) {
    // timing

    this_thread::sleep_for(50ms);
    speed_counter++;

    if (speed_counter == speed) {
      force_down = true;
      speed_counter = 0;
    }

    // input

    if (inputs[0]) {
      if (DoesPieceFit(current_piece, current_x - 1, current_y,
                       current_rotation)) {
        current_x--;
      }
    }

    if (inputs[1]) {
      if (DoesPieceFit(current_piece, current_x, current_y + 1,
                       current_rotation)) {
        current_y++;
      }
    }

    if (inputs[2]) {
      if (DoesPieceFit(current_piece, current_x + 1, current_y,
                       current_rotation)) {
        current_x++;
      }
    }

    if (inputs[3]) {
      if (DoesPieceFit(current_piece, current_x, current_y,
                       current_rotation + 1)) {
        current_rotation++;
      }
    }

    inputs[0] = inputs[1] = inputs[2] = inputs[3] = 0;

    // logic

    if (force_down) {
      if (DoesPieceFit(current_piece, current_x, current_y + 1,
                       current_rotation)) {
        current_y++;
      } else {
        // lock piece into the field
        for (int y = 0; y < 4; y++) {
          for (int x = 0; x < 4; x++) {
            auto tetromino_position = Rotate(x, y, current_rotation);
            auto current_tile = tetromino[current_piece][tetromino_position];

            if (current_tile == L'X') {
              auto field_position =
                  (current_y + y) * field_width + (current_x + x);

              field[field_position] = current_piece + 1;
            }
          }
        }

        piece_count++;
        if (piece_count % 10 == 0) {
          if (speed >= 10) {
            speed--;
          }
        }

        // check for tetris
        for (int y = 0; y < 4; y++) {
          int current_testing_y = current_y + y;

          if (current_testing_y < field_height - 1) {
            bool is_line = true;

            for (int x = 1; x < field_width; x++) {
              auto position = (current_testing_y)*field_width + x;
              is_line &= field[position] != 0;
            }

            if (is_line) {
              for (int x = 1; x < field_width - 1; x++) {
                auto position = (current_testing_y)*field_width + x;
                field[position] = 9;
              }

              lines_to_clean.push_back(current_testing_y);
            }
          }
        }

        score += 25;
        if (!lines_to_clean.empty()) {
          score += (1 << lines_to_clean.size()) * 100;
        }

        // next piece
        current_x = field_width / 2;
        current_y = 0;
        current_rotation = 0;
        current_piece = rand() % 8;

        // check for loss
        game_over = !DoesPieceFit(current_piece, current_x, current_y,
                                  current_rotation);
      }

      force_down = false;
    }

    // draw

    // walls
    for (int y = 0; y < field_height; y++) {
      for (int x = 0; x < field_width; x++) {
        auto position = y * field_width + x;
        auto current_tile = field[position];

        drawing_buffer[position] = " ABCDEFGH=#"[current_tile];
      }
    }

    // current piece
    for (int y = 0; y < 4; y++) {
      for (int x = 0; x < 4; x++) {
        auto position = Rotate(x, y, current_rotation);
        auto current_tile = tetromino[current_piece][position];

        if (current_tile == L'X') {
          auto screen_position =
              (current_y + y) * field_width + (current_x + x);

          drawing_buffer[screen_position] = " ABCDEFGH=#"[current_piece + 1];
        }
      }
    }

    cout << "\x1B[2J\x1B[H";

    printf("SCORE: %8d\n", score);

    // send the contents of the buffer to the terminal
    for (int y = 0; y < field_height; y++) {
      for (int x = 0; x < field_width; x++) {
        auto position = y * field_width + x;
        cout << drawing_buffer[position];
      }

      cout << endl;
    }

    // post-draw logic

    if (!lines_to_clean.empty()) {
      this_thread::sleep_for(400ms);

      for (auto &v : lines_to_clean) {
        for (int x = 1; x < field_width - 1; x++) {
          for (int y = v; y > 0; y--) {
            field[y * field_width + x] = field[(y - 1) * field_width + x];
          }

          field[0 + x] = 0;
        }
      }

      lines_to_clean.clear();
    }
  }

  cout << "Game Over!! Score: " << score << endl;

  pthread_cancel(input_handler.native_handle());
  input_handler.join();
}
