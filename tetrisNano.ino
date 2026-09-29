#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <U8x8lib.h>
#include <avr/pgmspace.h>

// Arduino Nano klasik (ATmega328P), matriks FC-16 4 modul, OLED SSD1306 I2C.
// Pasang strip matriks tegak: lebar 8 LED, tinggi 32 LED.
constexpr uint8_t MATRIX_CS = 10;
constexpr uint8_t JOY_X = A0;
constexpr uint8_t JOY_Y = A1;
constexpr uint8_t JOY_SW = 2;
constexpr uint8_t BUZZER_PIN = 3;
constexpr uint8_t RESTART_BUTTON_PIN = 4;
// Modul buzzer 3 pin aktif-LOW: HIGH berarti diam. Ubah ke LOW bila modul
// Anda justru berbunyi terus saat diberi HIGH.
constexpr uint8_t BUZZER_IDLE_LEVEL = HIGH;
constexpr uint8_t WIDTH = 8;
constexpr uint8_t HEIGHT = 32;

// Ubah ke true bila arah joystick atau tampilan terbalik pada modul Anda.
constexpr bool INVERT_X = true;
constexpr bool INVERT_Y = false;
constexpr bool MIRROR_MATRIX_X = false;
constexpr bool MIRROR_MATRIX_Y = false;

MD_MAX72XX matrix(MD_MAX72XX::FC16_HW, MATRIX_CS, 4);
U8X8_SSD1306_128X64_NONAME_HW_I2C oled(U8X8_PIN_NONE);

// Setiap baris arena disimpan dalam 8 bit. Bit 0 adalah kolom paling kiri.
uint8_t board[HEIGHT];

// Pola 4x4, bit ke-(baris*4+kolom). Urutan: I, O, T, S, Z, J, L.
const uint16_t SHAPES[7] PROGMEM = {
  0x00F0, 0x0066, 0x0072, 0x0036, 0x0063, 0x0071, 0x0074
};

uint16_t pieceMask = 0;
uint8_t pieceType = 0;
int8_t pieceX = 2;
int8_t pieceY = -1;
uint32_t score = 0;
uint16_t lines = 0;
bool gameOver = false;

uint32_t lastFallMs = 0;
uint32_t lastMoveMs = 0;
int8_t previousX = 0;
int8_t previousY = 0;

struct ButtonState {
  uint32_t lastLowMs;
  bool armed;
};
ButtonState joystickButton = {0, true};
ButtonState restartButton = {0, true};

bool hudReady = false;
uint32_t shownScore = 0xFFFFFFFFUL;
uint16_t shownLines = 0xFFFF;
uint8_t shownLevel = 0xFF;
bool shownGameOver = false;

struct ToneStep {
  uint16_t frequency;
  uint8_t ticks;
};

// Melodi utama Korobeiniki dalam A minor. Satu tick adalah not seperdelapan
// pada kira-kira 144 BPM. Ini suara utama saja, bukan aransemen Game Boy penuh.
constexpr uint16_t MELODY_TICK_MS = 208;
constexpr uint16_t NOTE_GS4 = 415;
constexpr uint16_t NOTE_A4 = 440;
constexpr uint16_t NOTE_B4 = 494;
constexpr uint16_t NOTE_C5 = 523;
constexpr uint16_t NOTE_D5 = 587;
constexpr uint16_t NOTE_E5 = 659;
constexpr uint16_t NOTE_F5 = 698;
constexpr uint16_t NOTE_G5 = 784;
constexpr uint16_t NOTE_A5 = 880;

// Bagian A dimainkan dua kali sebelum bagian penghubung B.
const ToneStep MELODY_A[] PROGMEM = {
  {NOTE_E5, 2}, {NOTE_B4, 1}, {NOTE_C5, 1}, {NOTE_D5, 2},
  {NOTE_C5, 1}, {NOTE_B4, 1}, {NOTE_A4, 2},
  {NOTE_A4, 1}, {NOTE_C5, 1}, {NOTE_E5, 2}, {NOTE_D5, 1},
  {NOTE_C5, 1}, {NOTE_B4, 3}, {NOTE_C5, 1}, {NOTE_D5, 2},
  {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_A4, 2}, {NOTE_A4, 2},

  {NOTE_D5, 2}, {NOTE_F5, 1}, {NOTE_A5, 2}, {NOTE_G5, 1},
  {NOTE_F5, 1}, {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_E5, 2},
  {NOTE_D5, 1}, {NOTE_C5, 1}, {NOTE_B4, 2}, {NOTE_B4, 1},
  {NOTE_C5, 1}, {NOTE_D5, 2}, {NOTE_E5, 2}, {NOTE_C5, 2},
  {NOTE_A4, 2}, {NOTE_A4, 2}
};

