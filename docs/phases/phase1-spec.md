# Phase 1 Spec: PicoCalc Graphing Calculator

**Scope**: Bootstrap the hardware abstraction layer and build a working scientific calculator with basic function graphing. This phase produces a daily-driver calculator firmware — input expressions, see results, plot $f(x)$.

**Target hardware**: Pico 1 H (RP2040) and Pico 2 H (RP2350) on ClockworkPi v2.0 mainboard.

**Language**: C++17, Pico SDK, CMake.

**Starting point**: Fork Coyote OS's peripheral drivers (`lcdspi/`, `i2ckbd/`, `rp2040-psram/`, `pwm_sound/`) as the HAL foundation. Rewrite the application layer from scratch in C++. Use Delta Pico's `rbop` layout-node architecture as the design reference for the natural math renderer.

**End state**: a UF2 firmware that boots to a home screen where you can type expressions, see pretty-printed results, define $Y_1 \ldots Y_n$ functions, and graph them on a $320\times320$ color display with trace, zoom, and window controls.

---

## 1. Project structure

```
picocalc-graphcalc/
├── CMakeLists.txt                 # Top-level build, board target selection
├── pico_sdk_import.cmake          # Standard Pico SDK import
├── src/
│   ├── main.cpp                   # Entry point, core 0/1 dispatch
│   ├── platform/                  # HAL layer (C wrappers around Coyote OS drivers)
│   │   ├── display.hpp / .cpp     # ST7365P SPI LCD abstraction
│   │   ├── keyboard.hpp / .cpp    # STM32 I2C keyboard abstraction
│   │   ├── storage.hpp / .cpp     # SD card FAT32 (FatFs)
│   │   ├── psram.hpp / .cpp       # 8MB PSRAM allocator
│   │   ├── audio.hpp / .cpp       # PWM buzzer (optional, low priority)
│   │   ├── system.hpp / .cpp      # Clock, sleep, battery, USB
│   │   └── platform.hpp           # Aggregate header + init
│   ├── gfx/                       # Graphics primitives
│   │   ├── framebuffer.hpp / .cpp # Line-buffer renderer + DMA pipeline
│   │   ├── font.hpp / .cpp        # Bitmap font loading and text draw
│   │   ├── primitives.hpp / .cpp  # Rect, line, circle, fill, blit
│   │   └── color.hpp              # RGB565 color constants and helpers
│   ├── ui/                        # UI framework
│   │   ├── screen.hpp             # Abstract Screen base class
│   │   ├── screen_manager.hpp/.cpp# Push/pop screen stack, render loop
│   │   ├── widget.hpp             # Base widget (Label, Input, Menu, etc.)
│   │   ├── input_line.hpp / .cpp  # Expression input with cursor + editing
│   │   ├── softkeys.hpp / .cpp    # Bottom row context-sensitive labels
│   │   └── dialog.hpp / .cpp      # Modal yes/no, text input dialogs
│   ├── math/                      # Math engine
│   │   ├── engine.hpp / .cpp      # Expression parse, evaluate, variables
│   │   ├── tinyexpr_pp.h / .c     # tinyexpr++ (vendored, C99/C++11)
│   │   ├── functions.hpp / .cpp   # Extended function library (custom fns)
│   │   └── types.hpp              # calc_float typedef, list/matrix stubs
│   ├── render/                    # Natural math renderer
│   │   ├── layout_node.hpp        # AST → layout tree node types
│   │   ├── layout_builder.hpp/.cpp# Parse expression → layout tree
│   │   ├── layout_renderer.hpp/.cpp# Layout tree → pixel rendering
│   │   └── glyphs.hpp             # Glyph metrics for math symbols
│   ├── apps/                      # Application screens
│   │   ├── home_screen.hpp / .cpp # Calculator home (expression + history)
│   │   ├── graph_screen.hpp / .cpp# Function graphing viewport
│   │   ├── y_editor.hpp / .cpp    # Y= function list editor
│   │   ├── window_screen.hpp/.cpp # Graph window settings
│   │   └── about_screen.hpp / .cpp# Version info
│   └── config.hpp                 # Compile-time config, board detection
├── drivers/                       # Vendored C drivers from Coyote OS
│   ├── lcdspi/                    # ST7365P SPI driver (C)
│   ├── i2ckbd/                    # I2C keyboard driver (C)
│   ├── rp2040-psram/              # PSRAM driver (C)
│   ├── pwm_sound/                 # PWM audio driver (C)
│   └── fatfs/                     # FatFs library (C)
├── assets/
│   ├── font_8x16.h               # Monospace bitmap font data
│   ├── font_6x8.h                # Small font for labels/status
│   └── font_prop.h               # Proportional font for menus
└── docs/
    └── phase1-spec.md             # This document
```

### Build system

The top-level `CMakeLists.txt` should support:

```cmake
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(picocalc_graphcalc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# Board detection: PICO_BOARD is set by cmake -DPICO_BOARD=pico or pico2
if(PICO_BOARD STREQUAL "pico2")
    add_compile_definitions(PICOCALC_PICO2=1)
else()
    add_compile_definitions(PICOCALC_PICO1=1)
endif()

add_executable(picocalc_graphcalc
    src/main.cpp
    # ... all .cpp and .c files
)

target_include_directories(picocalc_graphcalc PRIVATE
    src/ drivers/
)

target_link_libraries(picocalc_graphcalc
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_timer
    hardware_interp  # For softfloat acceleration on RP2040
)

pico_add_extra_outputs(picocalc_graphcalc)  # Generates .uf2
```

