#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <string.h>

// ======================================================
// Pocket-Boy Mega Pack
// Theme packs + directory-style menu + package modules
// ======================================================

// =======================
// OLED configuration
// =======================
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// =======================
// Button configuration
// =======================
const int BTN_MODE_PIN = 14;  // D5
const int BTN_B_PIN    = 12;  // D6
const int BTN_C_PIN    = 13;  // D7

const unsigned long DEBOUNCE_MS   = 30;
const unsigned long LONG_PRESS_MS = 600;

// =======================
// Enums / bitmasks
// =======================
enum ButtonBits : uint8_t {
  BTN_MODE = 1 << 0,
  BTN_B    = 1 << 1,
  BTN_C    = 1 << 2
};

enum AppState {
  APP_SPLASH,
  APP_PACK_MENU,
  APP_GAME_MENU,
  APP_RUNNING_GAME,
  APP_PAUSE_MENU
};

enum PackId : uint8_t {
  PACK_ARCADE = 0,
  PACK_ADVENTURE,
  PACK_ACTION,
  PACK_COUNT
};

enum HighScoreSlot : uint8_t {
  HS_DODGE = 0,
  HS_RUNNER,
  HS_CATCH,
  HS_TETRIS,
  HS_FLAPPY,
  HS_SIMON,
  HS_RACING,
  HS_TOWER,
  HS_INVADERS,
  HS_FROGGER,
  HS_PLATFORM,
  HS_BOULDER,
  HS_COUNT
};

struct ButtonInfo {
  bool pressed = false;
  bool released = false;
  bool held = false;
  bool shortPress = false;
  bool longPress = false;
};

struct HighScoreStore {
  uint32_t magic;
  uint32_t scores[HS_COUNT];
};

const uint32_t HIGHSCORE_MAGIC = 0x50424F5A; // "PBOZ" // "PBOY"
HighScoreStore highScoreStore;

// =======================
// Input manager
// =======================
class InputManager {
public:
  void begin() {
    pinMode(BTN_MODE_PIN, INPUT_PULLUP);
    pinMode(BTN_B_PIN, INPUT_PULLUP);
    pinMode(BTN_C_PIN, INPUT_PULLUP);
  }

  void update() {
    uint8_t currentRaw = readRaw();

    if (currentRaw != rawButtons) {
      rawButtons = currentRaw;
      lastDebounceChange = millis();
    }

    if ((millis() - lastDebounceChange) >= DEBOUNCE_MS) {
      prevButtons = stableButtons;
      stableButtons = rawButtons;
    }

    bool prevMode = (prevButtons & BTN_MODE) != 0;
    bool prevB    = (prevButtons & BTN_B)    != 0;
    bool prevC    = (prevButtons & BTN_C)    != 0;

    bool nowMode = (stableButtons & BTN_MODE) != 0;
    bool nowB    = (stableButtons & BTN_B)    != 0;
    bool nowC    = (stableButtons & BTN_C)    != 0;

    updateModeButton(nowMode, prevMode);
    updateSimpleButton(bBtn, nowB, prevB);
    updateSimpleButton(cBtn, nowC, prevC);
  }

  const ButtonInfo& mode() const { return modeBtn; }
  const ButtonInfo& b() const { return bBtn; }
  const ButtonInfo& c() const { return cBtn; }

private:
  uint8_t rawButtons = 0;
  uint8_t stableButtons = 0;
  uint8_t prevButtons = 0;
  unsigned long lastDebounceChange = 0;

  unsigned long modePressStart = 0;
  bool modeLongTriggered = false;

  ButtonInfo modeBtn;
  ButtonInfo bBtn;
  ButtonInfo cBtn;

  uint8_t readRaw() {
    uint8_t state = 0;
    if (digitalRead(BTN_MODE_PIN) == LOW) state |= BTN_MODE;
    if (digitalRead(BTN_B_PIN)    == LOW) state |= BTN_B;
    if (digitalRead(BTN_C_PIN)    == LOW) state |= BTN_C;
    return state;
  }

  void updateSimpleButton(ButtonInfo& btn, bool nowHeld, bool prevHeld) {
    btn.pressed = nowHeld && !prevHeld;
    btn.released = !nowHeld && prevHeld;
    btn.held = nowHeld;
    btn.shortPress = btn.released;
    btn.longPress = false;
  }

  void updateModeButton(bool nowHeld, bool prevHeld) {
    modeBtn.pressed = nowHeld && !prevHeld;
    modeBtn.released = !nowHeld && prevHeld;
    modeBtn.held = nowHeld;
    modeBtn.shortPress = false;
    modeBtn.longPress = false;

    if (modeBtn.pressed) {
      modePressStart = millis();
      modeLongTriggered = false;
    }

    if (nowHeld && !modeLongTriggered && (millis() - modePressStart >= LONG_PRESS_MS)) {
      modeLongTriggered = true;
      modeBtn.longPress = true;
    }

    if (modeBtn.released) {
      if (!modeLongTriggered) {
        modeBtn.shortPress = true;
      }
      modeLongTriggered = false;
    }
  }
};

// =======================
// Drawing helpers
// =======================
void drawTopBar(const char* title) {
  oled.drawRect(0, 0, 128, 10, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(3, 1);
  oled.print(title);
}

void drawFooter(const char* text) {
  oled.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  oled.setCursor(2, 56);
  oled.print(text);
}

void drawCenteredText(const char* text, int y, int size = 1) {
  int16_t x1, y1;
  uint16_t w, h;
  oled.setTextSize(size);
  oled.setTextColor(SSD1306_WHITE);
  oled.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  oled.setCursor(x, y);
  oled.print(text);
}

// =======================
// High score storage
// =======================
void saveHighScores() {
  EEPROM.put(0, highScoreStore);
  EEPROM.commit();
}

void loadHighScores() {
  EEPROM.begin(sizeof(HighScoreStore));
  EEPROM.get(0, highScoreStore);

  if (highScoreStore.magic != HIGHSCORE_MAGIC) {
    highScoreStore.magic = HIGHSCORE_MAGIC;
    for (uint8_t i = 0; i < HS_COUNT; i++) {
      highScoreStore.scores[i] = 0;
    }
    saveHighScores();
  }
}

uint32_t getHighScore(uint8_t slot) {
  if (slot >= HS_COUNT) return 0;
  return highScoreStore.scores[slot];
}

void submitHighScore(uint8_t slot, uint32_t score) {
  if (slot >= HS_COUNT) return;
  if (score > highScoreStore.scores[slot]) {
    highScoreStore.scores[slot] = score;
    saveHighScores();
  }
}

// =======================
// Game base class
// =======================
class Game {
public:
  virtual ~Game() {}
  virtual const char* name() = 0;
  virtual const char* packName() = 0;
  virtual bool implemented() { return true; }
  virtual void onEnter() = 0;
  virtual void onExit() = 0;
  virtual void handleInput(InputManager& input) = 0;
  virtual void update(uint32_t dt) = 0;
  virtual void render() = 0;
};

// =======================
// Placeholder module class
// Use this as the base location for future package modules
// =======================
class PlaceholderGame : public Game {
public:
  PlaceholderGame(const char* gameName, const char* pack, const char* hint)
    : gameName_(gameName), pack_(pack), hint_(hint) {}

  const char* name() override { return gameName_; }
  const char* packName() override { return pack_; }
  bool implemented() override { return false; }

  void onEnter() override {}
  void onExit() override {}
  void handleInput(InputManager& input) override { (void)input; }
  void update(uint32_t dt) override { (void)dt; }

  void render() override {
    oled.clearDisplay();
    drawTopBar(gameName_);
    drawCenteredText("COMING SOON", 18, 1);
    oled.drawRect(10, 28, 108, 20, SSD1306_WHITE);
    oled.setCursor(14, 34);
    oled.print(hint_);
    drawFooter("MODE back");
    oled.display();
  }

private:
  const char* gameName_;
  const char* pack_;
  const char* hint_;
};

// ======================================================
// ARCADE PACK MODULES
// Put future Arcade pack modules in this section
// ======================================================

// =======================
// Dodge
// =======================
class DodgeGame : public Game {
public:
  const char* name() override { return "Dodge"; }
  const char* packName() override { return "Arcade"; }

  void onEnter() override {
    playerX = 60;
    playerY = 48;
    playerW = 8;
    playerH = 8;
    score = 0;
    lives = 4;
    gameOver = false;
    spawnTimer = 0;
    difficultyTimer = 0;
    obstacleSpeed = 1.2f;
    spawnInterval = 700;
    frameCounter = 0;
    for (int i = 0; i < MAX_OBS; i++) obstacles[i].active = false;
  }

  void onExit() override { submitHighScore(HS_DODGE, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_DODGE, score);
        onEnter();
      }
      return;
    }
    if (input.b().held) playerX -= 5;
    if (input.c().held) playerX += 5;
    if (playerX < 0) playerX = 0;
    if (playerX > 128 - playerW) playerX = 128 - playerW;
  }

  void update(uint32_t dt) override {
    if (gameOver) return;
    spawnTimer += dt;
    difficultyTimer += dt;
    frameCounter++;

    if (difficultyTimer >= 2500) {
      difficultyTimer = 0;
      if (obstacleSpeed < 4.2f) obstacleSpeed += 0.15f;
      if (spawnInterval > 240) spawnInterval -= 25;
    }

    if (spawnTimer >= spawnInterval) {
      spawnTimer = 0;
      spawnObstacle();
    }

    for (int i = 0; i < MAX_OBS; i++) {
      if (!obstacles[i].active) continue;
      obstacles[i].y += obstacles[i].speed;

      if (checkCollision(playerX, playerY, playerW, playerH,
                         obstacles[i].x, (int)obstacles[i].y, obstacles[i].w, obstacles[i].h)) {
        obstacles[i].active = false;
        lives--;
        if (lives <= 0) {
          gameOver = true;
          submitHighScore(HS_DODGE, score);
        }
      }

      if (obstacles[i].y > 64) {
        obstacles[i].active = false;
        score++;
      }
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("DODGE");
    oled.setCursor(2, 12);
    oled.print("S:");
    oled.print(score);
    oled.setCursor(30, 12);
    oled.print("HI:");
    oled.print(getHighScore(HS_DODGE));
    oled.setCursor(92, 12);
    oled.print("L:");
    oled.print(lives);
    oled.drawLine(0, 58, 127, 58, SSD1306_WHITE);

    if (!gameOver || ((frameCounter / 6) % 2 == 0)) {
      oled.fillRect(playerX, playerY, playerW, playerH, SSD1306_WHITE);
    }

    for (int i = 0; i < MAX_OBS; i++) {
      if (obstacles[i].active) oled.fillRect(obstacles[i].x, (int)obstacles[i].y, obstacles[i].w, obstacles[i].h, SSD1306_WHITE);
    }

    if (gameOver) {
      oled.drawRect(18, 24, 92, 16, SSD1306_WHITE);
      oled.setCursor(34, 28);
      oled.print("GAME OVER");
    }

    drawFooter("Hold MODE menu");
    oled.display();
  }

private:
  struct Obstacle {
    bool active;
    int x;
    float y;
    int w;
    int h;
    float speed;
  };

  static const int MAX_OBS = 6;
  Obstacle obstacles[MAX_OBS];
  int playerX = 60, playerY = 48, playerW = 8, playerH = 8;
  int score = 0, lives = 3;
  bool gameOver = false;
  uint32_t spawnTimer = 0, difficultyTimer = 0, spawnInterval = 700, frameCounter = 0;
  float obstacleSpeed = 1.2f;

  void spawnObstacle() {
    for (int i = 0; i < MAX_OBS; i++) {
      if (obstacles[i].active) continue;
      obstacles[i].active = true;
      obstacles[i].y = 10;
      if (random(0, 5) == 0) {
        obstacles[i].w = random(16, 28);
        obstacles[i].h = 5;
        obstacles[i].x = random(0, 128 - obstacles[i].w);
        obstacles[i].speed = obstacleSpeed * 0.9f;
      } else {
        obstacles[i].w = random(5, 10);
        obstacles[i].h = random(5, 10);
        obstacles[i].x = random(0, 128 - obstacles[i].w);
        obstacles[i].speed = obstacleSpeed + random(0, 10) * 0.05f;
      }
      return;
    }
  }

  bool checkCollision(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return (ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
  }
};

// =======================
// Runner
// =======================
class RunnerGame : public Game {
public:
  const char* name() override { return "Runner"; }
  const char* packName() override { return "Arcade"; }

  void onEnter() override {
    playerX = 12;
    playerW = 8;
    playerH = 8;
    groundY = 50;
    playerY = groundY - playerH;

    velocityY = 0;
    gravity = 1;
    jumpStrength = -8;

    score = 0;
    gameOver = false;

    obstacleX = 128;
    obstacleW = 6;
    obstacleH = 8;

    speed = 2.4f;
    speedTimer = 0;
    frameCounter = 0;
    obstacleGap = random(28, 52);

    chooseNextObstacle();
  }

  void onExit() override { submitHighScore(HS_RUNNER, score / 10); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_RUNNER, score / 10);
        onEnter();
      }
      return;
    }

    if (input.mode().shortPress && isGrounded()) {
      velocityY = jumpStrength;
    }

    if (!isGrounded() && (input.b().held || input.c().held)) {
      velocityY += 1;
    }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    frameCounter++;
    speedTimer += dt;
    score++;

    if (speedTimer >= 1800) {
      speedTimer = 0;
      if (speed < 6.0f) speed += 0.15f;
    }

    velocityY += gravity;
    playerY += velocityY;

    if (playerY >= groundY - playerH) {
      playerY = groundY - playerH;
      velocityY = 0;
    }

    obstacleX -= speed;

    if (obstacleX + obstacleW < 0) {
      chooseNextObstacle();
      obstacleGap = random(24, 56);
      obstacleX = 128 + obstacleGap;
    }

    bool hitX = (obstacleX < playerX + playerW && obstacleX + obstacleW > playerX);
    bool hitY = (playerY + playerH > groundY - obstacleH);
    if (hitX && hitY) {
      gameOver = true;
      submitHighScore(HS_RUNNER, score / 10);
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("RUNNER");

    oled.setCursor(2, 12);
    oled.print("S:");
    oled.print(score / 10);

    oled.setCursor(34, 12);
    oled.print("HI:");
    oled.print(getHighScore(HS_RUNNER));

    oled.setCursor(86, 12);
    oled.print("V:");
    oled.print((int)(speed * 10));

    oled.drawLine(0, groundY, 127, groundY, SSD1306_WHITE);

    oled.fillRect(playerX, playerY, playerW, playerH, SSD1306_WHITE);
    oled.fillRect((int)obstacleX, groundY - obstacleH, obstacleW, obstacleH, SSD1306_WHITE);

    for (int i = 0; i < 5; i++) {
      int x = (frameCounter * 3 + i * 25) % 128;
      oled.drawPixel(127 - x, groundY + 3, SSD1306_WHITE);
    }

    if (gameOver) {
      oled.drawRect(18, 22, 92, 20, SSD1306_WHITE);
      oled.setCursor(34, 26);
      oled.print("GAME OVER");
      oled.setCursor(24, 35);
      oled.print("MODE restart");
    } else {
      drawFooter("MODE jump");
    }

    oled.display();
  }

