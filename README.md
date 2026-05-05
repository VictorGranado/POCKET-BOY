# Pocket-Boy


Pocket-Boy is a tiny handheld game console built around an **ESP8266**, a **0.96\" SSD1306 OLED**, and a compact **3-button control scheme**.

It is designed as a small, Game Boy Micro-inspired device that runs themed packs of mini games on simple hardware, with persistent high scores, a clean directory-style UI, and room to grow into OTA-updatable game packs later.

---

## Preview

[Watch the demo on YouTube](https://www.youtube.com/watch?v=DU62P35-DnI)

Example markdown when you have images:

---

## Features

- ESP8266-based handheld firmware
- 128x64 monochrome OLED UI
- 3-button control scheme
- Dynamic splash screen
- Theme-pack / directory-style menu system
- Multiple playable mini games in one firmware
- Persistent EEPROM-backed high scores
- Lightweight game architecture for adding and tuning new titles
- Designed around tight embedded constraints

---

## Current game packs

### Arcade Pack
- Dodge
- Runner
- Catch
- Flappy
- Racing
- Boulder
- Tower

### Puzzle Pack
- Tetris
- Simon
- Boulder

### Action Pack
- Invaders
- Frogger
- Platform

---

## Hardware

### Core components
- **MCU:** ESP8266
- **Display:** 0.96\" I2C SSD1306 OLED, 128x64
- **Buttons:** 3 momentary buttons
- **Power:** 3.3 V ESP8266 setup, with portable power options depending on build revision

### Pin configuration

Using raw GPIO numbering:

```cpp
const int BTN_MODE_PIN = 14;
const int BTN_B_PIN    = 12;
const int BTN_C_PIN    = 13;
```

OLED configuration:

```cpp
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C
```

### Typical wiring

#### Buttons
Each button is wired:
- one side to the GPIO pin
- the other side to **GND**

The firmware uses `INPUT_PULLUP`, so buttons are **active LOW**.

#### OLED
Typical SSD1306 I2C wiring:
- `VCC` → `3.3V`
- `GND` → `GND`
- `SDA` → board I2C SDA
- `SCL` → board I2C SCL

> Confirm SDA/SCL pin mapping for your exact ESP8266 board.

---

## Controls

### System / menus
- **B** → move left / previous
- **C** → move right / next
- **MODE short press** → select / action
- **MODE long press** → back / pause menu, depending on context

### In games
Controls vary slightly by game, but the general mapping is:

- **B / C** for movement or choice
- **MODE short** for jump, action, rotate, shoot, flap, or confirm
- **MODE long** for the pause menu

---

## Repository layout

Current main firmware:
- `PocketBoy_all_games_revised_v2.ino`

Suggested future structure:

```text
PocketBoy/
  firmware/
    PocketBoy_all_games_revised_v2.ino
  docs/
    images/
  hardware/
    case/
    pcb/
```

As the project grows, the firmware can be split into:
- hardware definitions
- UI/menu modules
- game modules
- asset definitions
- OTA / package-update support

---

## Building and flashing

This project is intended for the **Arduino IDE** or a compatible ESP8266 workflow.

### Dependencies
Install these before building:

- **Adafruit GFX**
- **Adafruit SSD1306**
- **EEPROM** (included with the ESP8266 Arduino core)

### Arduino IDE setup
Recommended:
- Select your ESP8266 target board
- Choose a flash size / partition scheme with enough room for a larger multi-game build
- Use an upload speed supported by your board

### Basic build steps
1. Open the main `.ino` file in Arduino IDE
2. Install required libraries
3. Select the correct ESP8266 board and port
4. Compile and upload
5. Reset the device if needed

### Notes
Because the firmware is getting fairly large, compile size matters. Depending on your exact ESP8266 board and flash layout, you may eventually want to split games into separate themed builds.

---

## High scores

High scores are stored in EEPROM for score-based games, so they survive resets and power cycles.

Currently tracked for:
- Dodge
- Runner
- Catch
- Tetris
- Flappy
- Simon
- Racing
- Tower
- Invaders
- Frogger
- Platform
- Boulder

---

## Software architecture

The firmware is organized around a few core ideas:

- **Input manager** for debounced 3-button input
- **Pack menu** for browsing game packs and games
- **Pause menu** for in-game navigation
- **Game base class** for a consistent interface
- **Per-game classes** for individual titles
- **EEPROM-backed storage** for high scores

Each game implements a common `Game` interface so games can be added, tuned, swapped, or regrouped without rewriting the full menu and input system.

---

## Design constraints

Pocket-Boy is intentionally built around tight constraints:

- 128x64 monochrome display
- only 3 buttons
- ESP8266 memory and flash limits
- simple sprite / primitive rendering
- handheld-friendly menu flow

These limitations are part of the identity of the project. Most games are adapted to fit the device rather than trying to directly clone full-size originals.

---

## Status

This project is actively evolving.

Recent work includes:
- tuning fairness in several games
- reorganizing the UI into theme packs
- adding persistent high scores
- improving scrolling / camera behavior in some games
- iterating on mechanics to better fit the 3-button layout

---

## Roadmap

- [ ] Continue tuning game balance and fairness
- [ ] Improve UI polish and transitions
- [ ] Add OTA update flow for firmware / game packs
- [ ] Refine package-based release workflow
- [ ] Improve enclosure and mechanical design
- [ ] Add sound / buzzer support
- [ ] Optimize memory usage for larger builds
- [ ] Split firmware into cleaner modules if needed

---

## Contributing

Contributions, ideas, balance suggestions, and hardware improvements are welcome.

Good areas to contribute:
- game tuning
- UI polish
- memory optimization
- enclosure / CAD refinement
- documentation
- new mini game concepts that fit the 3-button layout

---

## Inspiration

Pocket-Boy is inspired by:
- Nintendo Game Boy Micro style handhelds
- tiny embedded game consoles
- retro minigame collections
- building polished experiences on constrained hardware

---

## License

Choose a license for the repo, for example:
- MIT