Build commands:

```bash
# Pico 1 build
cmake -DPICO_BOARD=pico -B build/pico1 -S .
cmake --build build/pico1 -j$(nproc)

# Pico 2 build
cmake -DPICO_BOARD=pico2 -B build/pico2 -S .
cmake --build build/pico2 -j$(nproc)
```

---

## 2. Platform Abstraction Layer (HAL)

The HAL wraps Coyote OS's C drivers in C++ classes with RAII and typed interfaces. All hardware access flows through these classes — the application layer never calls Pico SDK or driver functions directly.

### 2.1 Display (`platform/display.hpp`)

```cpp
#pragma once
#include <cstdint>

namespace platform {

// 320x320 RGB565
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 320;

struct Color {
    uint16_t rgb565;
    
    static constexpr Color from_rgb(uint8_t r, uint8_t g, uint8_t b) {
        return { static_cast<uint16_t>(
            ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        )};
    }
};

// Common colors
namespace colors {
    constexpr Color BLACK   = {0x0000};
    constexpr Color WHITE   = {0xFFFF};
    constexpr Color BLUE    = Color::from_rgb(0, 0, 255);
    constexpr Color RED     = Color::from_rgb(255, 0, 0);
    constexpr Color GREEN   = Color::from_rgb(0, 200, 0);
    constexpr Color GRAY_BG = Color::from_rgb(240, 240, 240);
    constexpr Color GRAY_LN = Color::from_rgb(200, 200, 200);
    constexpr Color CURSOR  = Color::from_rgb(0, 120, 215);
}

class Display {
public:
    void init();

    // Primitive drawing — all operate on the internal line buffer.
    // Call flush() to push to screen.
    void clear(Color c = colors::WHITE);
    void set_pixel(int x, int y, Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    void draw_hline(int x, int y, int w, Color c);
    void draw_vline(int x, int y, int h, Color c);
    void draw_line(int x0, int y0, int x1, int y1, Color c);
    void draw_rect(int x, int y, int w, int h, Color c);  // outline
    
    // Text drawing (using loaded bitmap font)
    void draw_char(int x, int y, char ch, Color fg, Color bg);
    void draw_string(int x, int y, const char* s, Color fg, Color bg);
    
    // Flush current state to LCD via DMA. Non-blocking.
    void flush();
    
    // Wait for previous flush to complete.
    void wait_flush();

    // Backlight control (via STM32 south bridge)
    void set_backlight(uint8_t level);  // 0-255
};

} // namespace platform
```

**Implementation notes**:

The display driver wraps `lcdspi/` from Coyote OS. Internally, it uses a **line-buffer rendering** strategy rather than a full framebuffer:

- A small SRAM buffer holds 16–20 scanlines at a time (~10–12.5 KB for 20 lines of 320 RGB565 pixels).
- The render loop draws into this buffer, then DMA transfers it to the ST7365P while the next batch is being drawn.
- This runs on **core 1** via the multicore API: core 0 calls `display.flush()` which signals core 1 to initiate the DMA transfer.
- On **Pico 2**, the framebuffer may optionally be placed fully in SRAM (200 KB out of 520 KB available), falling back to line-buffer mode only if memory pressure requires it. This is controlled by `config.hpp`:

```cpp
// config.hpp
#ifdef PICOCALC_PICO2
    constexpr bool USE_FULL_FRAMEBUFFER = true;   // 200KB SRAM framebuffer
#else
    constexpr bool USE_FULL_FRAMEBUFFER = false;  // Line-buffer mode
#endif
```

### 2.2 Keyboard (`platform/keyboard.hpp`)

```cpp
#pragma once
#include <cstdint>

namespace platform {

// Logical key codes — not raw scan codes. 
// Mapped from the STM32 co-processor's I2C reports.
enum class Key : uint8_t {
    NONE = 0,
    
    // Digits and decimal
    K0, K1, K2, K3, K4, K5, K6, K7, K8, K9, DOT,
    
    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, POWER, 
    LPAREN, RPAREN, COMMA, EQUALS,
    
    // Letters (for variable names, function entry)
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Navigation
    UP, DOWN, LEFT, RIGHT,
    ENTER, BACKSPACE, DEL, ESCAPE,
    TAB, SPACE,
    
    // Function keys (F1-F6 on PicoCalc hardware)
    F1, F2, F3, F4, F5, F6,
    
    // Modifiers (stateful)
    SHIFT, CTRL, ALT,
    SECOND,  // Virtual: mapped from a physical key combo for TI-like "2nd" mode
    ALPHA,   // Virtual: mapped for alpha-lock mode
};

struct KeyEvent {
    Key key         = Key::NONE;
    bool pressed    = false;   // true = press, false = release
    bool shift_held = false;
    bool ctrl_held  = false;
    bool alt_held   = false;
    bool second     = false;   // "2nd" modifier was active when key was pressed
    bool alpha      = false;   // "Alpha" modifier was active
};

class Keyboard {
public:
    void init();
    
    // Poll the STM32 over I2C. Call once per main-loop iteration.
    // Returns the next key event, or a NONE event if no key activity.
    KeyEvent poll();
    
    // Check if a specific key is currently held down.
    bool is_held(Key k) const;
    
    // Get a printable ASCII character for this key event, 
    // accounting for shift state. Returns 0 if not printable.
    char to_char(const KeyEvent& ev) const;
    
private:
    uint8_t key_state_[12] {};  // Raw state from I2C
    bool modifier_state_[8] {};
};

} // namespace platform
```