private:
  int playerX = 12;
  int playerY = 42;
  int playerW = 8;
  int playerH = 8;
  int groundY = 50;

  int velocityY = 0;
  int gravity = 1;
  int jumpStrength = -8;

  float obstacleX = 128;
  int obstacleW = 6;
  int obstacleH = 8;
  int obstacleGap = 36;

  int score = 0;
  bool gameOver = false;

  float speed = 2.4f;
  uint32_t speedTimer = 0;
  uint32_t frameCounter = 0;

  bool isGrounded() {
    return playerY >= groundY - playerH;
  }

  void chooseNextObstacle() {
    int type = random(0, 6);

    switch (type) {
      case 0:
        obstacleW = 6;
        obstacleH = 8;
        break;
      case 1:
        obstacleW = 8;
        obstacleH = 10;
        break;
      case 2:
        obstacleW = 10;
        obstacleH = 6;
        break;
      case 3:
        obstacleW = 5;
        obstacleH = 14;
        break;
      case 4:
        obstacleW = 12;
        obstacleH = 8;
        break;
      default:
        obstacleW = 7;
        obstacleH = 12;
        break;
    }
  }
};

// =======================
// Catch
// =======================
class CatchGame : public Game {
public:
  const char* name() override { return "Catch"; }
  const char* packName() override { return "Puzzle"; }

  void onEnter() override {
    basketX = 56;
    itemX = random(0, 120);
    itemY = 12;
    score = 0;
    misses = 0;
    gameOver = false;
  }

  void onExit() override { submitHighScore(HS_CATCH, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_CATCH, score);
        onEnter();
      }
      return;
    }
    if (input.b().held) basketX -= 8;
    if (input.c().held) basketX += 8;
    if (basketX < 0) basketX = 0;
    if (basketX > 108) basketX = 108;
  }

  void update(uint32_t dt) override {
    (void)dt;
    if (gameOver) return;
    itemY += 2;
    if (itemY >= 48) {
      if (itemX + 4 >= basketX && itemX <= basketX + 20) {
        score++;
      } else {
        misses++;
        if (misses >= 5) {
          gameOver = true;
          submitHighScore(HS_CATCH, score);
        }
      }
      itemX = random(0, 120);
      itemY = 12;
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("CATCH");
    oled.setCursor(2, 12);
    oled.print("S:");
    oled.print(score);
    oled.setCursor(28, 12);
    oled.print("HI:");
    oled.print(getHighScore(HS_CATCH));
    oled.setCursor(96, 12);
    oled.print("M:");
    oled.print(misses);
    oled.fillRect(itemX, itemY, 4, 4, SSD1306_WHITE);
    oled.fillRect(basketX, 50, 20, 4, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(18, 24, 92, 16, SSD1306_WHITE);
      oled.setCursor(34, 28);
      oled.print("GAME OVER");
    }

    drawFooter("B/C move");
    oled.display();
  }

private:
  int basketX = 56, itemX = 50, itemY = 12;
  int score = 0, misses = 0;
  bool gameOver = false;
};

// =======================
// Tetris
// =======================
class TetrisGame : public Game {
public:
  const char* name() override { return "Tetris"; }
  const char* packName() override { return "Puzzle"; }

  void onEnter() override {
    clearBoard();
    score = 0;
    lines = 0;
    level = 1;
    gameOver = false;
    fallTimer = 0;
    fallInterval = 700;
    bRepeatTimer = 0;
    cRepeatTimer = 0;
    bHoldStarted = 0;
    cHoldStarted = 0;
    currentType = random(0, 7);
    currentRot = 0;
    currentX = 2;
    currentY = -1;
    nextType = random(0, 7);
    if (collides(currentX, currentY, currentType, currentRot)) {
      gameOver = true;
      submitHighScore(HS_TETRIS, score);
    }
  }

  void onExit() override { submitHighScore(HS_TETRIS, score); }
  void bindInput(InputManager* input) { inputRef = input; }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_TETRIS, score);
        onEnter();
      }
      return;
    }

    unsigned long now = millis();

    if (input.mode().shortPress) {
      int newRot = (currentRot + 1) & 3;
      if (!collides(currentX, currentY, currentType, newRot)) {
        currentRot = newRot;
      } else if (!collides(currentX - 1, currentY, currentType, newRot)) {
        currentX--;
        currentRot = newRot;
      } else if (!collides(currentX + 1, currentY, currentType, newRot)) {
        currentX++;
        currentRot = newRot;
      }
    }

    if (input.b().pressed) {
      moveHorizontal(-1);
      bHoldStarted = now;
      bRepeatTimer = now;
    } else if (input.b().held && !input.c().held) {
      if ((now - bHoldStarted) > 180 && (now - bRepeatTimer) > 95) {
        moveHorizontal(-1);
        bRepeatTimer = now;
      }
    }

    if (input.c().pressed) {
      moveHorizontal(1);
      cHoldStarted = now;
      cRepeatTimer = now;
    } else if (input.c().held && !input.b().held) {
      if ((now - cHoldStarted) > 180 && (now - cRepeatTimer) > 95) {
        moveHorizontal(1);
        cRepeatTimer = now;
      }
    }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;
    fallTimer += dt;
    uint32_t threshold = fallInterval;
    if (inputRef && inputRef->b().held && inputRef->c().held) threshold = 55;
    if (fallTimer >= threshold) {
      fallTimer = 0;
      stepDown();
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("TETRIS");
    oled.setCursor(40, 12);
    oled.print("S:");
    oled.print(score);
    oled.setCursor(40, 22);
    oled.print("HI:");
    oled.print(getHighScore(HS_TETRIS));
    oled.setCursor(40, 32);
    oled.print("L:");
    oled.print(level);
    oled.setCursor(40, 42);
    oled.print("Ln:");
    oled.print(lines);
    oled.setCursor(40, 52);
    oled.print("N:");
    drawBoard();
    drawNextPiece(58, 50);

    if (gameOver) {
      oled.drawRect(34, 22, 84, 18, SSD1306_WHITE);
      oled.setCursor(50, 26);
      oled.print("TOP OUT");
    }

    oled.display();
  }

