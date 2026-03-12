# Jezziku

A native Haiku clone of the classic ball-and-wall puzzle game. Draw lines to divide the playing field while avoiding the bouncing balls — capture 75% of the field to advance to the next level.

![Haiku OS](https://img.shields.io/badge/platform-Haiku%20OS-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Version](https://img.shields.io/badge/version-1.0.0-orange)

---

## Features

- Native Haiku application written in C++ using the BeAPI
- Per-pixel rendered 3D spinning balls with Phong lighting
- Two independent wall arms that can each be hit separately
- Ball-to-ball collision physics
- Three difficulty settings with per-difficulty high score tables
- Countdown timer that tightens with each level and difficulty
- Stylised in-game HUD with lives, captured percentage, and timer
- High score system with name entry, persistent storage, and top-10 tables
- Supports Haiku window stacking and tiling
- Vector icon in HVIF format

---

## Requirements

- [Haiku](https://www.haiku-os.org/) R1 Beta 5 or later
- `g++` with C++17 support (included with Haiku's development tools)
- Standard Haiku build tools: `rc`, `xres`, `mimeset` (all included with Haiku)

---

## Building

```bash
cd jezziku
make
```

The build pipeline:
1. `g++` compiles `Jezziku.cpp` to an object file
2. `rc` compiles `Jezziku.rdef` (app signature, flags, and icon) into a resource file
3. `g++` links the binary
4. `xres` embeds the compiled resources into the binary
5. `mimeset` registers the app with Haiku's MIME database and sets filesystem attributes

To clean and rebuild from scratch:

```bash
make clean && make
```

To launch directly after building:

```bash
make run
```

---

## How to Play

**Objective:** Capture 75% or more of the playing field before the timer runs out to advance to the next level.

### Controls

| Input | Action |
|---|---|
| Left-click | Draw a horizontal wall |
| Right-click | Draw a vertical wall |
| P | Pause / resume |

### Wall Mechanics

Clicking places a wall that grows outward from the click point in both directions simultaneously. The two halves are **independent** — a ball can destroy one arm while the other keeps growing. Each destroyed arm costs one life. When both arms complete (reach a wall or boundary), any enclosed area containing no balls is **captured**.

Only one wall can be drawn at a time.

### Scoring

Points are awarded when you complete a level:

```
Score = (area captured %) × level × 100
      + (seconds remaining) × level × 5
```

Scores are tracked separately for each difficulty setting.

### Difficulty

| Setting | Ball speed | Time per level |
|---|---|---|
| Easy | Moderate | 120 s (−5 s/level) |
| Medium | Fast | 90 s (−4 s/level) |
| Hard | Very fast | 60 s (−3 s/level) |

The minimum time per level is 20 seconds regardless of difficulty.

---

## High Scores

High scores are saved to:

```
~/config/settings/Jezziku/scores_easy.txt
~/config/settings/Jezziku/scores_medium.txt
~/config/settings/Jezziku/scores_hard.txt
```

Each file holds up to 10 entries in tab-separated `score\tname` format, sorted descending. The High Scores window (Game → High Scores) lets you switch between difficulty tables.

---

## Project Structure

```
jezziku/
├── Jezziku.cpp     — Full game implementation
├── Jezziku.h       — Class declarations and message constants
├── Jezziku.rdef    — Resource definition (app signature, flags, icon)
├── jezz_icon       — Application icon in HVIF (Haiku Vector Icon Format)
├── Makefile
├── LICENSE
└── README.md
```

---

## License

MIT — see [LICENSE](LICENSE) for details.

---

## Credits

Created by [David Freeman](https://d3.codes/about)

Inspired by the classic JezzBall game originally developed by Dima Pavlovsky and published by Microsoft for Windows Entertainment Pack 3 (1991).