**Implementation notes**:

Coyote OS's `i2ckbd/` module reads key matrix state from the STM32 via I2C at address `0x1F` (check `config.h` in Coyote OS). The raw data is a bitmap of pressed keys. The C++ wrapper:

1. Reads raw I2C state (carried over from Coyote OS's driver)
2. Debounces (STM32 firmware likely handles this, but add 20ms software debounce as a safety net)
3. Detects press/release edges by diffing against previous state
4. Maps scan codes to `Key` enum values using a lookup table derived from Coyote OS's `keyboard_definition.h`
5. Manages modifier state (Shift, Ctrl, 2nd, Alpha) as toggles or holds

The **2nd** and **Alpha** modes are implemented as software toggles — pressing the designated key enables the mode for the next keypress only, then it auto-clears (matching TI-83 behavior). A status indicator on the top bar shows when these modes are active.

### 2.3 Storage (`platform/storage.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace platform {

class Storage {
public:
    bool init();  // Mount SD card, returns false if no card
    
    // Simple file operations using FatFs
    bool file_exists(const char* path);
    int  read_file(const char* path, uint8_t* buf, size_t max_len);
    bool write_file(const char* path, const uint8_t* buf, size_t len);
    bool append_file(const char* path, const uint8_t* buf, size_t len);
    bool delete_file(const char* path);
    
    // Directory listing
    struct DirEntry {
        char name[64];
        bool is_dir;
        uint32_t size;
    };
    int list_dir(const char* path, DirEntry* entries, int max_entries);
    
    // Convenience
    bool read_string(const char* path, char* buf, size_t max_len);
    bool write_string(const char* path, const char* str);
};

} // namespace platform
```

The SD card holds: user programs (future), saved variables, graph window settings, expression history, and configuration. Directory layout on SD:

```
/picocalc/
├── config.ini         # Settings (backlight level, theme, etc.)
├── history.txt        # Expression history (newline-delimited)
├── variables.dat      # Saved variables (A-Z, Ans) as binary floats
├── yfuncs.txt         # Y1...Y0 function definitions
└── programs/          # Future: user MicroPython scripts
```

### 2.4 PSRAM (`platform/psram.hpp`)

```cpp
#pragma once
#include <cstdint>
#include <cstddef>

namespace platform {

class Psram {
public:
    void init();
    
    // Simple linear allocator (no free — reset clears everything).
    // Used for: framebuffer (if full-fb mode), statistics data, 
    // large matrix storage, scripting heap.
    void* alloc(size_t bytes, size_t alignment = 4);
    void  reset();  // Frees all allocations
    
    // Direct byte-level access (slow — SPI-mediated)
    void write(uint32_t addr, const uint8_t* data, size_t len);
    void read(uint32_t addr, uint8_t* data, size_t len);
    
    size_t total_bytes() const { return 8 * 1024 * 1024; }
    size_t used_bytes() const;
    size_t free_bytes() const;
};

} // namespace platform
```

In Phase 1, PSRAM is used primarily for framebuffer storage on Pico 1 (if full-framebuffer mode is enabled) and as a bulk arena for future phases (statistics lists, matrices). The allocator is a simple bump allocator — no deallocation needed for the framebuffer use case.

---

## 3. Graphics layer (`gfx/`)

### 3.1 Line-buffer renderer

The core rendering pipeline operates in two modes controlled by `config.hpp`:

**Line-buffer mode (Pico 1 default)**: a $320\times16$ pixel SRAM buffer (~10 KB). The render loop works in vertical strips:

```
for each strip of 16 scanlines:
    clear strip buffer
    call each UI element's render() with clip rect = this strip
    DMA strip buffer to LCD via SPI
```

This means every renderable object must support **clipped rendering** — given a rectangle representing the current strip, draw only the pixels that fall within it. This is straightforward for rectangles, lines, and text (just skip rows/chars outside the clip region). The main cost is that complex scenes require re-traversing the UI tree for each strip, but with only 20 strips per frame and a simple UI, this completes in <15 ms.

**Full-framebuffer mode (Pico 2 option)**: a $320\times320$ RGB565 buffer in SRAM (~200 KB). Draw freely, then DMA the entire buffer. Simpler code path but uses significant SRAM.

### 3.2 Fonts

Three bitmap fonts embedded as `const` arrays in flash:

| Font | Size | Use |
|------|------|-----|
| `font_8x16` | $8\times16$ px, monospace | Home screen expressions, history, program editor |
| `font_6x8` | $6\times8$ px, monospace | Status bar, axis labels, small annotations |
| `font_prop` | Variable-width, ~10px height | Menu items, dialog text, softkey labels |

Font data format: each character is a bitmap stored as an array of bytes, one byte per row, MSB-left. The proportional font additionally stores a width table.

**Source**: extract font data from Coyote OS's existing font header (`font6x8e500.h` in the MicroPython driver project) for the small font. Generate the $8\times16$ font from a public-domain bitmap font (e.g., Terminus, Cozette, or GNU Unifont) using a Python script that converts BDF/PCF to C header arrays.

---

## 4. UI framework (`ui/`)

### 4.1 Screen manager

```cpp
// ui/screen.hpp
#pragma once