private:
  static const int BW = 8, BH = 12, CELL = 4, ORIGIN_X = 2, ORIGIN_Y = 12;
  uint8_t board[BH][BW];
  int score = 0, lines = 0, level = 1;
  bool gameOver = false;
  int currentType = 0, currentRot = 0, currentX = 2, currentY = 0, nextType = 0;
  uint32_t fallTimer = 0, fallInterval = 700;
  unsigned long bRepeatTimer = 0, cRepeatTimer = 0, bHoldStarted = 0, cHoldStarted = 0;
  InputManager* inputRef = nullptr;

  const uint16_t pieceBits[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    {0x8E00, 0x6440, 0x0E20, 0x44C0},
    {0x2E00, 0x4460, 0x0E80, 0xC440},
    {0x6600, 0x6600, 0x6600, 0x6600},
    {0x6C00, 0x4620, 0x06C0, 0x8C40},
    {0x4E00, 0x4640, 0x0E40, 0x4C40},
    {0xC600, 0x2640, 0x0C60, 0x4C80}
  };

  void clearBoard() {
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) board[y][x] = 0;
  }

  bool pieceCellFilled(int type, int rot, int px, int py) {
    uint16_t bits = pieceBits[type][rot & 3];
    int bitIndex = py * 4 + px;
    return (bits & (0x8000 >> bitIndex)) != 0;
  }

  bool collides(int testX, int testY, int type, int rot) {
    for (int py = 0; py < 4; py++) {
      for (int px = 0; px < 4; px++) {
        if (!pieceCellFilled(type, rot, px, py)) continue;
        int bx = testX + px;
        int by = testY + py;
        if (bx < 0 || bx >= BW) return true;
        if (by >= BH) return true;
        if (by >= 0 && board[by][bx]) return true;
      }
    }
    return false;
  }

  void moveHorizontal(int dx) {
    if (!collides(currentX + dx, currentY, currentType, currentRot)) currentX += dx;
  }

  void stepDown() {
    if (!collides(currentX, currentY + 1, currentType, currentRot)) {
      currentY++;
      return;
    }
    lockPiece();
    clearLines();
    spawnNext();
  }

  void lockPiece() {
    for (int py = 0; py < 4; py++) {
      for (int px = 0; px < 4; px++) {
        if (!pieceCellFilled(currentType, currentRot, px, py)) continue;
        int bx = currentX + px;
        int by = currentY + py;
        if (by >= 0 && by < BH && bx >= 0 && bx < BW) board[by][bx] = 1;
      }
    }
  }

  void clearLines() {
    int cleared = 0;
    for (int y = BH - 1; y >= 0; y--) {
      bool full = true;
      for (int x = 0; x < BW; x++) if (!board[y][x]) { full = false; break; }
      if (full) {
        cleared++;
        for (int yy = y; yy > 0; yy--) for (int x = 0; x < BW; x++) board[yy][x] = board[yy - 1][x];
        for (int x = 0; x < BW; x++) board[0][x] = 0;
        y++;
      }
    }
    if (cleared > 0) {
      lines += cleared;
      switch (cleared) {
        case 1: score += 100; break;
        case 2: score += 250; break;
        case 3: score += 450; break;
        default: score += 700; break;
      }
      level = 1 + lines / 5;
      int newInterval = 700 - (level - 1) * 55;
      if (newInterval < 140) newInterval = 140;
      fallInterval = (uint32_t)newInterval;
    }
  }

  void spawnNext() {
    currentType = nextType;
    nextType = random(0, 7);
    currentRot = 0;
    currentX = 2;
    currentY = -1;
    if (collides(currentX, currentY, currentType, currentRot)) {
      gameOver = true;
      submitHighScore(HS_TETRIS, score);
    }
  }

  void drawBoard() {
    oled.drawRect(ORIGIN_X - 1, ORIGIN_Y - 1, BW * CELL + 2, BH * CELL + 2, SSD1306_WHITE);
    for (int y = 0; y < BH; y++) {
      for (int x = 0; x < BW; x++) {
        if (board[y][x]) oled.fillRect(ORIGIN_X + x * CELL, ORIGIN_Y + y * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
      }
    }
    if (!gameOver) {
      for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
          if (!pieceCellFilled(currentType, currentRot, px, py)) continue;
          int bx = currentX + px;
          int by = currentY + py;
          if (by >= 0) oled.fillRect(ORIGIN_X + bx * CELL, ORIGIN_Y + by * CELL, CELL - 1, CELL - 1, SSD1306_WHITE);
        }
      }
    }
  }

  void drawNextPiece(int ox, int oy) {
    for (int py = 0; py < 4; py++) {
      for (int px = 0; px < 4; px++) {
        if (pieceCellFilled(nextType, 0, px, py)) oled.fillRect(ox + px * 3, oy + py * 3, 2, 2, SSD1306_WHITE);
      }
    }
  }
};


class FlappyBirdGame : public Game {
public:
  const char* name() override { return "Flappy"; }
  const char* packName() override { return "Arcade"; }

  void onEnter() override {
    birdX = 28;
    birdY = 28.0f;
    velocityY = 0.0f;
    gravity = 0.19f;
    flapStrength = -1.8f;
    pipeX = 128;
    pipeW = 14;
    gapY = random(15, 28);
    gapH = 22;
    score = 0;
    passed = false;
    gameOver = false;
    tick = 0;
  }

  void onExit() override { submitHighScore(HS_FLAPPY, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_FLAPPY, score);
        onEnter();
      }
      return;
    }
    if (input.mode().shortPress) velocityY = flapStrength;
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    tick++;
    velocityY += gravity;
    if (velocityY > 2.8f) velocityY = 2.8f;
    birdY += velocityY;

    int speed = 2 + score / 5;
    if (speed > 4) speed = 4;
    pipeX -= speed;

    if (pipeX + pipeW < 0) {
      pipeX = 128 + random(6, 20);
      gapH = 22 - score / 6;
      if (gapH < 16) gapH = 16;
      gapY = random(12, 42 - gapH);
      passed = false;
    }

    if (!passed && pipeX + pipeW < birdX) {
      passed = true;
      score++;
    }

    if (birdY < 10 || birdY + 6 > 54) {
      gameOver = true;
      submitHighScore(HS_FLAPPY, score);
    }

    bool hitX = (birdX + 6 > pipeX && birdX < pipeX + pipeW);
    bool hitTop = birdY < gapY;
    bool hitBot = birdY + 6 > gapY + gapH;
    if (hitX && (hitTop || hitBot)) {
      gameOver = true;
      submitHighScore(HS_FLAPPY, score);
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("FLAPPY");
    oled.setCursor(2, 12); oled.print("S:"); oled.print(score);
    oled.setCursor(46, 12); oled.print("HI:"); oled.print(getHighScore(HS_FLAPPY));

    oled.fillRect(pipeX, 10, pipeW, gapY - 10, SSD1306_WHITE);
    oled.fillRect(pipeX, gapY + gapH, pipeW, 54 - (gapY + gapH), SSD1306_WHITE);

    int wing = (tick / 4) % 2;
    oled.fillRect(birdX, (int)birdY, 6, 6, SSD1306_WHITE);
    oled.drawLine(birdX - 2, (int)birdY + 3, birdX, (int)birdY + (wing ? 0 : 5), SSD1306_WHITE);

    oled.drawLine(0, 54, 127, 54, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(20, 22, 88, 18, SSD1306_WHITE);
      oled.setCursor(34, 26); oled.print("GAME OVER");
      oled.setCursor(26, 35); oled.print("MODE restart");
    } else {
      drawFooter("MODE flap");
    }

    oled.display();
  }

private:
  int birdX = 28;
  float birdY = 28.0f;
  float velocityY = 0.0f;
  float gravity = 0.23f;
  float flapStrength = -3.6f;
  int pipeX = 128;
  int pipeW = 14;
  int gapY = 20;
  int gapH = 22;
  int score = 0;
  int tick = 0;
  bool passed = false;
  bool gameOver = false;
};

class SimonGame : public Game {
public:
  const char* name() override { return "Simon"; }
  const char* packName() override { return "Puzzle"; }

  void onEnter() override {
    seqLen = 1;
    score = 0;
    inputIndex = 0;
    showIndex = 0;
    phase = SHOWING;
    phaseTimer = 0;
    flashButton = seq[0] = random(0, 3);
    flashOn = true;
    gameOver = false;
  }