const ToneStep MELODY_B[] PROGMEM = {
  {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_D5, 2}, {NOTE_B4, 2},
  {NOTE_C5, 2}, {NOTE_A4, 2}, {NOTE_GS4, 2}, {NOTE_B4, 2},
  {NOTE_E5, 2}, {NOTE_C5, 2}, {NOTE_D5, 2}, {NOTE_B4, 2},
  {NOTE_C5, 2}, {NOTE_E5, 2}, {NOTE_A4, 2}, {NOTE_GS4, 2}
};

const ToneStep CLEAR_SOUND[] PROGMEM = {
  {784, 1}, {988, 1}, {1175, 2}
};

const ToneStep GAME_OVER_SOUND[] PROGMEM = {
  {NOTE_E5, 1}, {NOTE_D5, 1}, {NOTE_C5, 1},
  {NOTE_B4, 2}, {NOTE_A4, 4}
};

enum SoundEffect : uint8_t { NO_EFFECT, LINE_CLEAR_EFFECT, GAME_OVER_EFFECT };
SoundEffect currentEffect = NO_EFFECT;
uint8_t melodyIndex = 0;
uint8_t melodySection = 0;  // 0: A, 1: A lagi, 2: B.
uint8_t effectIndex = 0;
uint32_t nextNoteMs = 0;
uint32_t noteOffMs = 0;
bool tonePlaying = false;

void stopBuzzer() {
  if (tonePlaying) noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, BUZZER_IDLE_LEVEL);
  tonePlaying = false;
}

void playEffect(SoundEffect effect) {
  stopBuzzer();
  currentEffect = effect;
  effectIndex = 0;
  nextNoteMs = millis();
}

void updateSound(uint32_t now) {
  if (tonePlaying && (int32_t)(now - noteOffMs) >= 0) stopBuzzer();
  if ((int32_t)(now - nextNoteMs) < 0) return;

  const ToneStep *notes = melodySection < 2 ? MELODY_A : MELODY_B;
  uint8_t count = melodySection < 2
    ? sizeof(MELODY_A) / sizeof(MELODY_A[0])
    : sizeof(MELODY_B) / sizeof(MELODY_B[0]);
  uint8_t *index = &melodyIndex;
  uint16_t tickMs = MELODY_TICK_MS;

  if (currentEffect != NO_EFFECT) {
    notes = currentEffect == LINE_CLEAR_EFFECT ? CLEAR_SOUND : GAME_OVER_SOUND;
    count = currentEffect == LINE_CLEAR_EFFECT
      ? sizeof(CLEAR_SOUND) / sizeof(CLEAR_SOUND[0])
      : sizeof(GAME_OVER_SOUND) / sizeof(GAME_OVER_SOUND[0]);
    index = &effectIndex;
    tickMs = currentEffect == GAME_OVER_EFFECT ? 140 : 105;
    if (*index >= count) {
      currentEffect = NO_EFFECT;
      stopBuzzer();
      nextNoteMs = now + 80;
      return;
    }
  } else if (gameOver) {
    return;  // Musik berhenti setelah efek game over selesai.
  }

  const uint16_t frequency = pgm_read_word(&notes[*index].frequency);
  const uint8_t ticks = pgm_read_byte(&notes[*index].ticks);
  const uint16_t durationMs = tickMs * ticks;
  if (frequency == 0) {
    stopBuzzer();
  } else {
    tone(BUZZER_PIN, frequency);
    tonePlaying = true;
    noteOffMs = now + durationMs * 9UL / 10UL;
  }
  nextNoteMs = now + durationMs;
  ++*index;
  if (currentEffect == NO_EFFECT && melodyIndex >= count) {
    melodyIndex = 0;
    melodySection = (melodySection + 1) % 3;
  }
}

void finishGame() {
  gameOver = true;
  playEffect(GAME_OVER_EFFECT);
}

bool hasCell(uint16_t mask, uint8_t x, uint8_t y) {
  return (mask & ((uint16_t)1 << (y * 4 + x))) != 0;
}

uint8_t levelNow() {
  uint32_t n = score / 500UL + 1UL;
  return n > 16UL ? 16 : (uint8_t)n;
}