namespace ui {

class Screen {
public:
    virtual ~Screen() = default;
    
    // Called when this screen becomes the top of the stack
    virtual void on_activate() {}
    
    // Called when this screen is popped or covered
    virtual void on_deactivate() {}
    
    // Handle a single key event. Return true if consumed.
    virtual bool on_key(const platform::KeyEvent& ev) = 0;
    
    // Render this screen's contents. 
    // In line-buffer mode, called once per strip with clip rect.
    virtual void render(gfx::Framebuffer& fb) = 0;
    
    // Optional: return true if this screen needs a status bar
    virtual bool wants_status_bar() const { return true; }
};

} // namespace ui
```

```cpp
// ui/screen_manager.hpp
class ScreenManager {
public:
    void push(Screen* screen);   // Push and activate
    void pop();                  // Pop and re-activate previous
    void replace(Screen* screen);// Pop current, push new
    Screen* current() const;
    
    // Main loop integration
    void handle_key(const platform::KeyEvent& ev);
    void render(gfx::Framebuffer& fb);
    
private:
    static constexpr int MAX_DEPTH = 8;
    Screen* stack_[MAX_DEPTH] {};
    int depth_ = 0;
};
```

**Screen stack typical flow**:

```
[HomeScreen]                          ← boot state
[HomeScreen] → [GraphScreen]          ← press GRAPH key
[HomeScreen] → [GraphScreen] → [WindowScreen]  ← press WINDOW
[HomeScreen] → [GraphScreen]          ← press ESCAPE (pops WindowScreen)
[HomeScreen]                          ← press ESCAPE (pops GraphScreen)
[HomeScreen] → [YEditorScreen]        ← press Y= key
```

### 4.2 Softkeys

The bottom 20 pixels of the screen display context-sensitive softkey labels (like the TI-84's F1–F5 row above the function keys). Each screen defines its own softkey labels and handlers:

```cpp
struct SoftkeyDef {
    const char* label;               // e.g., "ZOOM", "TRACE", "TBLSET"
    void (*handler)(Screen* self);   // Callback when F-key pressed
};
```

The softkey bar renders as: `| F1:LABEL | F2:LABEL | F3:LABEL | F4:LABEL | F5:LABEL | F6:LABEL |` in the small font, with a dark background.

### 4.3 Status bar

The top 16 pixels display: battery icon (placeholder in Phase 1), 2nd/Alpha mode indicators, current screen title (left-aligned), and clock or calculation progress indicator (right-aligned).

### 4.4 Screen layout map ($320\times320$)

```
┌──────────────────────────────────┐  y=0
│  Status bar (16px)                │
├──────────────────────────────────┤  y=16
│                                    │
│  Main content area (280px)         │
│  (screen-specific rendering)       │
│                                    │
├──────────────────────────────────┤  y=296
│  Softkey bar (24px)                │
└──────────────────────────────────┘  y=320
```

Usable content area: $320\times280$ pixels.

---

## 5. Math engine (`math/`)

### 5.1 Expression evaluation

The math engine wraps **tinyexpr++** (or plain tinyexpr compiled as C++). Tinyexpr provides:

- Parsing infix expressions like `2+3*sin(pi/4)` into an evaluation tree
- Evaluating with variable bindings
- Custom function registration

The wrapper (`math/engine.hpp`) adds:

```cpp
namespace math {

// Calculator numeric type
using calc_t = double;  // Use double everywhere for now.
                        // Pico 1 uses ROM softfloat (slow but accurate).
                        // Pico 2 hardware FPU is single-precision only,
                        // but double gives correct rounding for display.

// Variable storage: A-Z (26), theta, Ans = 28 variables
struct Variables {
    calc_t vars[28] {};       // A=0, B=1, ..., Z=25, theta=26, Ans=27
    calc_t& operator[](char name);  // 'A'..'Z', 't' for theta
    calc_t& ans() { return vars[27]; }
};

// Expression evaluation result
struct EvalResult {
    bool ok;
    calc_t value;
    const char* error;  // Non-null if ok == false
};

class Engine {
public:
    Engine();
    
    // Evaluate a string expression. Updates Ans on success.
    EvalResult evaluate(const char* expr);
    
    // Evaluate with a specific variable binding (for graphing).
    // Temporarily binds 'x_var' to 'x_val', evaluates, then restores.
    EvalResult evaluate_at(const char* expr, char x_var, calc_t x_val);
    
    // Access variables
    Variables& vars() { return vars_; }
    