  void onExit() override { submitHighScore(HS_SIMON, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_SIMON, score);
        onEnter();
      }
      return;
    }
    if (phase != INPUTTING) return;

    int btn = -1;
    if (input.b().shortPress) btn = 0;
    else if (input.mode().shortPress) btn = 1;
    else if (input.c().shortPress) btn = 2;

    if (btn >= 0) {
      flashButton = btn;
      flashOn = true;
      phaseTimer = 0;

      if (btn == seq[inputIndex]) {
        inputIndex++;
        if (inputIndex >= seqLen) {
          score = seqLen;
          if (seqLen < MAX_SEQ) seq[seqLen++] = random(0, 3);
          inputIndex = 0;
          showIndex = 0;
          phase = SHOWING;
          flashOn = false;
          phaseTimer = 0;
        }
      } else {
        gameOver = true;
        submitHighScore(HS_SIMON, score);
      }
    }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;
    phaseTimer += dt;

    if (phase == SHOWING) {
      if (!flashOn) {
        if (phaseTimer > 180) {
          phaseTimer = 0;
          flashOn = true;
          flashButton = seq[showIndex];
        }
      } else if (phaseTimer > 280) {
        phaseTimer = 0;
        flashOn = false;
        showIndex++;
        if (showIndex >= seqLen) {
          phase = INPUTTING;
          showIndex = 0;
          inputIndex = 0;
        }
      }
    } else if (phase == INPUTTING) {
      if (flashOn && phaseTimer > 120) flashOn = false;
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("SIMON");
    oled.setCursor(2, 12); oled.print("Rnd:"); oled.print(seqLen);
    oled.setCursor(56, 12); oled.print("HI:"); oled.print(getHighScore(HS_SIMON));

    for (int i = 0; i < 3; i++) {
      int x = 14 + i * 34;
      bool lit = flashOn && flashButton == i;
      if (lit) oled.fillRoundRect(x, 24, 24, 24, 4, SSD1306_WHITE);
      else oled.drawRoundRect(x, 24, 24, 24, 4, SSD1306_WHITE);

      oled.setTextColor(lit ? SSD1306_BLACK : SSD1306_WHITE);
      oled.setCursor(x + 9, 33);
      oled.print(i == 0 ? "B" : (i == 1 ? "M" : "C"));
      oled.setTextColor(SSD1306_WHITE);
    }

    if (gameOver) {
      oled.drawRect(20, 22, 88, 18, SSD1306_WHITE);
      oled.setCursor(34, 26); oled.print("WRONG!");
      oled.setCursor(26, 35); oled.print("MODE restart");
    } else {
      drawFooter(phase == SHOWING ? "Watch pattern" : "Repeat pattern");
    }

    oled.display();
  }

private:
  static const int MAX_SEQ = 32;
  uint8_t seq[MAX_SEQ];
  int seqLen = 1;
  int score = 0;
  int inputIndex = 0;
  int showIndex = 0;
  int flashButton = 0;
  bool flashOn = false;
  bool gameOver = false;
  uint32_t phaseTimer = 0;
  enum { SHOWING, INPUTTING } phase = SHOWING;
};

class RacingGame : public Game {
public:
  const char* name() override { return "Racing"; }
  const char* packName() override { return "Arcade"; }

  void onEnter() override {
    playerLane = 1;
    score = 0;
    speed = 1.4f;
    spawnTimer = 0;
    distanceTimer = 0;
    stripeOffset = 0;
    gameOver = false;
    lastSpawnLane = 1;
    for (int i = 0; i < MAX_CARS; i++) cars[i].active = false;
  }

  void onExit() override { submitHighScore(HS_RACING, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_RACING, score);
        onEnter();
      }
      return;
    }
    if (input.b().pressed && playerLane > 0) playerLane--;
    if (input.c().pressed && playerLane < 2) playerLane++;
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    spawnTimer += dt;
    distanceTimer += dt;
    stripeOffset += speed;

    if (distanceTimer >= 550) {
      distanceTimer = 0;
      score++;
      if (score % 8 == 0 && speed < 3.6f) speed += 0.18f;
    }

    for (int i = 0; i < MAX_CARS; i++) {
      if (!cars[i].active) continue;
      cars[i].y += cars[i].speed;

      if (cars[i].y > 56) {
        cars[i].active = false;
        continue;
      }

      if (cars[i].lane == playerLane && cars[i].y + cars[i].h >= 43 && cars[i].y <= 52) {
        gameOver = true;
        submitHighScore(HS_RACING, score);
      }
    }

    int interval = 820 - (int)(speed * 120);
    if (interval < 340) interval = 340;

    if (spawnTimer >= (uint32_t)interval) {
      spawnTimer = 0;
      spawnEnemy();
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("RACING");
    oled.setCursor(2, 12); oled.print("S:"); oled.print(score);
    oled.setCursor(46, 12); oled.print("HI:"); oled.print(getHighScore(HS_RACING));

    oled.drawRect(8, 16, 112, 38, SSD1306_WHITE);
    drawCenterDashes(45);
    drawCenterDashes(82);

    for (int i = 0; i < MAX_CARS; i++) {
      if (cars[i].active) oled.fillRect(laneX(cars[i].lane), (int)cars[i].y, 12, cars[i].h, SSD1306_WHITE);
    }

    int px = laneX(playerLane);
    oled.drawRect(px, 44, 12, 8, SSD1306_WHITE);
    oled.drawPixel(px + 5, 45, SSD1306_WHITE);
    oled.drawPixel(px + 6, 45, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(20, 22, 88, 18, SSD1306_WHITE);
      oled.setCursor(38, 26); oled.print("CRASH!");
      oled.setCursor(26, 35); oled.print("MODE restart");
    } else {
      drawFooter("B/C change lane");
    }

    oled.display();
  }

private:
  struct Car { bool active; int lane; float y; int h; float speed; };
  static const int MAX_CARS = 5;
  Car cars[MAX_CARS];
  int playerLane = 1;
  int score = 0;
  float speed = 1.4f;
  uint32_t spawnTimer = 0;
  uint32_t distanceTimer = 0;
  float stripeOffset = 0;
  int lastSpawnLane = 1;
  bool gameOver = false;

  int laneX(int lane) { return 18 + lane * 34; }

  void drawCenterDashes(int x) {
    int off = ((int)stripeOffset) % 10;
    for (int y = 18 - off; y < 52; y += 10) {
      oled.drawLine(x, y, x, y + 5, SSD1306_WHITE);
    }
  }

  void spawnEnemy() {
    for (int i = 0; i < MAX_CARS; i++) {
      if (!cars[i].active) {
        int lane = random(0, 3);
        if (lane == lastSpawnLane && random(0, 3) != 0) {
          lane = (lane + 1 + random(0, 2)) % 3;
        }
        cars[i].active = true;
        cars[i].lane = lane;
        cars[i].y = -10;
        cars[i].h = random(8, 12);
        cars[i].speed = speed + random(0, 8) * 0.08f;
        lastSpawnLane = lane;
        return;
      }
    }
  }
};

class TowerGame : public Game {
public:
  const char* name() override { return "Tower"; }
  const char* packName() override { return "Arcade"; }

  void onEnter() override {
    for (int i = 0; i < MAX_STACK; i++) {
      blockX[i] = 0;
      blockW[i] = 0;
    }

    blockX[0] = 34;
    blockW[0] = 60;
    height = 1;
    movingX = 0;
    movingW = 60;
    dir = 1;
    speed = 1;
    perfectFlash = 0;
    gameOver = false;
  }

  void onExit() override { submitHighScore(HS_TOWER, height - 1); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_TOWER, height - 1);
        onEnter();
      }
      return;
    }
    if (input.mode().shortPress) dropBlock();
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    movingX += dir * speed;
    if (movingX < 0) {
      movingX = 0;
      dir = 1;
    }
    if (movingX + movingW > 128) {
      movingX = 128 - movingW;
      dir = -1;
    }

    if (perfectFlash > 0) perfectFlash--;
  }

  void render() override {
  oled.clearDisplay();
  drawTopBar("TOWER");

  oled.setCursor(2, 12);
  oled.print("H:");
  oled.print(height - 1);

  oled.setCursor(42, 12);
  oled.print("HI:");
  oled.print(getHighScore(HS_TOWER));

  // Playfield area
  const int playTop = 14;
  const int playBottom = 53;
  const int blockStep = 4;
  const int cameraFollowY = 26;  

  // World Y for the newest placed block if the stack were fully visible
  int towerTopWorldY = 50 - (height - 1) * blockStep;

  // Camera offset: once the tower grows above the visible area,
  // shift everything downward in screen space so the top stays visible
  int cameraOffset = 0;
  if (towerTopWorldY < cameraFollowY) {
  cameraOffset = cameraFollowY - towerTopWorldY;
  }

  // Draw placed blocks
  for (int i = 0; i < height; i++) {
    int worldY = 50 - i * blockStep;
    int screenY = worldY + cameraOffset;

    // Only draw if visible in playfield
    if (screenY >= playTop && screenY <= playBottom) {
      oled.fillRect(blockX[i], screenY, blockW[i], 3, SSD1306_WHITE);
    }
  }

  // Draw moving block above the current stack
  if (!gameOver) {
    int movingWorldY = 50 - height * blockStep;
    int movingScreenY = movingWorldY + cameraOffset;

    if (movingScreenY < playTop) movingScreenY = playTop;

    if (movingScreenY <= playBottom) {
      oled.drawRect(movingX, movingScreenY, movingW, 3, SSD1306_WHITE);
    }
  }

  if (perfectFlash > 0) {
    oled.setCursor(88, 12);
    oled.print("PERF!");
  }

  if (gameOver) {
    oled.drawRect(18, 22, 92, 18, SSD1306_WHITE);
    oled.setCursor(28, 26);
    oled.print("TOWER FELL");
    oled.setCursor(24, 35);
    oled.print("MODE restart");
  } else {
    drawFooter("MODE drop");
  }

  oled.display();
}

private:
  static const int MAX_STACK = 40;
  static const int MAX_VISIBLE = 10;
  int blockX[MAX_STACK];
  int blockW[MAX_STACK];
  int height = 1;
  int movingX = 0;
  int movingW = 60;
  int dir = 1;
  int speed = 1;
  int perfectFlash = 0;
  bool gameOver = false;

  void dropBlock() {
    if (height >= MAX_STACK) {
      submitHighScore(HS_TOWER, height - 1);
      gameOver = true;
      return;
    }

    int prevX = blockX[height - 1];
    int prevW = blockW[height - 1];
    int overlapL = movingX > prevX ? movingX : prevX;
    int overlapR = (movingX + movingW) < (prevX + prevW) ? (movingX + movingW) : (prevX + prevW);
    int overlap = overlapR - overlapL;

    if (overlap <= 0) {
      gameOver = true;
      submitHighScore(HS_TOWER, height - 1);
      return;
    }

    if (overlap == prevW) {
      perfectFlash = 20;
      if (movingW < 70) movingW += 1;
    }

    blockX[height] = overlapL;
    blockW[height] = overlap;
    height++;

    movingW = overlap;
    movingX = 0;
    dir = 1;
    speed = 1 + (height / 4);
    if (speed > 4) speed = 4;
  }
};

FlappyBirdGame flappyBirdGame;
SimonGame simonGame;
RacingGame racingGame;
TowerGame towerGame;


// ======================================================
// ADVENTURE PACK MODULES
// Put future Adventure pack modules in this section
// ======================================================

// =======================
// Pocket Pet
// =======================
class PetGame : public Game {
public:
  const char* name() override { return "Pocket Pet"; }
  const char* packName() override { return "Adventure"; }