uint16_t gravityMs() {
  return 700 - (levelNow() - 1) * 40;  // level 1: 700 ms, level 16: 100 ms
}

void showHud() {
  if (!hudReady) {
    oled.clear();
    oled.setFont(u8x8_font_chroma48medium8_r);
    oled.drawString(0, 0, "TETRIS NANO");
    shownScore = 0xFFFFFFFFUL;
    shownLines = 0xFFFF;
    shownLevel = 0xFF;
    shownGameOver = !gameOver;
    hudReady = true;
  }

  if (shownScore != score) {
    oled.clearLine(2);
    oled.setCursor(0, 2);
    oled.print(F("SKOR  : "));
    oled.print(score);
    shownScore = score;
  }
  if (shownLines != lines) {
    oled.clearLine(3);
    oled.setCursor(0, 3);
    oled.print(F("BARIS : "));
    oled.print(lines);
    shownLines = lines;
  }
  const uint8_t level = levelNow();
  if (shownLevel != level) {
    oled.clearLine(4);
    oled.setCursor(0, 4);
    oled.print(F("LEVEL : "));
    oled.print(level);
    shownLevel = level;
  }
  if (shownGameOver != gameOver) {
    oled.clearLine(6);
    oled.clearLine(7);
    if (gameOver) {
      oled.drawString(0, 6, "GAME OVER");
      oled.drawString(0, 7, "RESET/SW: ULANG");
    } else {
      oled.drawString(0, 6, "ATAS: PUTAR");
      oled.drawString(0, 7, "SW: JATUHKAN");
    }
    shownGameOver = gameOver;
  }
}

bool collides(int8_t x, int8_t y, uint16_t mask) {
  for (uint8_t dy = 0; dy < 4; ++dy) {
    for (uint8_t dx = 0; dx < 4; ++dx) {
      if (!hasCell(mask, dx, dy)) continue;
      const int8_t bx = x + dx;
      const int8_t by = y + dy;
      if (bx < 0 || bx >= WIDTH || by >= HEIGHT) return true;
      if (by >= 0 && (board[by] & (1U << bx))) return true;
    }
  }
  return false;
}

void plot(uint8_t x, uint8_t y) {
  const uint8_t row = MIRROR_MATRIX_X ? WIDTH - 1 - x : x;
  const uint8_t col = MIRROR_MATRIX_Y ? HEIGHT - 1 - y : y;
  matrix.setPoint(row, col, true);
}

void renderBoard() {
  matrix.clear();
  for (uint8_t y = 0; y < HEIGHT; ++y) {
    for (uint8_t x = 0; x < WIDTH; ++x) {
      if (board[y] & (1U << x)) plot(x, y);
    }
  }
  if (!gameOver) {
    for (uint8_t dy = 0; dy < 4; ++dy) {
      for (uint8_t dx = 0; dx < 4; ++dx) {
        if (!hasCell(pieceMask, dx, dy)) continue;
        const int8_t x = pieceX + dx;
        const int8_t y = pieceY + dy;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) plot(x, y);
      }
    }
  }
  matrix.update();
}

void spawnPiece() {
  pieceType = random(7);
  pieceMask = pgm_read_word(&SHAPES[pieceType]);
  pieceX = 2;
  pieceY = -1;
  if (collides(pieceX, pieceY, pieceMask)) finishGame();
}

uint8_t clearFullLines() {
  uint8_t cleared = 0;
  for (int8_t y = HEIGHT - 1; y >= 0; --y) {
    if (board[y] != 0xFF) continue;
    for (int8_t k = y; k > 0; --k) board[k] = board[k - 1];
    board[0] = 0;
    ++cleared;
    ++y;  // Periksa lagi posisi ini setelah baris di atas turun.
  }
  return cleared;
}

void lockPiece() {
  bool reachedTop = false;
  for (uint8_t dy = 0; dy < 4; ++dy) {
    for (uint8_t dx = 0; dx < 4; ++dx) {
      if (!hasCell(pieceMask, dx, dy)) continue;
      const int8_t x = pieceX + dx;
      const int8_t y = pieceY + dy;
      if (y < 0) reachedTop = true;
      else board[y] |= (1U << x);
    }
  }

  if (reachedTop) {
    finishGame();
  } else {
    const uint8_t count = clearFullLines();
    if (count) {
      const uint16_t bonus[5] = {0, 100, 300, 500, 800};
      lines += count;
      score += (uint32_t)bonus[count] * levelNow();
      playEffect(LINE_CLEAR_EFFECT);
    }
    spawnPiece();
  }
  lastFallMs = millis();
  showHud();
  renderBoard();
}