    // Register extended functions (Phase 1: basic trig, log, exp, abs, etc.)
    void register_builtins();
    
private:
    Variables vars_;
};

} // namespace math
```

**Extended function library** (Phase 1): beyond tinyexpr's built-ins (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `sqrt`, `abs`, `ceil`, `floor`, `pow`, `fmod`), register:

- `ln(x)` → alias for `log(x)` (natural log)
- `log10(x)` → `log(x)/log(10)` (common log, matching TI's `log()` key)
- `nPr(n,r)`, `nCr(n,r)` → permutations and combinations
- `factorial(n)` or `n!` handling (tinyexpr doesn't support postfix `!` — may need a preprocessing step)
- `rand()` → pseudo-random [0,1)
- `min(a,b)`, `max(a,b)`
- `round(x,n)` → round to $n$ decimal places
- `deg(x)`, `rad(x)` → degree/radian conversion

**Angle mode**: maintain a global `AngleMode` enum (`RADIANS`, `DEGREES`). When in degree mode, the engine wraps all trig functions: `sin(x)` internally becomes `sin(x * PI/180)`, `asin(x)` returns `result * 180/PI`. This is implemented by registering custom function wrappers that check the mode.

### 5.2 Expression formatting

For display, results need formatting:

```cpp
namespace math {

// Format a number for display.
// Rules:
//   - If integer-valued and |x| < 1e10: display as integer (no decimal)
//   - Otherwise: up to 10 significant figures, strip trailing zeros
//   - Scientific notation for |x| >= 1e10 or |x| < 1e-4 (nonzero)
//   - Special values: "Error", "Inf", "-Inf", "NaN"
int format_number(calc_t x, char* buf, size_t buf_len);

} // namespace math
```

---

## 6. Natural math renderer (`render/`)

This is the most complex component. The goal is to display expressions as 2D typeset math: fractions with horizontal bars, superscript exponents, square root symbols, and nested parentheses that scale vertically.

### 6.1 Design (adapted from Delta Pico's `rbop` concept)

The renderer works in two passes:

**Pass 1 — Layout**: transform a parsed expression string (or AST) into a tree of `LayoutNode` objects. Each node knows its bounding box dimensions (width, height, baseline offset).

**Pass 2 — Render**: walk the layout tree, drawing each node at its computed position relative to its parent.

```cpp
namespace render {

// A layout node knows its size and how to render itself.
struct LayoutNode {
    int width  = 0;   // pixels
    int height = 0;   // pixels
    int baseline = 0; // pixels from top to the "math baseline"
    
    virtual ~LayoutNode() = default;
    virtual void render(gfx::Framebuffer& fb, int x, int y,
                        platform::Color fg) = 0;
};

// Concrete node types:

// Simple text span: "123", "sin", "x", "+", etc.
struct TextNode : LayoutNode {
    char text[32];
    void render(...) override; // Draw text at (x, y+baseline-font_ascent)
};

// Fraction: numerator / denominator with horizontal bar
struct FractionNode : LayoutNode {
    LayoutNode* numerator;
    LayoutNode* denominator;
    // width  = max(num.width, den.width) + 4px padding
    // height = num.height + bar(2px) + den.height + 2px gap
    // baseline = num.height + 1 (bar sits on baseline)
    void render(...) override;
};

// Superscript: base^exponent
struct SuperscriptNode : LayoutNode {
    LayoutNode* base;
    LayoutNode* exponent;  // Rendered at 75% font size, raised
    // width  = base.width + exp.width
    // height = max(base.height, exp.height + exp_raise)
    void render(...) override;
};

// Parenthesized group: ( child ) with auto-scaling parens
struct ParenNode : LayoutNode {
    LayoutNode* child;
    // Parens stretch vertically to match child.height
    void render(...) override;
};

// Square root: √(child) with overline
struct SqrtNode : LayoutNode {
    LayoutNode* child;
    // Radical symbol + horizontal overbar spanning child.width
    void render(...) override;
};

// Horizontal sequence: node1 node2 node3 ...
struct HBoxNode : LayoutNode {
    static constexpr int MAX_CHILDREN = 32;
    LayoutNode* children[MAX_CHILDREN];
    int count = 0;
    // width  = sum of children widths
    // height = max of children heights (baseline-aligned)
    void render(...) override;
};

} // namespace render
```

### 6.2 Layout builder

The layout builder parses an expression string and produces a layout tree. This is a **simplified** parser — it does not need to handle all of math, only the constructs you actually type on a calculator:

- Numbers and variables → `TextNode`
- `a/b` → `FractionNode` if both `a` and `b` are "simple" (single number, variable, or parenthesized group); otherwise, render as inline division `a/b`
- `a^b` → `SuperscriptNode`
- `sqrt(x)` → `SqrtNode`
- Parenthesized subexpressions → `ParenNode`
- Everything else (function names, operators, commas) → `TextNode` within an `HBoxNode`

**Phase 1 scope**: implement `TextNode`, `FractionNode`, `SuperscriptNode`, `ParenNode`, `HBoxNode`. Defer `SqrtNode` to Phase 2 (render `sqrt(x)` as text for now).

### 6.3 Memory management for layout nodes

Layout trees are short-lived (built for display, then discarded). Use a **pool allocator** backed by a static buffer:

```cpp
// Preallocate a pool for layout nodes (enough for complex expressions)
constexpr size_t LAYOUT_POOL_SIZE = 4096;  // ~4KB, fits ~50-80 nodes
static uint8_t layout_pool[LAYOUT_POOL_SIZE];
static size_t layout_pool_offset = 0;