  void onEnter() override {
    reactionIndex = 0;
    blink = false;
    animTimer = 0;
    moodTimer = 0;
    setReaction(0);
  }

  void onExit() override {}

  void handleInput(InputManager& input) override {
    if (input.b().shortPress || input.c().shortPress || input.mode().shortPress) triggerRandomReaction();
  }

  void update(uint32_t dt) override {
    animTimer += dt;
    moodTimer += dt;
    if (animTimer >= 350) {
      animTimer = 0;
      blink = !blink;
    }
    if (moodTimer >= 1400 && reactionIndex != 0) setReaction(0);
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("POCKET PET");
    drawPet(64, 34, reactionIndex, blink);
    oled.drawRect(10, 50, 108, 12, SSD1306_WHITE);
    oled.setCursor(16, 52);
    oled.print(reactionText);
    drawFooter("Press any btn");
    oled.display();
  }

private:
  uint32_t animTimer = 0, moodTimer = 0;
  bool blink = false;
  int reactionIndex = 0;
  char reactionText[16] = "Hello!";

  void triggerRandomReaction() { setReaction(random(1, 6)); }

  void setReaction(int idx) {
    reactionIndex = idx;
    moodTimer = 0;
    switch (idx) {
      case 0: strcpy(reactionText, "Just chillin"); break;
      case 1: strcpy(reactionText, "Happy!"); break;
      case 2: strcpy(reactionText, "Sleepy..."); break;
      case 3: strcpy(reactionText, "Excited!!"); break;
      case 4: strcpy(reactionText, "Confused?"); break;
      case 5: strcpy(reactionText, "Grumpy!"); break;
    }
  }

  void drawPet(int cx, int cy, int mood, bool blinkState) {
    oled.fillCircle(cx, cy, 16, SSD1306_WHITE);
    oled.fillCircle(cx - 14, cy + 2, 4, SSD1306_WHITE);
    oled.fillCircle(cx + 14, cy + 2, 4, SSD1306_WHITE);
    oled.fillCircle(cx - 7, cy + 14, 3, SSD1306_WHITE);
    oled.fillCircle(cx + 7, cy + 14, 3, SSD1306_WHITE);
    oled.fillRect(cx - 11, cy - 8, 22, 16, SSD1306_BLACK);

    if (blinkState || mood == 2) {
      oled.drawLine(cx - 7, cy - 2, cx - 3, cy - 2, SSD1306_WHITE);
      oled.drawLine(cx + 3, cy - 2, cx + 7, cy - 2, SSD1306_WHITE);
    } else if (mood == 3) {
      oled.fillCircle(cx - 5, cy - 2, 3, SSD1306_WHITE);
      oled.fillCircle(cx + 5, cy - 2, 3, SSD1306_WHITE);
    } else if (mood == 4) {
      oled.drawCircle(cx - 5, cy - 2, 2, SSD1306_WHITE);
      oled.fillCircle(cx + 5, cy - 2, 2, SSD1306_WHITE);
    } else if (mood == 5) {
      oled.drawLine(cx - 8, cy - 4, cx - 3, cy - 1, SSD1306_WHITE);
      oled.drawLine(cx + 3, cy - 1, cx + 8, cy - 4, SSD1306_WHITE);
      oled.fillCircle(cx - 5, cy - 1, 1, SSD1306_WHITE);
      oled.fillCircle(cx + 5, cy - 1, 1, SSD1306_WHITE);
    } else {
      oled.fillCircle(cx - 5, cy - 2, 2, SSD1306_WHITE);
      oled.fillCircle(cx + 5, cy - 2, 2, SSD1306_WHITE);
    }

    oled.drawPixel(cx, cy + 1, SSD1306_WHITE);

    switch (mood) {
      case 1:
        oled.drawLine(cx - 4, cy + 5, cx + 4, cy + 5, SSD1306_WHITE);
        oled.drawPixel(cx - 5, cy + 4, SSD1306_WHITE);
        oled.drawPixel(cx + 5, cy + 4, SSD1306_WHITE);
        break;
      case 2:
        oled.drawLine(cx - 3, cy + 5, cx + 3, cy + 5, SSD1306_WHITE);
        break;
      case 3:
        oled.drawCircle(cx, cy + 5, 3, SSD1306_WHITE);
        break;
      case 4:
        oled.drawPixel(cx - 3, cy + 6, SSD1306_WHITE);
        oled.drawLine(cx - 2, cy + 5, cx + 2, cy + 5, SSD1306_WHITE);
        oled.drawPixel(cx + 3, cy + 4, SSD1306_WHITE);
        break;
      case 5:
        oled.drawLine(cx - 4, cy + 6, cx + 4, cy + 4, SSD1306_WHITE);
        break;
      default:
        oled.drawLine(cx - 2, cy + 5, cx + 2, cy + 5, SSD1306_WHITE);
        break;
    }
  }
};


class RpgBattleGame : public Game {
public:
  const char* name() override { return "RPG Battle"; }
  const char* packName() override { return "Adventure"; }

  void onEnter() override {
    wins = 0;
    playerHP = 20;
    level = 1;
    gameOver = false;
    selection = 0;
    message[0] = 0;
    newEnemy();
  }

  void onExit() override {}

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) onEnter();
      return;
    }
    if (input.b().pressed && selection > 0) selection--;
    if (input.c().pressed && selection < 2) selection++;
    if (input.mode().shortPress) takeTurn();
  }

  void update(uint32_t dt) override { (void)dt; }

  void render() override {
    oled.clearDisplay();
    drawTopBar("RPG BATTLE");
    oled.setCursor(2, 12); oled.print("HP:"); oled.print(playerHP);
    oled.setCursor(54, 12); oled.print("EN:"); oled.print(enemyHP);
    oled.setCursor(98, 12); oled.print("W:"); oled.print(wins);

    oled.drawRect(84, 22, 22, 18, SSD1306_WHITE);
    oled.fillCircle(95, 28, 2, SSD1306_WHITE);
    oled.fillCircle(95, 34, 2, SSD1306_WHITE);
    oled.drawLine(91, 38, 99, 38, SSD1306_WHITE);

    const char* opts[3] = {"Atk", "Heal", "Guard"};
    for (int i = 0; i < 3; i++) {
      int x = 6 + i * 38;
      if (i == selection) oled.fillRect(x, 44, 28, 9, SSD1306_WHITE);
      oled.setTextColor(i == selection ? SSD1306_BLACK : SSD1306_WHITE);
      oled.setCursor(x + 4, 46); oled.print(opts[i]);
      oled.setTextColor(SSD1306_WHITE);
    }

    oled.drawRect(2, 22, 72, 18, SSD1306_WHITE);
    oled.setCursor(6, 28); oled.print(message);

    if (gameOver) drawFooter("MODE restart");
    else drawFooter("B/C choose MODE");
    oled.display();
  }

private:
  int playerHP = 20;
  int enemyHP = 10;
  int level = 1;
  int wins = 0;
  int selection = 0;
  bool gameOver = false;
  char message[16] = "Fight!";

  void setMsg(const char* s) {
    strncpy(message, s, sizeof(message)-1);
    message[sizeof(message)-1] = 0;
  }

  void newEnemy() {
    enemyHP = 8 + level * 3;
    setMsg("A foe appears");
  }

  void takeTurn() {
    bool guarded = false;
    if (selection == 0) {
      int dmg = random(3, 6);
      enemyHP -= dmg;
      setMsg("You strike!");
    } else if (selection == 1) {
      playerHP += 4;
      if (playerHP > 20) playerHP = 20;
      setMsg("You heal");
    } else {
      guarded = true;
      setMsg("Brace...");
    }

    if (enemyHP <= 0) {
      wins++;
      level++;
      if (playerHP < 20) playerHP += 2;
      if (playerHP > 20) playerHP = 20;
      newEnemy();
      return;
    }

    int enemyDmg = random(2, 5) + level / 2;
    if (guarded) enemyDmg /= 2;
    playerHP -= enemyDmg;
    if (playerHP <= 0) {
      playerHP = 0;
      gameOver = true;
      setMsg("Defeated...");
    }
  }
};

class DecisionGame : public Game {
public:
  const char* name() override { return "Decision"; }
  const char* packName() override { return "Adventure"; }

  void onEnter() override {
    score = 0;
    step = 0;
    selected = 0;
    gameOver = false;
    nextScene();
  }

  void onExit() override {}

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) onEnter();
      return;
    }
    if (input.b().pressed) selected = 0;
    if (input.c().pressed) selected = 1;
    if (input.mode().shortPress) choose();
  }

  void update(uint32_t dt) override { (void)dt; }

  void render() override {
    oled.clearDisplay();
    drawTopBar("DECISION");
    oled.setCursor(2, 12); oled.print("Step:"); oled.print(step + 1);
    oled.setCursor(70, 12); oled.print("Luck:"); oled.print(score);

    oled.drawRect(4, 18, 120, 18, SSD1306_WHITE);
    oled.setCursor(8, 24); oled.print(prompt);

    for (int i = 0; i < 2; i++) {
      int x = 10 + i * 58;
      if (selected == i) oled.fillRect(x, 42, 48, 10, SSD1306_WHITE);
      oled.setTextColor(selected == i ? SSD1306_BLACK : SSD1306_WHITE);
      oled.setCursor(x + 6, 44); oled.print(i == 0 ? leftChoice : rightChoice);
      oled.setTextColor(SSD1306_WHITE);
    }

    if (gameOver) {
      oled.drawRect(16, 20, 96, 18, SSD1306_WHITE);
      oled.setCursor(30, 24); oled.print("OUTCOME DONE");
      drawFooter("MODE restart");
    } else {
      drawFooter("B/C choose MODE");
    }
    oled.display();
  }

private:
  int score = 0;
  int step = 0;
  int selected = 0;
  bool gameOver = false;
  char prompt[20];
  char leftChoice[10];
  char rightChoice[10];

  void setTxt(char* dst, const char* s, size_t n) {
    strncpy(dst, s, n - 1);
    dst[n - 1] = 0;
  }

  void nextScene() {
    const char* prompts[] = {"Dark cave?", "Old bridge?", "Strange trader?", "Hidden path?", "Treasure chest?"};
    const char* lefts[] = {"Enter", "Cross", "Trust", "Take", "Open"};
    const char* rights[] = {"Leave", "Wait", "Refuse", "Ignore", "Skip"};
    int idx = random(0, 5);
    setTxt(prompt, prompts[idx], sizeof(prompt));
    setTxt(leftChoice, lefts[idx], sizeof(leftChoice));
    setTxt(rightChoice, rights[idx], sizeof(rightChoice));
  }

  void choose() {
    int roll = random(0, 100);
    if (selected == 0) score += (roll > 35) ? 2 : -1;
    else score += (roll > 55) ? 1 : 0;
    if (score < 0) score = 0;
    step++;
    if (step >= 5) gameOver = true;
    else nextScene();
  }
};