bool movePiece(int8_t dx, int8_t dy) {
  if (collides(pieceX + dx, pieceY + dy, pieceMask)) return false;
  pieceX += dx;
  pieceY += dy;
  renderBoard();
  return true;
}

uint16_t rotateClockwise(uint16_t mask) {
  uint16_t rotated = 0;
  for (uint8_t y = 0; y < 4; ++y) {
    for (uint8_t x = 0; x < 4; ++x) {
      if (hasCell(mask, x, y)) {
        const uint8_t nx = 3 - y;
        const uint8_t ny = x;
        rotated |= (uint16_t)1 << (ny * 4 + nx);
      }
    }
  }
  return rotated;
}

void rotatePiece() {
  if (pieceType == 1) return;  // O tidak perlu diputar.
  const uint16_t rotated = rotateClockwise(pieceMask);
  const int8_t kicks[] = {0, -1, 1, -2, 2};
  for (uint8_t i = 0; i < 5; ++i) {
    if (!collides(pieceX + kicks[i], pieceY, rotated)) {
      pieceX += kicks[i];
      pieceMask = rotated;
      renderBoard();
      return;
    }
  }
}

void hardDrop() {
  uint8_t distance = 0;
  while (!collides(pieceX, pieceY + 1, pieceMask)) {
    ++pieceY;
    ++distance;
  }
  score += (uint32_t)distance * 2;
  lockPiece();
}

void startGame() {
  stopBuzzer();
  currentEffect = NO_EFFECT;
  melodyIndex = 0;
  melodySection = 0;
  nextNoteMs = millis();
  memset(board, 0, sizeof(board));
  score = 0;
  lines = 0;
  gameOver = false;
  previousX = 0;
  previousY = 0;
  hudReady = false;
  lastFallMs = millis();
  spawnPiece();
  showHud();
  renderBoard();
}

int8_t axisDirection(int reading, bool invert) {
  int8_t direction = 0;
  if (reading < 350) direction = -1;
  else if (reading > 700) direction = 1;
  return invert ? -direction : direction;
}

// Tekanan LOW langsung diproses. Tombol baru siap lagi setelah dilepas
// selama 40 ms, sehingga bounce dan menahan tombol tidak menggandakan aksi.
bool buttonPressed(uint8_t pin, ButtonState &button, uint32_t now) {
  if (digitalRead(pin) == LOW) {
    button.lastLowMs = now;
    if (button.armed) {
      button.armed = false;
      return true;
    }
  } else if (!button.armed && now - button.lastLowMs >= 40) {
    button.armed = true;
  }
  return false;
}

void setup() {
  digitalWrite(BUZZER_PIN, BUZZER_IDLE_LEVEL);
  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(RESTART_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  randomSeed((unsigned long)analogRead(A2) ^ micros()); // A2 tidak terhubung.
  matrix.begin();
  matrix.control(MD_MAX72XX::INTENSITY, 2); // 0..15, mulai redup.
  matrix.update(MD_MAX72XX::OFF);
  oled.begin();
  startGame();
}

void loop() {
  const uint32_t now = millis();
  updateSound(now);
  const bool restartPressed =
    buttonPressed(RESTART_BUTTON_PIN, restartButton, now);
  const bool joystickPressed = buttonPressed(JOY_SW, joystickButton, now);
  if (restartPressed) {
    startGame();
    return;
  }
  if (joystickPressed) {
    if (gameOver) startGame();
    else hardDrop();
    return;
  }
  if (gameOver) return;

  const int8_t xDirection = axisDirection(analogRead(JOY_X), INVERT_X);
  const int8_t yDirection = axisDirection(analogRead(JOY_Y), INVERT_Y);

  if (xDirection != 0 &&
      (xDirection != previousX || now - lastMoveMs >= 140)) {
    movePiece(xDirection, 0);
    lastMoveMs = now;
  }
  previousX = xDirection;

  if (yDirection == -1 && previousY != -1) rotatePiece();
  previousY = yDirection;

  const uint16_t interval = yDirection == 1 ? 55 : gravityMs();
  if (now - lastFallMs >= interval) {
    if (movePiece(0, 1)) {
      if (yDirection == 1) {
        ++score;  // 1 poin per sel soft drop.
        showHud();
      }
    } else {
      lockPiece();
    }
    lastFallMs = now;
  }
}