template<typename T, typename... Args>
T* pool_new(Args&&... args) {
    // Bump allocator from layout_pool
    // Reset layout_pool_offset = 0 before each render pass
}
```

This avoids `new`/`delete` and heap fragmentation on a constrained target.

---

## 7. Application screens

### 7.1 Home screen (`apps/home_screen.hpp`)

The primary calculator interface. Layout:

```
┌──────────────────────────────────┐  y=16
│                                    │
│  Expression history (scrollable)   │
│  Shows previous entries and        │
│  results, pretty-printed           │
│                                    │
│   2+3                              │
│                            = 5     │
│   sin(pi/4)                        │
│                    = 0.7071067812  │
│                                    │
├──────────────────────────────────┤  y=268
│  Current input line                │
│  > 1/2 + 3^2|                      │
├──────────────────────────────────┤  y=296
│  F1:Y= F2:WIN F3:GRAPH F4:MODE    │
└──────────────────────────────────┘
```

**Behavior**:

- Typing characters appends to the input expression
- `ENTER` evaluates: result appears right-aligned below the expression, both are pretty-printed via the layout renderer, `Ans` is updated, and the input clears
- `UP`/`DOWN` scrolls through history
- History stores the last 50 expression/result pairs
- History persists to SD card (`/picocalc/history.txt`) on each evaluation

**Softkeys**: `Y=` (push Y editor), `WINDOW` (push window settings), `GRAPH` (push graph screen), `MODE` (angle mode toggle, display format), `VARS` (variable list), `CLEAR` (clear history).

### 7.2 Y= editor (`apps/y_editor.hpp`)

A list of 7 function slots ($Y_1$ through $Y_7$):

```
┌──────────────────────────────────┐
│  Y= Function Editor               │
├──────────────────────────────────┤
│  Y₁ = x^2 - 3              [✓]   │
│  Y₂ = sin(x)                [✓]   │
│  Y₃ = 2*x + 1              [ ]   │
│  Y₄ =                       [ ]   │
│  Y₅ =                       [ ]   │
│  Y₆ =                       [ ]   │
│  Y₇ =                       [ ]   │
├──────────────────────────────────┤
│  F1:EDIT F2:SEL F3:CLEAR F4:GRAPH│
└──────────────────────────────────┘
```

**Behavior**:

- `UP`/`DOWN` to navigate between Y-slots
- `ENTER` or `F1` to edit the selected slot (inline editing with cursor)
- `F2` toggles the checkbox (enabled/disabled for graphing)
- Functions are stored as strings and persisted to `/picocalc/yfuncs.txt`
- Variable `X` is the independent variable (case-insensitive matching)

### 7.3 Graph screen (`apps/graph_screen.hpp`)

Renders all enabled Y-functions on a coordinate plane.

**Graph viewport**: the full $320\times280$ content area (between status bar and softkeys).

```cpp
struct GraphWindow {
    double x_min = -10.0;
    double x_max =  10.0;
    double y_min = -10.0;
    double y_max =  10.0;
    double x_scl =  1.0;   // Grid line spacing
    double y_scl =  1.0;
};
```

**Rendering algorithm**:

```
for each enabled Y-function:
    for px = 0 to 319:                // Each pixel column
        x = x_min + px * (x_max - x_min) / 320.0
        y = evaluate(Y_expr, x)
        py = 280 - (y - y_min) / (y_max - y_min) * 280.0
        
        if py is within [0, 279]:
            plot pixel at (px, py + 16)  // +16 for status bar offset
        
        // Connect to previous point with a line segment
        if |py - prev_py| < 140:       // Discontinuity check
            draw_line(prev_px, prev_py, px, py)