class FishingGame : public Game {
public:
  const char* name() override { return "Fishing"; }
  const char* packName() override { return "Adventure"; }

  void onEnter() override {
    state = AIMING;
    boatX = 64;
    hookY = 14;
    fishX = 20;
    fishDir = 2;
    score = 0;
    nibbleTimer = 0;
    reelMeter = 0;
    gameOver = false;
  }

  void onExit() override {}

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) onEnter();
      return;
    }

    if (state == AIMING) {
      if (input.b().held && boatX > 16) boatX -= 2;
      if (input.c().held && boatX < 112) boatX += 2;
      if (input.mode().shortPress) {
        state = DROPPING;
        hookY = 14;
      }
    } else if (state == HOOKED) {
      if (input.mode().shortPress) reelMeter += 2;
      if (reelMeter >= 12) {
        score++;
        state = AIMING;
        reelMeter = 0;
      }
    }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    fishX += fishDir;
    if (fishX < 16 || fishX > 112) fishDir = -fishDir;

    if (state == DROPPING) {
      hookY += 2;
      if (abs((boatX - 2) - fishX) < 8 && hookY > 38) {
        state = HOOKED;
        reelMeter = 0;
      } else if (hookY > 50) {
        state = AIMING;
      }
    } else if (state == HOOKED) {
      nibbleTimer += dt;
      if (nibbleTimer > 250) {
        nibbleTimer = 0;
        reelMeter--;
        if (reelMeter < 0) {
          state = AIMING;
          reelMeter = 0;
        }
      }
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("FISHING");
    oled.setCursor(2, 12); oled.print("Catch:"); oled.print(score);

    oled.drawLine(0, 18, 127, 18, SSD1306_WHITE);
    oled.fillRect(boatX - 6, 10, 12, 4, SSD1306_WHITE);
    oled.drawLine(boatX, 14, boatX, hookY, SSD1306_WHITE);
    if (state != AIMING) oled.drawCircle(boatX, hookY, 1, SSD1306_WHITE);

    oled.fillTriangle(fishX, 42, fishX - 5, 39, fishX - 5, 45, SSD1306_WHITE);
    oled.fillRect(fishX, 40, 6, 4, SSD1306_WHITE);

    if (state == HOOKED) {
      oled.drawRect(84, 44, 30, 8, SSD1306_WHITE);
      oled.fillRect(86, 46, reelMeter * 2, 4, SSD1306_WHITE);
      oled.setCursor(82, 34); oled.print("REEL!");
    } else {
      drawFooter("Move + MODE cast");
    }
    oled.display();
  }

private:
  enum { AIMING, DROPPING, HOOKED } state = AIMING;
  int boatX = 64;
  int hookY = 14;
  int fishX = 20;
  int fishDir = 2;
  int score = 0;
  int reelMeter = 0;
  uint32_t nibbleTimer = 0;
  bool gameOver = false;
};

RpgBattleGame rpgGame;


// ======================================================
// ACTION PACK MODULES
// Put future Action pack modules in this section
// ======================================================

class InvadersGame : public Game {
public:
  const char* name() override { return "Invaders"; }
  const char* packName() override { return "Action"; }

  void onEnter() override {
    playerX = 58;
    bulletActive = false;
    enemyBulletActive = false;
    alienDir = 1;
    alienStepTimer = 0;
    enemyShotTimer = 0;
    wave = 1;
    score = 0;
    lives = 5;
    gameOver = false;
    resetWave();
  }

  void onExit() override { submitHighScore(HS_INVADERS, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_INVADERS, score);
        onEnter();
      }
      return;
    }

    if (input.b().held && playerX > 4) playerX -= 2;
    if (input.c().held && playerX < 114) playerX += 2;

    if (input.mode().shortPress && !bulletActive) {
      bulletActive = true;
      bulletX = playerX + 4;
      bulletY = 46;
    }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    alienStepTimer += dt;
    enemyShotTimer += dt;

    int stepInterval = 460 - (wave - 1) * 35;
    if (stepInterval < 220) stepInterval = 220;

    if (alienStepTimer >= (uint32_t)stepInterval) {
      alienStepTimer = 0;

      alienBaseX += alienDir * 3;

      if (alienBaseX < 10 || alienBaseX > 52) {
        alienDir = -alienDir;
        alienBaseX += alienDir * 3;
        alienY += 3;
      }

      if (alienY > 38) {
        gameOver = true;
        submitHighScore(HS_INVADERS, score);
      }
    }

    if (bulletActive) {
      bulletY -= 4;
      if (bulletY < 10) bulletActive = false;

      for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
          if (!aliens[r][c]) continue;

          int ax = alienBaseX + c * 22;
          int ay = alienY + r * 10;

          if (bulletX >= ax && bulletX <= ax + 10 &&
              bulletY >= ay && bulletY <= ay + 6) {
            aliens[r][c] = false;
            bulletActive = false;
            score += 10;
          }
        }
      }
    }

    if (!enemyBulletActive && enemyShotTimer > 950) {
      enemyShotTimer = 0;
      spawnEnemyBullet();
    }

    if (enemyBulletActive) {
      enemyBulletY += 3;
      if (enemyBulletY > 54) enemyBulletActive = false;

      if (enemyBulletX >= playerX && enemyBulletX <= playerX + 10 &&
          enemyBulletY >= 48) {
        enemyBulletActive = false;
        lives--;

        if (lives <= 0) {
          gameOver = true;
          submitHighScore(HS_INVADERS, score);
        }
      }
    }

    if (aliveCount() == 0) {
      wave++;
      score += 25;
      resetWave();
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("INVADERS");
    oled.setCursor(2, 12);
    oled.print("S:");
    oled.print(score);
    oled.setCursor(42, 12);
    oled.print("HI:");
    oled.print(getHighScore(HS_INVADERS));
    oled.setCursor(94, 12);
    oled.print("L:");
    oled.print(lives);

    for (int r = 0; r < ALIEN_ROWS; r++) {
      for (int c = 0; c < ALIEN_COLS; c++) {
        if (aliens[r][c]) {
          oled.drawRect(alienBaseX + c * 22, alienY + r * 10, 10, 6, SSD1306_WHITE);
        }
      }
    }

    if (bulletActive) {
      oled.drawLine(bulletX, bulletY, bulletX, bulletY - 3, SSD1306_WHITE);
    }

    if (enemyBulletActive) {
      oled.drawLine(enemyBulletX, enemyBulletY, enemyBulletX, enemyBulletY + 3, SSD1306_WHITE);
    }

    oled.fillRect(playerX, 48, 10, 4, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(18, 22, 92, 18, SSD1306_WHITE);
      oled.setCursor(36, 26);
      oled.print("DEFEAT");
      oled.setCursor(26, 35);
      oled.print("MODE restart");
    } else {
      drawFooter("B/C move MODE fire");
    }

    oled.display();
  }

private:
  static const int ALIEN_ROWS = 2;
  static const int ALIEN_COLS = 4;

  bool aliens[ALIEN_ROWS][ALIEN_COLS];
  int playerX = 56;

  bool bulletActive = false;
  int bulletX = 0;
  int bulletY = 0;

  bool enemyBulletActive = false;
  int enemyBulletX = 0;
  int enemyBulletY = 0;

  int alienBaseX = 22;
  int alienY = 16;
  int alienDir = 1;

  uint32_t alienStepTimer = 0;
  uint32_t enemyShotTimer = 0;

  int wave = 1;
  int score = 0;
  int lives = 5;
  bool gameOver = false;

  void resetWave() {
    for (int r = 0; r < ALIEN_ROWS; r++) {
      for (int c = 0; c < ALIEN_COLS; c++) {
        aliens[r][c] = true;
      }
    }

    alienBaseX = 22;
    alienY = 16;
    alienDir = 1;
    bulletActive = false;
    enemyBulletActive = false;
  }

  int aliveCount() {
    int alive = 0;

    for (int r = 0; r < ALIEN_ROWS; r++) {
      for (int c = 0; c < ALIEN_COLS; c++) {
        if (aliens[r][c]) alive++;
      }
    }

    return alive;
  }

  void spawnEnemyBullet() {
    int choices[ALIEN_COLS];
    int choiceCount = 0;

    for (int c = 0; c < ALIEN_COLS; c++) {
      for (int r = ALIEN_ROWS - 1; r >= 0; r--) {
        if (aliens[r][c]) {
          choices[choiceCount++] = c * 10 + r;
          break;
        }
      }
    }

    if (choiceCount == 0) return;

    int pick = choices[random(0, choiceCount)];
    int c = pick / 10;
    int r = pick % 10;

    enemyBulletActive = true;
    enemyBulletX = alienBaseX + c * 22 + 5;
    enemyBulletY = alienY + r * 10 + 6;
  }
};

class FroggerGame : public Game {
public:
  const char* name() override { return "Frogger"; }
  const char* packName() override { return "Action"; }

  void onEnter() override {
    row = 4;
    x = 60;
    score = 0;
    level = 1;
    moveCooldown = 0;
    gameOver = false;
    initCars();
  }

  void onExit() override { submitHighScore(HS_FROGGER, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_FROGGER, score);
        onEnter();
      }
      return;
    }

    if (moveCooldown > 0) return;

    if (input.b().pressed && x > 4) { x -= 14; moveCooldown = 120; }
    if (input.c().pressed && x < 118) { x += 14; moveCooldown = 120; }
    if (input.mode().shortPress && row > 0) { row--; moveCooldown = 140; }
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    if (moveCooldown > dt) moveCooldown -= dt;
    else moveCooldown = 0;

    for (int i = 0; i < 3; i++) {
      cars1[i] += speed1;
      cars2[i] += speed2;
      cars3[i] += speed3;
      cars4[i] += speed4;
      wrap(cars1[i], 16);
      wrap(cars2[i], 14);
      wrap(cars3[i], 18);
      wrap(cars4[i], 12);
    }

    if ((row == 1 && hitCars(cars1, 16)) ||
        (row == 2 && hitCars(cars2, 14)) ||
        (row == 3 && hitCars(cars3, 18)) ||
        (row == 4 && hitCars(cars4, 12))) {
      gameOver = true;
      submitHighScore(HS_FROGGER, score);
    }

    if (row == 0) {
      score++;
      level = 1 + score / 4;

      speed1 = 0.7f + level * 0.08f;
      speed2 = -0.8f - level * 0.06f;
      speed3 = 1.0f + level * 0.08f;
      speed4 = -0.6f - level * 0.05f;

      row = 4;
      x = 60;
      moveCooldown = 250;
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("FROGGER");
    oled.setCursor(2, 12); oled.print("S:"); oled.print(score);
    oled.setCursor(40, 12); oled.print("HI:"); oled.print(getHighScore(HS_FROGGER));

    for (int y = 16; y <= 48; y += 8) oled.drawLine(0, y + 6, 127, y + 6, SSD1306_WHITE);

    drawCars(cars1, 24, 16);
    drawCars(cars2, 32, 14);
    drawCars(cars3, 40, 18);
    drawCars(cars4, 48, 12);

    oled.drawRect(x, 16 + row * 8, 6, 6, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(18, 22, 92, 18, SSD1306_WHITE);
      oled.setCursor(40, 26); oled.print("SPLAT!");
      oled.setCursor(26, 35); oled.print("MODE restart");
    } else {
      drawFooter("B/C side MODE up");
    }

    oled.display();
  }

private:
  int row = 4;
  int x = 60;
  int score = 0;
  int level = 1;
  bool gameOver = false;
  uint32_t moveCooldown = 0;
  float cars1[3], cars2[3], cars3[3], cars4[3];
  float speed1 = 1.0f, speed2 = -1.2f, speed3 = 1.5f, speed4 = -0.9f;

  void initCars() {
    cars1[0] = 0;  cars1[1] = 52;  cars1[2] = 104;
    cars2[0] = 18; cars2[1] = 72;  cars2[2] = 124;
    cars3[0] = 10; cars3[1] = 66;  cars3[2] = 118;
    cars4[0] = 34; cars4[1] = 86;  cars4[2] = 138;

    speed1 = 0.7f;
    speed2 = -0.8f;
    speed3 = 1.0f;
    speed4 = -0.6f;
  }

  void wrap(float& v, int w) {
    if (v > 128) v = -w;
    if (v < -w) v = 128;
  }

  bool hitCars(float* arr, int w) {
    for (int i = 0; i < 3; i++) {
      if (x + 6 > arr[i] && x < arr[i] + w) return true;
    }
    return false;
  }

  void drawCars(float* arr, int y, int w) {
    for (int i = 0; i < 3; i++) oled.fillRect((int)arr[i], y, w, 6, SSD1306_WHITE);
  }
};

class PlatformGame : public Game {
public:
  const char* name() override { return "Platform"; }
  const char* packName() override { return "Action"; }

  void onEnter() override {
    initWorld();
    playerX = 18;
    playerY = 28;
    velY = 0.0f;
    score = 0;
    coins = 0;
    scrollX = 0.0f;
    scrollSpeed = 1.2f;
    gameOver = false;
  }

  void onExit() override { submitHighScore(HS_PLATFORM, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_PLATFORM, score);
        onEnter();
      }
      return;
    }

    if (input.b().held && playerX > 2) playerX -= 2;
    if (input.c().held && playerX < 118) playerX += 2;
    if (input.mode().shortPress && isGrounded()) velY = -4.2f;
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    scrollX += scrollSpeed;
    if (scrollX >= COL_W) {
      scrollX -= COL_W;
      shiftWorld();
      score++;
      if (score % 20 == 0 && scrollSpeed < 2.8f) scrollSpeed += 0.18f;
    }

    velY += 0.35f;
    if (velY > 4.5f) velY = 4.5f;
    playerY += velY;

    int ground = supportY(playerX);
    if (ground >= 0 && playerY + 8 >= ground && velY >= 0) {
      playerY = ground - 8;
      velY = 0;
    }

    int cx = worldColumnAtScreen(playerX + 4);
    if (cx >= 0 && cx < WORLD_COLS && world[cx].coin && playerY < world[cx].topY - 2) {
      world[cx].coin = false;
      coins++;
      score += 5;
    }

    if (playerY > 64) {
      gameOver = true;
      submitHighScore(HS_PLATFORM, score);
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("PLATFORM");
    oled.setCursor(2, 12); oled.print("S:"); oled.print(score);
    oled.setCursor(42, 12); oled.print("HI:"); oled.print(getHighScore(HS_PLATFORM));
    oled.setCursor(94, 12); oled.print("C:"); oled.print(coins);

    for (int i = 0; i < WORLD_COLS; i++) {
      int sx = i * COL_W - (int)scrollX;
      if (sx <= -COL_W || sx >= 128) continue;

      if (world[i].solid) oled.fillRect(sx, world[i].topY, COL_W, 64 - world[i].topY, SSD1306_WHITE);

      if (world[i].coin) {
        int cy = world[i].topY - 6;
        if (cy > 14) oled.drawCircle(sx + COL_W / 2, cy, 2, SSD1306_WHITE);
      }
    }

    oled.fillRect(playerX, (int)playerY, 8, 8, SSD1306_BLACK);
    oled.drawRect(playerX, (int)playerY, 8, 8, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(18, 22, 92, 18, SSD1306_WHITE);
      oled.setCursor(30, 26); oled.print("YOU FELL");
      oled.setCursor(24, 35); oled.print("MODE restart");
    } else {
      drawFooter("B/C move MODE jump");
    }

    oled.display();
  }

private:
  static const int WORLD_COLS = 22;
  static const int COL_W = 8;

  struct Column {
    bool solid;
    uint8_t topY;
    bool coin;
  } world[WORLD_COLS];

  int playerX = 18;
  float playerY = 28.0f;
  float velY = 0.0f;
  int score = 0;
  int coins = 0;
  float scrollX = 0.0f;
  float scrollSpeed = 1.2f;
  bool gameOver = false;

  int genRemaining = 0;
  int genGapRemaining = 0;
  int genTopY = 42;

  void initWorld() {
    genRemaining = 0;
    genGapRemaining = 0;
    genTopY = 42;
    for (int i = 0; i < WORLD_COLS; i++) generateNextColumn(world[i], i < 4);
  }

  void shiftWorld() {
    for (int i = 0; i < WORLD_COLS - 1; i++) world[i] = world[i + 1];
    generateNextColumn(world[WORLD_COLS - 1], false);
  }

  void generateNextColumn(Column& c, bool safe) {
    c.coin = false;

    if (safe) {
      c.solid = true;
      c.topY = 42;
      return;
    }

    if (genGapRemaining > 0) {
      c.solid = false;
      c.topY = 63;
      genGapRemaining--;
      return;
    }

    if (genRemaining <= 0) {
      int difficulty = score / 25;
      int gapChance = 20 + difficulty * 3;
      if (gapChance > 42) gapChance = 42;

      if (random(0, 100) < gapChance) {
        genGapRemaining = 1 + random(0, 2 + (difficulty > 4 ? 1 : 0));
        c.solid = false;
        c.topY = 63;
        genGapRemaining--;
        return;
      }

      genRemaining = random(2, 5);
      int step = random(-1, 2);
      genTopY += step * 8;
      if (genTopY < 26) genTopY = 26;
      if (genTopY > 46) genTopY = 46;
    }

    c.solid = true;
    c.topY = genTopY;
    c.coin = (random(0, 8) == 0 && genTopY > 28);
    genRemaining--;
  }

  int worldColumnAtScreen(int sx) {
    return (sx + (int)scrollX) / COL_W;
  }

  int supportY(int sx) {
    int leftCol = worldColumnAtScreen(sx);
    int rightCol = worldColumnAtScreen(sx + 7);
    int best = -1;

    if (leftCol >= 0 && leftCol < WORLD_COLS && world[leftCol].solid) best = world[leftCol].topY;
    if (rightCol >= 0 && rightCol < WORLD_COLS && world[rightCol].solid) {
      if (best < 0 || world[rightCol].topY < best) best = world[rightCol].topY;
    }

    return best;
  }

  bool isGrounded() {
    int ground = supportY(playerX);
    return (ground >= 0 && playerY + 8 >= ground - 1 && velY == 0);
  }
};

class BoulderGame : public Game {
public:
  const char* name() override { return "Boulder"; }
  const char* packName() override { return "Puzzle"; }

  void onEnter() override {
    playerX = 60;
    score = 0;
    gameOver = false;
    tick = 0;
    for (int i = 0; i < 5; i++) {
      rocksX[i] = random(0, 120);
      rocksY[i] = -i * 12;
      gemsX[i] = random(0, 120);
      gemsY[i] = -20 - i * 24;
    }
  }

  void onExit() override { submitHighScore(HS_BOULDER, score); }

  void handleInput(InputManager& input) override {
    if (gameOver) {
      if (input.mode().shortPress) {
        submitHighScore(HS_BOULDER, score);
        onEnter();
      }
      return;
    }
    if (input.b().held && playerX > 0) playerX -= 3;
    if (input.c().held && playerX < 120) playerX += 3;
  }

  void update(uint32_t dt) override {
    if (gameOver) return;

    tick++;
    for (int i = 0; i < 5; i++) {
      rocksY[i] += 2 + (tick / 120);
      gemsY[i] += 2;

      if (rocksY[i] > 54) {
        rocksY[i] = -random(8, 30);
        rocksX[i] = random(0, 120);
      }

      if (gemsY[i] > 54) {
        gemsY[i] = -random(20, 60);
        gemsX[i] = random(0, 120);
      }

      if (playerX + 8 > rocksX[i] && playerX < rocksX[i] + 8 && 54 > rocksY[i] && 46 < rocksY[i] + 8) {
        gameOver = true;
        submitHighScore(HS_BOULDER, score);
      }

      if (playerX + 8 > gemsX[i] && playerX < gemsX[i] + 6 && 54 > gemsY[i] && 46 < gemsY[i] + 6) {
        score++;
        gemsY[i] = -random(20, 60);
        gemsX[i] = random(0, 120);
      }
    }
  }