```

Each Y-function gets a distinct color from a predefined palette: blue, red, green, magenta, orange, cyan, dark green.

**Axes and grid**:

- Draw x-axis and y-axis as solid lines if they're within the visible window
- Draw grid lines at $x_\text{scl}$ and $y_\text{scl}$ intervals as dotted or light gray lines
- Label axis tick marks with numeric values in the small font

**Interaction**:

- **Trace mode** (F1): a cursor moves along a selected function. `LEFT`/`RIGHT` moves the trace cursor by 1 pixel. The current $(x, y)$ coordinates display at the top or bottom of the graph. `UP`/`DOWN` switches between Y-functions.
- **Zoom In** (F2): halves the window range around the current center
- **Zoom Out** (F3): doubles the window range
- **Zoom Fit** (F4): auto-scales Y to fit the visible functions
- **WINDOW** (F5): push to window settings screen
- **Y=** (F6): push to Y editor

**Performance target**: full graph render (7 functions, 320 points each) in <50 ms on Pico 1, <20 ms on Pico 2. Based on ~400K evals/sec on RP2040, $7\times320$ = 2240 evaluations takes ~5.6 ms. Display rendering dominates at ~10–30 ms.

### 7.4 Window settings (`apps/window_screen.hpp`)

Simple form with editable number fields:

```
Xmin = -10       Xmax = 10
Ymin = -10       Ymax = 10
Xscl = 1         Yscl = 1
```

`UP`/`DOWN` to navigate fields, type to edit, `ENTER` to confirm. Persisted to `/picocalc/window.dat`.

**Preset zoom modes** (accessible from graph screen):

| Preset | $X_\text{min}$ | $X_\text{max}$ | $Y_\text{min}$ | $Y_\text{max}$ |
|--------|:-:|:-:|:-:|:-:|
| ZStandard | $-10$ | $10$ | $-10$ | $10$ |
| ZTrig | $-2\pi$ | $2\pi$ | $-4$ | $4$ |
| ZSquare | adjusted to make pixels square | | | |
| ZInteger | $-160$ | $160$ | $-140$ | $140$ |

---

## 8. Dual-core architecture

```
Core 0 (Application)              Core 1 (Display)
─────────────────────            ─────────────────────
main_loop() {                    display_loop() {
  kbd.poll()                       wait for render signal
  screen_mgr.handle_key(ev)       DMA line buffer → SPI LCD
  screen_mgr.render(fb)           signal completion
  fb.submit_to_core1()          }
  // continues immediately
  // (double-buffered)
}
```

Core 1 is launched via `multicore_launch_core1()` and enters a tight loop waiting on a multicore FIFO signal. When core 0 finishes rendering a strip (or a full frame), it pushes a signal to the FIFO. Core 1 then initiates the DMA transfer to the LCD's SPI bus and waits for DMA completion before signaling back.

This keeps the display refresh completely off the application core, ensuring responsive keyboard input even during graph rendering.

---

## 9. Task breakdown and schedule

Estimated as a **solo developer, part-time (~20 hrs/week)**. Total: ~8–10 weeks.

### Week 1–2: Bootstrap (HAL + build system)

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 1.1 | Set up repo, CMakeLists, dual-board build | 3 | `cmake -DPICO_BOARD=pico` and `pico2` both produce .uf2 |
| 1.2 | Vendor Coyote OS C drivers into `drivers/` | 2 | Compiles as C within C++ project |
| 1.3 | Implement `platform::Display` wrapper | 8 | Solid color fill visible on PicoCalc screen |
| 1.4 | Implement `platform::Keyboard` wrapper | 6 | Key events logged to USB serial |
| 1.5 | Implement `platform::Storage` wrapper | 4 | Read/write a test file on SD card |
| 1.6 | Implement `platform::Psram` wrapper | 3 | Allocate + read-back a 1KB test buffer |
| 1.7 | Implement line-buffer renderer + DMA on core 1 | 8 | 30+ fps color gradient animation on screen |
| 1.8 | Implement `gfx::Font` with $8\times16$ font | 4 | "Hello PicoCalc" displayed cleanly |
| 1.9 | Implement basic `ScreenManager` | 3 | Push/pop between two blank screens via F-keys |
| | **Subtotal** | **~41 hrs** | |

**Deliverable**: PicoCalc boots, shows text, responds to keyboard, reads/writes SD card.

### Week 3–4: Calculator core

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 2.1 | Integrate tinyexpr++ into `math::Engine` | 4 | `evaluate("2+3")` returns 5.0 |
| 2.2 | Register extended functions (trig, log, nCr, etc.) | 4 | `evaluate("nCr(10,3)")` returns 120 |
| 2.3 | Implement angle mode (degree/radian) | 2 | `sin(90)` returns 1.0 in degree mode |
| 2.4 | Implement `format_number()` | 3 | Correct formatting for integers, decimals, scientific |
| 2.5 | Build `HomeScreen` with input line + history | 10 | Type expression, press ENTER, see result |
| 2.6 | Implement `Variables` A–Z + Ans | 3 | `2→A` stores, `A+1` evaluates to 3 |
| 2.7 | Persist history + variables to SD card | 3 | Survives power cycle |
| 2.8 | Input line editing: cursor movement, insert, delete | 5 | Arrow keys move cursor, backspace works |
| | **Subtotal** | **~34 hrs** | |

**Deliverable**: a working scientific calculator. Type expressions, get results, use variables.

### Week 5–6: Natural math renderer (basic)

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 3.1 | Implement `TextNode`, `HBoxNode` | 4 | Inline expression renders correctly |
| 3.2 | Implement `FractionNode` (stacked fraction) | 6 | `1/2` renders as $\frac{1}{2}$ |
| 3.3 | Implement `SuperscriptNode` | 4 | `x^2` renders with raised exponent |
| 3.4 | Implement `ParenNode` with auto-scaling | 4 | Parentheses stretch to match content height |
| 3.5 | Build `LayoutBuilder` parser | 8 | Nested expressions like `(1+2)/(3^4)` render correctly |
| 3.6 | Integrate renderer into HomeScreen | 4 | History entries display as pretty math |
| 3.7 | Pool allocator for layout nodes | 2 | No heap allocation during rendering |
| | **Subtotal** | **~32 hrs** | |

**Deliverable**: expressions in history render as 2D typeset math.

### Week 7–8: Graphing

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 4.1 | Build `YEditorScreen` | 6 | Enter/edit/enable/disable Y-functions |
| 4.2 | Build `GraphScreen` — basic plot | 8 | Single function plots correctly with axes |
| 4.3 | Multi-function graphing with color palette | 3 | 7 functions render in distinct colors |
| 4.4 | Grid lines and axis labeling | 4 | Grid at Xscl/Yscl, numeric labels on axes |
| 4.5 | Discontinuity detection | 2 | Vertical asymptotes don't draw connecting lines |
| 4.6 | Trace cursor | 6 | Move along curve, display $(x, y)$ |
| 4.7 | Zoom in/out/fit/standard/trig presets | 4 | Each zoom mode works correctly |
| 4.8 | Build `WindowScreen` | 4 | Edit Xmin/Xmax/etc., changes reflected in graph |
| 4.9 | Persist Y-functions and window to SD | 2 | Survives power cycle |
| | **Subtotal** | **~39 hrs** | |

**Deliverable**: full function graphing with trace and zoom.

### Week 9–10: Polish and integration

| # | Task | Est. hours | Acceptance criteria |
|---|------|-----------|-------------------|
| 5.1 | Status bar (mode indicators, title) | 3 | 2nd, Alpha, DEG/RAD visible |
| 5.2 | Softkey bar integration across all screens | 3 | Context-sensitive labels, F-key handlers |
| 5.3 | Mode screen (angle, display format) | 3 | Toggle DEG/RAD, FIX/FLOAT/SCI display |
| 5.4 | Error handling (division by zero, syntax) | 3 | Graceful error messages, no crashes |
| 5.5 | Expression recall (use history as input) | 2 | UP key on empty input line recalls last expression |
| 5.6 | Performance profiling + optimization | 4 | Graph renders in <50ms on Pico 1 |
| 5.7 | Test on both Pico 1 and Pico 2 hardware | 4 | Both .uf2 files boot and function correctly |
| 5.8 | UF2 loader compatibility (F5 reboot) | 2 | Clean reboot to uf2loader from menu |
| 5.9 | README, build instructions | 2 | Someone else could build from source |
| | **Subtotal** | **~26 hrs** | |

**Deliverable**: release-quality Phase 1 firmware.

### Summary

| Phase | Weeks | Hours | Deliverable |
|-------|-------|-------|-------------|
| Bootstrap (HAL) | 1–2 | ~41 | Display + keyboard + SD working |
| Calculator core | 3–4 | ~34 | Scientific calculator |
| Math renderer | 5–6 | ~32 | Pretty-printed expressions |
| Graphing | 7–8 | ~39 | Multi-function graphing |
| Polish | 9–10 | ~26 | Integrated, tested firmware |
| **Total** | **~10 weeks** | **~172 hrs** | |

---

## 10. Key decisions and open questions

### Decided

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Language | C++17 | Best match for Pico SDK, DB48X reference, your preference |
| Expression parser | tinyexpr++ | Proven on PicoCalc, single-file, extensible |
| HAL source | Coyote OS drivers | Already working on PicoCalc, GPL-2.0 (fine for personal use) |
| Renderer architecture | Layout-node tree (Delta Pico `rbop` style) | Clean separation, recursive sizing/rendering, battle-tested |
| Display strategy | Line-buffer on Pico 1, full FB on Pico 2 | Maximizes SRAM availability on constrained target |
| User scripting (future) | MicroPython | Your preference, rich stdlib, `micropython-embed` API |
| Dual-core split | Core 0 = app, Core 1 = display DMA | Standard pattern, proven in PicoCalc emulator projects |

### Open (resolve during implementation)

| Question | Options | When to decide |
|----------|---------|---------------|
| Store operator for variables: TI-style `2→A` vs. `A=2`? | `→` requires special key mapping; `=` conflicts with comparison. Could use `:=` or `STO` key. | Week 3, during variable implementation |
| Fraction display threshold: always show `a/b` as stacked, or only when simple? | Always stacked (simpler code) vs. heuristic (inline for complex operands). | Week 5, during layout builder |
| Graph coordinate display: top of graph or bottom? | Top risks overlap with graph lines; bottom risks overlap with softkeys. TI-84 uses bottom. | Week 7, during trace implementation |
| History storage format: plaintext or binary? | Plaintext is debuggable and editable; binary saves parsing time. | Week 3, before persistence implementation |
| `double` vs `float` for graphing on Pico 1? | `float` is ~2x faster in softfloat but loses precision for large/small values. Could use `float` for graph point evaluation only. | Week 7, based on profiling |

---

## 11. References

1. Coyote OS source — https://github.com/laingcc/Picocalc-Coyote-OS
2. Coyote OS forum thread — https://forum.clockworkpi.com/t/coyote-os-calculator-firmware-for-picocalc/21130
3. Delta Pico source — https://github.com/AaronC81/delta-pico
4. Delta Pico `rbop` engine — https://github.com/AaronC81/rbop
5. Delta Pico writeup — https://aaronc.cc/2022/10/23/delta-pico.html
6. tinyexpr (C99) — https://github.com/codeplea/tinyexpr
7. DB48X source (C++, GPL v3) — https://github.com/c3d/db48x
8. ClockworkPi PicoCalc official repo — https://github.com/clockworkpi/PicoCalc
9. ST7365P datasheet — https://github.com/clockworkpi/PicoCalc/blob/master/ST7365P_SPEC_V1.0.pdf
10. ClockworkPi v2.0 mainboard schematic — https://github.com/clockworkpi/PicoCalc/blob/master/clockwork_Mainboard_V2.0_Schematic.pdf
11. PicoCalc Text Starter Kit (C/C++ drivers) — https://forum.clockworkpi.com/t/picocalc-text-starter-c-c/18451
12. Pico SDK floating-point docs — https://www.raspberrypi.com/documentation/pico-sdk/runtime.html
13. uf2loader (SD card bootloader) — https://github.com/pelrun/uf2loader
14. PicoCalc MicroPython driver — https://github.com/zenodante/PicoCalc-micropython-driver