  void render() override {
    oled.clearDisplay();
    drawTopBar("BOULDER");
    oled.setCursor(2, 12); oled.print("G:"); oled.print(score);
    oled.setCursor(42, 12); oled.print("HI:"); oled.print(getHighScore(HS_BOULDER));

    for (int i = 0; i < 5; i++) {
      oled.fillRect(rocksX[i], rocksY[i], 8, 8, SSD1306_WHITE);
      oled.drawCircle(gemsX[i], gemsY[i], 2, SSD1306_WHITE);
    }

    oled.drawRect(playerX, 46, 8, 8, SSD1306_WHITE);

    if (gameOver) {
      oled.drawRect(18, 22, 92, 18, SSD1306_WHITE);
      oled.setCursor(40, 26); oled.print("CRUSH!");
      oled.setCursor(26, 35); oled.print("MODE restart");
    }

    oled.display();
  }

private:
  int playerX = 60;
  int rocksX[5], rocksY[5], gemsX[5], gemsY[5];
  int score = 0;
  int tick = 0;
  bool gameOver = false;
};

InvadersGame invadersGame;
FroggerGame froggerGame;
PlatformGame platformGame;
BoulderGame boulderGame;


// =======================
// Pack directory + menu models
// =======================
struct PackDefinition {
  const char* name;
  const char* subtitle;
  Game** games;
  uint8_t count;
};

class PackMenu {
public:
  void begin(PackDefinition* defs, uint8_t count) {
    packDefs = defs;
    packCount = count;
  }

  void updatePackMenu(InputManager& input) {
    if (input.b().pressed && selectedPack > 0) selectedPack--;
    if (input.c().pressed && selectedPack + 1 < packCount) selectedPack++;
  }

  void renderPackMenu() {
    oled.clearDisplay();
    drawTopBar("PACKS");
    int start = selectedPack > 1 ? selectedPack - 1 : 0;
    if (start > packCount - 3) start = packCount > 3 ? packCount - 3 : 0;

    for (int row = 0; row < 3 && (start + row) < packCount; row++) {
      int i = start + row;
      int y = 16 + row * 13;
      if (i == selectedPack) {
        oled.fillRect(6, y - 1, 116, 10, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(12, y);
        oled.print(packDefs[i].name);
        oled.setTextColor(SSD1306_WHITE);
      } else {
        oled.setCursor(12, y);
        oled.print(packDefs[i].name);
      }
    }

    //oled.drawRect(4, 44, 120, 10, SSD1306_WHITE);
    //oled.setCursor(8, 46);
    //oled.print(packDefs[selectedPack].subtitle);
    drawFooter("B/C nav MODE open");
    oled.display();
  }

  void openSelectedPack() { selectedGame = 0; }

  void updateGameMenu(InputManager& input) {
    uint8_t count = currentPack().count;
    if (input.b().pressed && selectedGame > 0) selectedGame--;
    if (input.c().pressed && selectedGame + 1 < count) selectedGame++;
  }

  void renderGameMenu() {
    oled.clearDisplay();
    drawTopBar(currentPack().name);
    uint8_t count = currentPack().count;
    int start = selectedGame > 1 ? selectedGame - 1 : 0;
    if (start > count - 3) start = count > 3 ? count - 3 : 0;

    for (int row = 0; row < 3 && (start + row) < count; row++) {
      int i = start + row;
      int y = 16 + row * 13;
      Game* g = currentPack().games[i];
      if (i == selectedGame) {
        oled.fillRect(6, y - 1, 116, 10, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(12, y);
        oled.print(g->name());
        oled.setTextColor(SSD1306_WHITE);
      } else {
        oled.setCursor(12, y);
        oled.print(g->name());
      }
    }

    //oled.drawRect(4, 44, 120, 10, SSD1306_WHITE);
    //oled.setCursor(8, 46);
    //oled.print(currentPack().games[selectedGame]->implemented() ? "Ready to launch" : "Placeholder module");
    drawFooter("Hold MODE back");
    oled.display();
  }

  Game* selectedGamePtr() { return currentPack().games[selectedGame]; }

private:
  PackDefinition* packDefs = nullptr;
  uint8_t packCount = 0;
  uint8_t selectedPack = 0;
  uint8_t selectedGame = 0;

  PackDefinition& currentPack() { return packDefs[selectedPack]; }
};

class PauseMenu {
public:
  void open() { selection = 0; }

  void update(InputManager& input, bool& resumeGame, bool& goDirectory) {
    resumeGame = false;
    goDirectory = false;
    if (input.b().pressed && selection > 0) selection--;
    if (input.c().pressed && selection < 1) selection++;
    if (input.mode().shortPress) {
      if (selection == 0) resumeGame = true;
      else goDirectory = true;
    }
  }

  void render() {
    oled.clearDisplay();
    drawTopBar("PAUSED");
    const char* items[2] = { "Resume", "Directory" };
    for (int i = 0; i < 2; i++) {
      int y = 22 + i * 14;
      if (i == selection) {
        oled.fillRect(18, y - 1, 90, 10, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
        oled.setCursor(28, y);
        oled.print(items[i]);
        oled.setTextColor(SSD1306_WHITE);
      } else {
        oled.setCursor(28, y);
        oled.print(items[i]);
      }
    }
    drawFooter("B/C move MODE ok");
    oled.display();
  }

private:
  uint8_t selection = 0;
};

// =======================
// Globals
// =======================
InputManager input;
PackMenu packMenu;
PauseMenu pauseMenu;

DodgeGame dodgeGame;
RunnerGame runnerGame;
CatchGame catchGame;
TetrisGame tetrisGame;

Game* arcadeGames[] = {
  &dodgeGame,
  &runnerGame,
  &catchGame,
  &flappyBirdGame,
  &racingGame,
  &boulderGame,
  &towerGame
};

Game* puzzleGames[] = {
  
  &tetrisGame,
  &simonGame
};

Game* actionGames[] = {
  &invadersGame,
  &froggerGame,
  &platformGame
};

PackDefinition packs[] = {
  { "Arcade Pack", "Fast score chasers", arcadeGames, (uint8_t)(sizeof(arcadeGames) / sizeof(arcadeGames[0])) },
  { "Puzzle Pack", "Patterns + builders", puzzleGames, (uint8_t)(sizeof(puzzleGames) / sizeof(puzzleGames[0])) },
  { "Action Pack", "Shooter + hops + runs", actionGames, (uint8_t)(sizeof(actionGames) / sizeof(actionGames[0])) }
};

AppState appState = APP_SPLASH;
unsigned long splashStart = 0;
unsigned long lastFrame = 0;
Game* activeGame = nullptr;

// =======================
// Dynamic splash
// =======================
void renderSplash() {
  oled.clearDisplay();

  unsigned long elapsed = millis() - splashStart;
  int progress = map((int)(elapsed > 1500 ? 1500 : elapsed), 0, 1500, 0, 96);
  int bob = (elapsed / 120) % 2;
  int shipX = map((int)(elapsed > 1200 ? 1200 : elapsed), 0, 1200, -24, 18);

  for (int i = 0; i < 8; i++) {
    int sx = (elapsed / 5 + i * 17) % 128;
    int sy = 8 + (i * 7) % 42;
    oled.drawPixel(127 - sx, sy, SSD1306_WHITE);
  }

  oled.drawRoundRect(shipX, 18 + bob, 30, 16, 4, SSD1306_WHITE);
  oled.drawRect(shipX + 4, 22 + bob, 12, 8, SSD1306_WHITE);
  oled.fillCircle(shipX + 22, 22 + bob, 2, SSD1306_WHITE);
  oled.fillCircle(shipX + 26, 28 + bob, 2, SSD1306_WHITE);
  oled.drawLine(shipX - 6, 24 + bob, shipX - 1, 24 + bob, SSD1306_WHITE);
  oled.drawLine(shipX - 10, 27 + bob, shipX - 3, 27 + bob, SSD1306_WHITE);

  drawCenteredText("POCKET-BOY", 8, 2);
  drawCenteredText("Mega Pack", 30, 1);

  oled.drawRect(14, 50, 100, 8, SSD1306_WHITE);
  if (progress > 0) oled.fillRect(16, 52, progress, 4, SSD1306_WHITE);
  if ((elapsed / 180) % 2 == 0) drawCenteredText("Loading...", 57, 1);

  oled.display();
}

void launchSelectedGame() {
  activeGame = packMenu.selectedGamePtr();
  if (activeGame) {
    activeGame->onEnter();
    appState = APP_RUNNING_GAME;
  }
}

void setup() {
  input.begin();
  Wire.begin();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) delay(100);
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.display();

  loadHighScores();
  randomSeed(micros());

  tetrisGame.bindInput(&input);
  packMenu.begin(packs, PACK_COUNT);

  splashStart = millis();
  lastFrame = millis();
}

void loop() {
  input.update();

  unsigned long now = millis();
  uint32_t dt = now - lastFrame;
  lastFrame = now;

  switch (appState) {
    case APP_SPLASH:
      renderSplash();
      if (millis() - splashStart > 1500) appState = APP_PACK_MENU;
      break;

    case APP_PACK_MENU:
      packMenu.updatePackMenu(input);
      if (input.mode().shortPress) {
        packMenu.openSelectedPack();
        appState = APP_GAME_MENU;
      }
      packMenu.renderPackMenu();
      break;

    case APP_GAME_MENU:
      packMenu.updateGameMenu(input);
      if (input.mode().shortPress) {
        launchSelectedGame();
      } else if (input.mode().longPress) {
        appState = APP_PACK_MENU;
      }
      packMenu.renderGameMenu();
      break;

    case APP_RUNNING_GAME:
      if (input.mode().longPress) {
        pauseMenu.open();
        appState = APP_PAUSE_MENU;
      } else if (activeGame) {
        activeGame->handleInput(input);
        activeGame->update(dt);
        activeGame->render();
      }
      break;

    case APP_PAUSE_MENU: {
      bool resumeGame = false;
      bool goDirectory = false;
      pauseMenu.update(input, resumeGame, goDirectory);
      if (resumeGame) {
        appState = APP_RUNNING_GAME;
      } else if (goDirectory) {
        if (activeGame) activeGame->onExit();
        activeGame = nullptr;
        appState = APP_GAME_MENU;
      }
      pauseMenu.render();
      break;
    }
  }

  delay(16);
}
