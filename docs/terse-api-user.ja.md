# Terse API ユーザーガイド

[English version is here](terse-api-user.md)

## 目次

1. [はじめに](#はじめに)
2. [クイックスタート](#クイックスタート)
3. [ライフサイクル管理](#ライフサイクル管理)
4. [エラーハンドリング](#エラーハンドリング)
5. [能力検出](#能力検出)
6. [P0: 基本的な出力](#p0-基本的な出力)
7. [P0: 入力イベント](#p0-入力イベント)
8. [P0: 状態管理](#p0-状態管理)
9. [P1: 色とスタイル](#p1-色とスタイル)
10. [P2: 高度な入出力](#p2-高度な入出力)
11. [P3: 拡張機能](#p3-拡張機能)
12. [テストモードとモック](#テストモードとモック)
13. [ベストプラクティス](#ベストプラクティス)
14. [一般的なパターン](#一般的なパターン)
15. [トラブルシューティング](#トラブルシューティング)

---

## はじめに

Terseは、ポータブルなターミナルUIアプリケーションを構築するためのCライブラリです。プロファイルベースのシステム(P0-P3)を提供し、ターミナルの能力を自動検出し、機能が利用できない場合には適切に機能を縮退させます。

### プロファイルレベル

- **P0**: 基本的な出力(カーソル移動、画面/行のクリア、テキスト出力、サイズ検出)
- **P1**: 色とテキスト装飾(SGRスタイル、16/256/TrueColor)
- **P2**: 高度な入出力(マウス、括弧付きペースト、タイトル、ハイパーリンク)
- **P3**: 拡張機能(クリップボード、画像、カーソル形状、通知)

### 中核となる概念

- **ハンドル (Handle)**: 不透明な `terse_handle_t` がターミナルセッションを表します
- **能力 (Capabilities)**: 実行時の検出と適切な機能縮退
- **状態管理 (State management)**: カーソル位置、可視性、スタイルの保存/復元
- **エラーハンドリング (Error handling)**: カテゴリ別に分類されたエラーコードを持つカスタム `terse_error_t` 列挙型

---

## クイックスタート

### 最小限の例

```c
#include "terse.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    // Configure options
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8",
        .disabled_caps = 0,
        .enabled_caps = 0
    };

    // Open session with automatic profile detection
    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) {
        fprintf(stderr, "Failed to open terse session\n");
        return 1;
    }

    // Clear screen and write text
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_move_to(handle, 5, 10);
    terse_write_text(handle, "Hello, Terminal!");
    terse_flush(handle);

    // Cleanup
    terse_close(handle);
    return 0;
}
```

### ビルド方法

```bash
cc -I./c/include -L./build/c -lterse example.c -o example
./example
```

---

## ライフサイクル管理

### セッションを開く

```c
terse_handle_t terse_open(terse_profile_t requested_profile,
                          const terse_options_t *options);
```

**パラメータ:**
- `requested_profile`: 要求するプロファイルレベル、または自動検出の場合は `TERSE_PROFILE_AUTO`
- `options`: 設定オプション(NULLの場合はデフォルトを使用)

**戻り値:** 成功時はハンドル、失敗時はNULL

**オプション構造体:**
```c
typedef struct terse_options {
    int input_fd;              // Input file descriptor (STDIN_FILENO)
    int output_fd;             // Output file descriptor (STDOUT_FILENO)
    const char *codec_name;    // Character encoding ("UTF-8" or "Shift_JIS")
    unsigned int disabled_caps; // Capabilities to disable
    unsigned int enabled_caps;  // Capabilities to enable
    int east_asian_ambiguous_as_wide; // Treat ambiguous-width chars as wide
} terse_options_t;
```

**オプションを使用した例:**
```c
terse_options_t options = {
    .input_fd = STDIN_FILENO,
    .output_fd = STDOUT_FILENO,
    .codec_name = "UTF-8",
    .disabled_caps = TERSE_CAP_DISABLE_MOUSE,  // Disable mouse
    .enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR, // Force TrueColor
    .east_asian_ambiguous_as_wide = 0
};

terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
```

**重要: ターミナルモードの管理**

`terse_open()` は、ターミナルを自動的にrawモードに設定**しません**。Terseは、独自の設定を強制するのではなく、アプリケーションのターミナル設定と協調するように設計されています。

- 出力のみの操作(カーソル移動、色、画面のクリア)では、rawモードは不要です
- 入力操作(`terse_read_event()`)では、`tcsetattr()` などを使用してターミナルモードを自分で設定する必要があります
- この設計により、アプリケーションは自身のニーズに適したターミナル設定(例: カスタムVMIN/VTIME、シグナル処理)を使用できます

入力処理のためのターミナルモード要件については、「P0: 入力イベント」セクションを参照してください。

### セッションを閉じる

```c
void terse_close(terse_handle_t handle);
```

自動的にターミナルの状態を復元します:
- カーソルの可視性を再有効化
- SGRスタイルをリセット
- マウストラッキングを無効化
- 括弧付きペーストを無効化

**例:**
```c
terse_close(handle);  // Always call on exit
```

### オプションの検証

```c
int terse_validate_options(const terse_options_t *options);
```

開く前にオプションを検証します。成功時は `TERSE_OK` を、エラー時は `terse_error_t` コードを返します。

---

## エラーハンドリング

### 戻り値

ほとんどの関数は `terse_error_t` 値を返します:
- 成功時は `TERSE_OK` (0)
- 失敗時は非ゼロの `terse_error_t` エラーコード

### エラーコード

```c
typedef enum terse_error {
    TERSE_OK = 0,

    /* Argument/Configuration Errors (1-99) */
    TERSE_ERR_INVALID_ARGUMENT = 1,
    TERSE_ERR_UNSUPPORTED = 2,
    TERSE_ERR_OVERFLOW = 3,

    /* State Errors (100-199) */
    TERSE_ERR_INVALID_HANDLE = 100,
    TERSE_ERR_NOT_IMPLEMENTED = 101,
    TERSE_ERR_STACK_OVERFLOW = 102,
    TERSE_ERR_STACK_UNDERFLOW = 103,

    /* I/O Transport Errors (200-299) */
    TERSE_ERR_IO = 200,
    TERSE_ERR_WOULD_BLOCK = 201,
    TERSE_ERR_NOT_TTY = 202,

    /* Protocol Errors (300-399) */
    TERSE_ERR_PROTOCOL = 300,

    /* Resource Errors (400-499) */
    TERSE_ERR_OUT_OF_MEMORY = 400,

    /* Encoding Errors (500-599) */
    TERSE_ERR_INVALID_ENCODING = 500,
    TERSE_ERR_BUFFER_TOO_SMALL = 501,
} terse_error_t;
```

エラーコードはカテゴリ別に範囲で整理されています:
- **1-99**: 引数/設定エラー(無効なパラメータ、サポートされていない操作)
- **100-199**: 状態エラー(無効なハンドル、スタックのオーバー/アンダーフロー)
- **200-299**: I/O転送エラー(I/O失敗、ブロッキング操作、TTY問題)
- **300-399**: プロトコルエラー(ターミナルプロトコル違反)
- **400-499**: リソースエラー(メモリ割り当て失敗)
- **500-599**: エンコーディングエラー(文字セット変換問題、バッファサイズの問題)

### 最後のエラーを取得

```c
terse_error_t terse_get_last_error(terse_handle_t handle);
```

このハンドルで発生した最後のエラーコードを返します。エラーがない場合は `TERSE_OK` を返します。

**例:**
```c
terse_error_t err = terse_move_to(handle, 10, 5);
if (err != TERSE_OK) {
    fprintf(stderr, "move_to failed: error code %d\n", err);

    // Can also retrieve the last error later
    terse_error_t last_err = terse_get_last_error(handle);
    fprintf(stderr, "last error: %d\n", last_err);
}
```

---

## 能力検出

### 能力のクエリ

```c
terse_capabilities_t terse_get_capabilities(terse_handle_t handle);
```

現在の能力構造体を返します。

**能力構造体:**
```c
typedef struct terse_capabilities {
    terse_profile_t profile;
    int has_basic_output;
    int has_cursor_visibility;
    int has_move_absolute;
    int has_move_relative;
    int has_clear_line;
    int has_clear_screen;
    int has_size;
    int has_sgr_basic;
    int has_sgr_extended;
    int has_truecolor;
    int has_text_styles;
    terse_mouse_mode_t mouse;
    int has_bracketed_paste;
    int has_title;
    int has_hyperlinks;
    int has_cursor_shape;
    terse_color_support_t colors;
    unsigned int effects;
    int has_clipboard_write;
    terse_image_support_t images;
    unsigned int notifications;
    unsigned int keyboard_features;
} terse_capabilities_t;
```

**例:**
```c
terse_capabilities_t caps = terse_get_capabilities(handle);

if (caps.colors >= TERSE_COLOR_TRUECOLOR) {
    // Use 24-bit RGB colors
} else if (caps.colors >= TERSE_COLOR_PALETTE256) {
    // Use 256-color palette
} else if (caps.colors >= TERSE_COLOR_BASIC16) {
    // Use basic 16 colors
}

if (caps.mouse >= TERSE_MOUSE_SGR) {
    terse_enable_mouse(handle, TERSE_MOUSE_SGR);
}
```

### 実行時の能力オーバーライド

```c
int terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask);
int terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask);
int terse_capabilities_reset_overrides(terse_handle_t handle);
```

**有効化フラグ:**
- `TERSE_CAP_ENABLE_SGR_BASIC`
- `TERSE_CAP_ENABLE_TEXT_STYLES`
- `TERSE_CAP_ENABLE_SGR_EXTENDED`
- `TERSE_CAP_ENABLE_TRUECOLOR`
- `TERSE_CAP_ENABLE_MOUSE`
- `TERSE_CAP_ENABLE_BRACKETED_PASTE`
- `TERSE_CAP_ENABLE_TITLE`
- `TERSE_CAP_ENABLE_HYPERLINK`
- `TERSE_CAP_ENABLE_CURSOR_SHAPE`
- `TERSE_CAP_ENABLE_CLIPBOARD_WRITE`
- `TERSE_CAP_ENABLE_IMAGE_INLINE`
- `TERSE_CAP_ENABLE_NOTIFICATION_*`

**無効化フラグ:**
- `TERSE_CAP_DISABLE_*` (対応する無効化フラグ)

**例:**
```c
// Force-enable TrueColor
terse_capabilities_enable(handle, TERSE_CAP_ENABLE_TRUECOLOR);

// Disable mouse support
terse_capabilities_disable(handle, TERSE_CAP_DISABLE_MOUSE);

// Restore automatic detection
terse_capabilities_reset_overrides(handle);
```

---

## P0: 基本的な出力

### 画面のクリア

```c
int terse_clear_screen(terse_handle_t handle, terse_clear_mode_t mode);
int terse_clear_line(terse_handle_t handle, terse_clear_mode_t mode);
```

**クリアモード:**
- `TERSE_CLEAR_AFTER`: カーソルから末尾までクリア
- `TERSE_CLEAR_BEFORE`: 先頭からカーソルまでクリア
- `TERSE_CLEAR_ALL`: 画面/行全体をクリア

**例:**
```c
terse_clear_screen(handle, TERSE_CLEAR_ALL);  // Clear entire screen
terse_clear_line(handle, TERSE_CLEAR_AFTER);  // Clear to end of line
```

### カーソルの移動

```c
int terse_move_to(terse_handle_t handle, int row, int col);
int terse_move_by(terse_handle_t handle, int drow, int dcol);
```

**座標系:** 0ベース(row=0, col=0が左上)

**重要:** Terseは、多くのプログラミング言語と同様に0ベースの座標を使用します。
- 左上隅: `(0, 0)`
- 最初の列: `col=0`
- 最初の行: `row=0`
- 最後の行: `size.rows - 1`
- 最後の列: `size.cols - 1`

**注意:** ターミナルのエスケープシーケンスは内部的に1ベースの座標を使用しますが、ライブラリがこの変換を自動的に処理します。

**例:**
```c
terse_move_to(handle, 9, 19);   // Move to row 9, column 19 (10th row, 20th column)
terse_move_by(handle, 2, -5);   // Move down 2, left 5

// Move to top-left corner
terse_move_to(handle, 0, 0);

// Move to beginning of row 4 (5th row)
terse_move_to(handle, 4, 0);
```

### カーソルの可視性

```c
int terse_show_cursor(terse_handle_t handle, int visible);
```

**例:**
```c
terse_show_cursor(handle, 0);  // Hide cursor
// ... draw UI ...
terse_show_cursor(handle, 1);  // Show cursor
```

### テキスト出力

```c
int terse_write_text(terse_handle_t handle, const char *graphemes);
int terse_flush(terse_handle_t handle);
```

**注意:**
- `graphemes`: UTF-8またはShift_JIS文字列(コーデックに依存)
- `terse_flush()`: 現在は何もしない(出力はバッファリングされていない)

**例:**
```c
terse_write_text(handle, "Hello, 世界!");
terse_flush(handle);
```

### ターミナルサイズ

```c
terse_size_t terse_get_size(terse_handle_t handle);
```

**戻り値:**
```c
typedef struct terse_size {
    int rows;
    int cols;
    int known;  // 0 if size is unknown
} terse_size_t;
```

**例:**
```c
terse_size_t size = terse_get_size(handle);
if (size.known) {
    printf("Terminal: %d rows x %d cols\n", size.rows, size.cols);

    // Remember: coordinates are 0-based
    // Bottom-right corner is at (size.rows-1, size.cols-1)
    terse_move_to(handle, size.rows - 1, size.cols - 1);  // Move to bottom-right
}
```

### カーソル位置

```c
terse_cursor_position_t terse_get_cursor_position(terse_handle_t handle);
```

**戻り値:**
```c
typedef struct terse_cursor_position {
    int row;
    int col;
    int known;  // 0 if position is unknown
} terse_cursor_position_t;
```

**例:**
```c
terse_cursor_position_t pos = terse_get_cursor_position(handle);
if (pos.known) {
    printf("Cursor at: %d, %d\n", pos.row, pos.col);
}
```

---

## P0: 入力イベント

### イベントの読み取り

```c
int terse_read_event(terse_handle_t handle, int timeout_ms,
                     terse_event_t *out_event);
```

**パラメータ:**
- `timeout_ms`: タイムアウト(ミリ秒単位、-1でブロッキング)
- `out_event`: 出力イベント構造体

**戻り値:**
- `TERSE_OK` (0): イベントが正常に受信された
- `TERSE_ERR_NO_EVENT` (1): タイムアウト、イベントなし
- その他の `terse_error_t` 値: エラーが発生した

**ターミナルモードの要件:**

`terse_read_event()` は、非正規入力に適したモードにターミナルが設定されている必要があります。Terseはターミナルモードを自動的に設定**しません** - これはアプリケーションの責任です。

典型的な要件:
- 非正規モード(ICANONを無効化)
- エコーなし(ECHOを無効化)
- 希望するブロッキング動作に合わせてVMINとVTIMEを設定

**例: Rawモードの設定**
```c
#include <termios.h>

int setup_raw_mode(int fd) {
    struct termios raw;
    if (tcgetattr(fd, &raw) < 0) {
        return -1;
    }

    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    return tcsetattr(fd, TCSAFLUSH, &raw);
}

// Before using terse_read_event():
setup_raw_mode(STDIN_FILENO);
```

**注意:** ターミナルモード管理の完全な例については、`samples/event_logger_demo.c` を参照してください。

**イベントタイプ:**
```c
typedef enum terse_event_type {
    TERSE_EVENT_CHAR,
    TERSE_EVENT_ENTER,
    TERSE_EVENT_BACKSPACE,
    TERSE_EVENT_TAB,
    TERSE_EVENT_ARROW_UP,
    TERSE_EVENT_ARROW_DOWN,
    TERSE_EVENT_ARROW_LEFT,
    TERSE_EVENT_ARROW_RIGHT,
    TERSE_EVENT_HOME,
    TERSE_EVENT_END,
    TERSE_EVENT_PAGE_UP,
    TERSE_EVENT_PAGE_DOWN,
    TERSE_EVENT_INSERT,
    TERSE_EVENT_DELETE,
    TERSE_EVENT_FUNCTION,
    TERSE_EVENT_MOUSE_DOWN,
    TERSE_EVENT_MOUSE_UP,
    TERSE_EVENT_MOUSE_MOVE,
    TERSE_EVENT_MOUSE_SCROLL,
    TERSE_EVENT_PASTE_BEGIN,
    TERSE_EVENT_PASTE_END,
    TERSE_EVENT_RESIZE,
    TERSE_EVENT_RAW_SEQUENCE
} terse_event_type_t;
```

### イベント構造体

```c
typedef struct terse_event {
    terse_event_type_t type;
    union {
        struct {
            unsigned int scalar;  // Unicode codepoint
            int width;            // Display width (0, 1, or 2)
            int mods;             // Modifier keys
        } ch;
        struct {
            int mods;
        } key;
        struct {
            int mods;
            int number;           // Function key number (F1-F12)
        } function;
        struct {
            int rows;
            int cols;
        } resize;
        struct {
            terse_mouse_button_t button;
            int mods;
            int row;
            int col;
        } mouse;
        struct {
            size_t length;
            unsigned char bytes[TERSE_EVENT_RAW_MAX];
        } raw;
    } data;
} terse_event_t;
```

**修飾キーフラグ:**
```c
enum {
    TERSE_MOD_SHIFT = (1 << 0),
    TERSE_MOD_CTRL = (1 << 1),
    TERSE_MOD_ALT = (1 << 2),
    TERSE_MOD_META = (1 << 3),
};
```

### イベントループの例

```c
while (1) {
    terse_event_t event;
    int result = terse_read_event(handle, 100, &event);

    if (result == TERSE_ERR_NO_EVENT) {
        continue;  // Timeout
    }
    if (result < 0) {
        break;  // Error
    }

    switch (event.type) {
        case TERSE_EVENT_CHAR:
            if (event.data.ch.scalar == 'q') {
                return;  // Quit
            }
            printf("Char: %u (width=%d, mods=0x%x)\n",
                   event.data.ch.scalar,
                   event.data.ch.width,
                   event.data.ch.mods);
            break;

        case TERSE_EVENT_ARROW_UP:
            printf("Arrow Up (mods=0x%x)\n", event.data.key.mods);
            break;

        case TERSE_EVENT_RESIZE:
            printf("Resize: %dx%d\n",
                   event.data.resize.rows,
                   event.data.resize.cols);
            break;

        case TERSE_EVENT_RAW_SEQUENCE:
            printf("Raw sequence: %zu bytes\n", event.data.raw.length);
            break;
    }
}
```

### キーボード機能の拡張

```c
int terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask);
int terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask);
unsigned int terse_keyboard_get_enabled(terse_handle_t handle);
unsigned int terse_keyboard_get_supported(terse_handle_t handle);
```

**機能:**
```c
typedef enum terse_keyboard_feature {
    TERSE_KEYBOARD_FEATURE_NONE = 0,
    TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS = 1 << 0,  // xterm modifyOtherKeys
    TERSE_KEYBOARD_FEATURE_KITTY_PROTOCOL = 1 << 1,     // kitty keyboard protocol
} terse_keyboard_feature_t;
```

**例:**
```c
// Check support
unsigned int supported = terse_keyboard_get_supported(handle);
if (supported & TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS) {
    // Enable enhanced key reporting
    terse_keyboard_enable(handle, TERSE_KEYBOARD_FEATURE_MODIFY_OTHER_KEYS);
}
```

---

## P0: 状態管理

### キャプチャと復元

```c
int terse_capture_state(terse_handle_t handle, terse_state_t *out_state);
int terse_restore_state(terse_handle_t handle, const terse_state_t *state);
```

**状態構造体:**
```c
typedef struct terse_state {
    int cursor_known;
    int cursor_visible;
    int cursor_row;
    int cursor_col;
    int style_known;
    terse_style_t style;
} terse_state_t;
```

**例:**
```c
terse_state_t saved_state;

// Save current state
terse_capture_state(handle, &saved_state);

// Modify terminal
terse_move_to(handle, 1, 1);
terse_show_cursor(handle, 0);
// ... draw UI ...

// Restore original state
terse_restore_state(handle, &saved_state);
```

### 状態スタックのプッシュとポップ

```c
int terse_push_state(terse_handle_t handle);
int terse_pop_state(terse_handle_t handle);
```

**スタック深度:** 最大8レベル

**例:**
```c
// Save state on stack
terse_push_state(handle);

// Temporarily modify
terse_move_to(handle, 5, 10);
terse_write_text(handle, "Temporary message");

// Restore from stack
terse_pop_state(handle);
```

### 状態のオーバーライド

```c
int terse_state_override(terse_handle_t handle, const terse_state_t *state);
int terse_state_clear(terse_handle_t handle);
```

**例:**
```c
terse_state_t custom_state = {
    .cursor_known = 1,
    .cursor_visible = 1,
    .cursor_row = 10,
    .cursor_col = 5,
    .style_known = 0
};

terse_state_override(handle, &custom_state);
// ... operations ...
terse_state_clear(handle);  // Clear override
```

---

## P1: 色とスタイル

### 色の作成

```c
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);
```

**基本色:**
```c
typedef enum terse_basic_color {
    TERSE_BASIC_COLOR_BLACK = 0,
    TERSE_BASIC_COLOR_RED,
    TERSE_BASIC_COLOR_GREEN,
    TERSE_BASIC_COLOR_YELLOW,
    TERSE_BASIC_COLOR_BLUE,
    TERSE_BASIC_COLOR_MAGENTA,
    TERSE_BASIC_COLOR_CYAN,
    TERSE_BASIC_COLOR_WHITE
} terse_basic_color_t;
```

**例:**
```c
// Terminal default color
terse_color_t default_fg = terse_color_default();

// Basic 16 colors
terse_color_t red = terse_color_basic(TERSE_BASIC_COLOR_RED, 0);
terse_color_t bright_yellow = terse_color_basic(TERSE_BASIC_COLOR_YELLOW, 1);

// 256-color palette
terse_color_t palette_color = terse_color_palette(196);  // Bright red

// TrueColor (24-bit RGB)
terse_color_t orange = terse_color_truecolor(255, 165, 0);
```

### スタイル管理

```c
terse_style_t terse_style_default(void);
int terse_set_style(terse_handle_t handle, const terse_style_t *style);
int terse_reset_style(terse_handle_t handle, terse_reset_scope_t scope);
```

**スタイル構造体:**
```c
typedef struct terse_style {
    terse_color_t foreground;
    terse_color_t background;
    unsigned int effects;
} terse_style_t;
```

**テキスト効果:**
```c
enum {
    TERSE_STYLE_BOLD = 1 << 0,
    TERSE_STYLE_FAINT = 1 << 1,
    TERSE_STYLE_ITALIC = 1 << 2,
    TERSE_STYLE_UNDERLINE = 1 << 3,
    TERSE_STYLE_INVERSE = 1 << 4,
    TERSE_STYLE_BLINK = 1 << 5,
    TERSE_STYLE_STRIKE = 1 << 6
};
```

**リセットスコープ:**
```c
typedef enum terse_reset_scope {
    TERSE_RESET_ALL = 0,         // Reset colors and effects
    TERSE_RESET_COLOR_ONLY,      // Reset colors only
    TERSE_RESET_EFFECTS_ONLY     // Reset effects only
} terse_reset_scope_t;
```

### 完全なスタイルの例

```c
// Create style with TrueColor and effects
terse_style_t style = terse_style_default();
style.foreground = terse_color_truecolor(255, 100, 50);
style.background = terse_color_truecolor(20, 20, 40);
style.effects = TERSE_STYLE_BOLD | TERSE_STYLE_ITALIC;

// Apply style
terse_set_style(handle, &style);
terse_write_text(handle, "Styled text");

// Reset to default
terse_reset_style(handle, TERSE_RESET_ALL);
```

### 色の縮退

ライブラリは、ターミナルの能力に基づいて自動的に色を縮退させます:

```c
terse_capabilities_t caps = terse_get_capabilities(handle);

terse_color_t color = terse_color_truecolor(255, 128, 0);  // Orange

// Automatically degrades:
// - TrueColor terminal: RGB(255, 128, 0)
// - 256-color terminal: Closest palette match
// - 16-color terminal: Basic yellow/bright yellow
// - No color: Ignored
```

### カラーパレットの例

```c
// Rainbow gradient using 256-color palette
for (int i = 0; i < 256; i++) {
    terse_style_t style = terse_style_default();
    style.background = terse_color_palette(i);
    terse_set_style(handle, &style);
    terse_write_text(handle, " ");
}
terse_reset_style(handle, TERSE_RESET_ALL);
```

---

## P2: 高度な入出力

### マウストラッキング

```c
int terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode);
int terse_disable_mouse(terse_handle_t handle);
```

**マウスモード:**
```c
typedef enum terse_mouse_mode {
    TERSE_MOUSE_NONE = 0,
    TERSE_MOUSE_X10,      // Button press only
    TERSE_MOUSE_VT200,    // Press and release
    TERSE_MOUSE_SGR       // Press, release, move, scroll (recommended)
} terse_mouse_mode_t;
```

**マウスボタン:**
```c
typedef enum terse_mouse_button {
    TERSE_MOUSE_BUTTON_NONE = 0,
    TERSE_MOUSE_BUTTON_LEFT,
    TERSE_MOUSE_BUTTON_MIDDLE,
    TERSE_MOUSE_BUTTON_RIGHT,
    TERSE_MOUSE_BUTTON_SCROLL_UP,
    TERSE_MOUSE_BUTTON_SCROLL_DOWN
} terse_mouse_button_t;
```

**例:**
```c
// Enable SGR mouse tracking
terse_enable_mouse(handle, TERSE_MOUSE_SGR);

while (1) {
    terse_event_t event;
    if (terse_read_event(handle, -1, &event) != TERSE_OK) {
        break;
    }

    switch (event.type) {
        case TERSE_EVENT_MOUSE_DOWN:
            printf("Mouse down: button=%d, pos=(%d,%d), mods=0x%x\n",
                   event.data.mouse.button,
                   event.data.mouse.row,
                   event.data.mouse.col,
                   event.data.mouse.mods);
            break;

        case TERSE_EVENT_MOUSE_MOVE:
            printf("Mouse move: pos=(%d,%d)\n",
                   event.data.mouse.row,
                   event.data.mouse.col);
            break;

        case TERSE_EVENT_MOUSE_SCROLL:
            if (event.data.mouse.button == TERSE_MOUSE_BUTTON_SCROLL_UP) {
                printf("Scroll up\n");
            } else {
                printf("Scroll down\n");
            }
            break;
    }
}

// Disable mouse tracking
terse_disable_mouse(handle);
```

### 括弧付きペースト

```c
int terse_enable_bracketed_paste(terse_handle_t handle);
int terse_disable_bracketed_paste(terse_handle_t handle);
```

括弧付きペーストモードにより、ペーストされたテキストとタイプされたテキストを区別できます。

**例:**
```c
terse_enable_bracketed_paste(handle);

int in_paste = 0;
while (1) {
    terse_event_t event;
    if (terse_read_event(handle, -1, &event) != TERSE_OK) {
        break;
    }

    switch (event.type) {
        case TERSE_EVENT_PASTE_BEGIN:
            printf("Paste started\n");
            in_paste = 1;
            break;

        case TERSE_EVENT_PASTE_END:
            printf("Paste ended\n");
            in_paste = 0;
            break;

        case TERSE_EVENT_CHAR:
            if (in_paste) {
                printf("Pasted char: %u\n", event.data.ch.scalar);
            } else {
                printf("Typed char: %u\n", event.data.ch.scalar);
            }
            break;
    }
}

terse_disable_bracketed_paste(handle);
```

### ウィンドウタイトル

```c
int terse_set_title(terse_handle_t handle, const char *title);
```

**注意:** `title` に ESC (0x1B) または BEL (0x07) を含めることはできません。含まれている場合は `TERSE_ERR_INVALID_ARGUMENT` を返します。

**例:**
```c
terse_set_title(handle, "My Awesome TUI App v1.0");

// Clear title
terse_set_title(handle, "");
```

### ハイパーリンク

```c
int terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label);
```

サポートされているターミナル(iTerm2、現代的なターミナル)でクリック可能なハイパーリンクを作成します。
**注意:** `url` と `label` に ESC (0x1B) または BEL (0x07) を含めることはできません。含まれている場合は `TERSE_ERR_INVALID_ARGUMENT` を返します。

**例:**
```c
// Create hyperlink
terse_set_hyperlink(handle, "https://github.com/", "GitHub");

// End hyperlink
terse_set_hyperlink(handle, "", "");

// Full example
terse_write_text(handle, "Visit ");
terse_set_hyperlink(handle, "https://example.com", "example.com");
terse_set_hyperlink(handle, "", "");
terse_write_text(handle, " for more info.");
```

---

## P3: 拡張機能

### クリップボード

```c
int terse_set_clipboard(terse_handle_t handle, const char *data);
```

OSC 52を使用してシステムクリップボードにテキストを書き込みます。
**注意:** `data` に ESC (0x1B) または BEL (0x07) を含めることはできません。含まれている場合は `TERSE_ERR_INVALID_ARGUMENT` を返します。

**例:**
```c
const char *text = "Copied to clipboard!";
if (terse_set_clipboard(handle, text) == 0) {
    printf("Clipboard updated\n");
} else {
    printf("Clipboard not supported\n");
}
```

**注意:** クリップボードの読み取りはサポートされていません(ターミナルの制限)。

### カーソル形状

```c
int terse_set_cursor_shape(terse_handle_t handle,
                           terse_cursor_shape_t shape,
                           int blinking);
```

**カーソル形状:**
```c
typedef enum terse_cursor_shape {
    TERSE_CURSOR_SHAPE_DEFAULT = 0,
    TERSE_CURSOR_SHAPE_BLOCK,
    TERSE_CURSOR_SHAPE_UNDERLINE,
    TERSE_CURSOR_SHAPE_BAR
} terse_cursor_shape_t;
```

**例:**
```c
// Blinking bar cursor
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_BAR, 1);

// Steady block cursor
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_BLOCK, 0);

// Restore default
terse_set_cursor_shape(handle, TERSE_CURSOR_SHAPE_DEFAULT, 1);
```

### 画像表示

```c
int terse_display_image(terse_handle_t handle,
                        const terse_image_request_t *request);
```

**画像リクエスト構造体:**
```c
typedef struct terse_image_request {
    const unsigned char *data;  // Image data
    size_t size;                // Data size in bytes
    const char *name;           // Optional filename (can be NULL)
    terse_image_format_t format;// Image format
    int width;                  // Optional width hint (0 = auto)
    int height;                 // Optional height hint (0 = auto)
    unsigned int flags;         // Display flags
} terse_image_request_t;
```

**注意:** `name` に ESC (0x1B) または BEL (0x07) を含めることはできません。含まれている場合は `TERSE_ERR_INVALID_ARGUMENT` を返します。

**画像フォーマット:**
```c
typedef enum terse_image_format {
    TERSE_IMAGE_FORMAT_AUTO = 0,  // Auto-detect from data
    TERSE_IMAGE_FORMAT_PNG,
    TERSE_IMAGE_FORMAT_JPEG,
    TERSE_IMAGE_FORMAT_SIXEL,
    TERSE_IMAGE_FORMAT_KITTY
} terse_image_format_t;
```

**画像フラグ:**
```c
enum {
    TERSE_IMAGE_FLAG_INLINE = 1 << 0,          // Display inline
    TERSE_IMAGE_FLAG_ALLOW_DEGRADE = 1 << 1    // Allow format degradation
};
```

**サポートされているプロトコル:**
```c
typedef enum terse_image_support {
    TERSE_IMAGE_NONE = 0,
    TERSE_IMAGE_ITERM_INLINE,  // iTerm2 inline images
    TERSE_IMAGE_SIXEL,         // Sixel graphics
    TERSE_IMAGE_KITTY          // Kitty graphics protocol
} terse_image_support_t;
```

**例:**
```c
// Load PNG image data
unsigned char *png_data;
size_t png_size;
// ... load image file ...

terse_image_request_t request = {
    .data = png_data,
    .size = png_size,
    .name = "example.png",
    .format = TERSE_IMAGE_FORMAT_PNG,
    .width = 0,   // Auto width
    .height = 0,  // Auto height
    .flags = TERSE_IMAGE_FLAG_INLINE | TERSE_IMAGE_FLAG_ALLOW_DEGRADE
};

if (terse_display_image(handle, &request) == 0) {
    printf("Image displayed\n");
} else {
    printf("Image display failed or not supported\n");
}
```

**レガシーAPI:**
```c
int terse_display_image_inline(terse_handle_t handle,
                               const unsigned char *data,
                               size_t size,
                               const char *name);
```

後方互換性のための `terse_display_image()` のシンプルなラッパー。

### 通知

```c
int terse_notify(terse_handle_t handle,
                 terse_notification_kind_t kind,
                 const char *payload);
```

**注意:** `TERSE_NOTIFICATION_KIND_DESKTOP` の `payload` に ESC (0x1B) または BEL (0x07) を含めることはできません。含まれている場合は `TERSE_ERR_INVALID_ARGUMENT` を返します。

**通知タイプ:**
```c
typedef enum terse_notification_kind {
    TERSE_NOTIFICATION_KIND_BELL = 0,     // Terminal bell
    TERSE_NOTIFICATION_KIND_VISUAL,       // Visual bell (flash)
    TERSE_NOTIFICATION_KIND_DESKTOP       // Desktop notification
} terse_notification_kind_t;
```

**サポートフラグ:**
```c
enum {
    TERSE_NOTIFICATION_SUPPORT_BELL = 1 << 0,
    TERSE_NOTIFICATION_SUPPORT_VISUAL = 1 << 1,
    TERSE_NOTIFICATION_SUPPORT_DESKTOP = 1 << 2
};
```

**例:**
```c
// Terminal bell
terse_notify(handle, TERSE_NOTIFICATION_KIND_BELL, NULL);

// Visual bell (screen flash)
terse_notify(handle, TERSE_NOTIFICATION_KIND_VISUAL, NULL);

// Desktop notification
terse_notify(handle, TERSE_NOTIFICATION_KIND_DESKTOP,
             "Build completed successfully!");

// Check support
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.notifications & TERSE_NOTIFICATION_SUPPORT_DESKTOP) {
    terse_notify(handle, TERSE_NOTIFICATION_KIND_DESKTOP,
                 "Important message");
}
```

---

## テストモードとモック

### 概要

Terseは、自動テストと検証のためのテストモードを提供します。ビルド時に有効にすると、ライブラリはAPI呼び出しを記録し、能力、ターミナルサイズ、入力イベントのモックを可能にします。

### テストモードでのビルド

```bash
cmake -S . -B build -DTERSE_ENABLE_TEST_MODE=ON -G Ninja
ninja -C build
```

**注意:** テストモードは、本番ビルドではデフォルトで無効になっています。

### API呼び出しの記録

テストモードは、検証のためにすべてのレンダリングと制御API呼び出しを記録できます:

```c
#ifdef TERSE_ENABLE_TEST_MODE
#include "terse_test.h"

// Start recording
terse_test_start_recording(handle);

// Execute rendering operations
terse_move_to(handle, 5, 10);
terse_write_text(handle, "Hello");
terse_clear_screen(handle, TERSE_CLEAR_ALL);

// Stop recording
terse_test_stop_recording(handle);

// Retrieve recorded calls
int count = 0;
const terse_call_record_t *calls = terse_test_get_calls(handle, &count);

// Verify calls
assert(count == 3);
assert(calls[0].type == TERSE_CALL_MOVE_TO);
assert(calls[0].data.move_to.row == 5);
assert(calls[0].data.move_to.col == 10);

assert(calls[1].type == TERSE_CALL_WRITE_TEXT);
assert(strcmp(calls[1].data.write_text.text, "Hello") == 0);

assert(calls[2].type == TERSE_CALL_CLEAR_SCREEN);
assert(calls[2].data.clear_screen.mode == TERSE_CLEAR_ALL);

// Clear recorded calls
terse_test_clear_calls(handle);
#endif
```

**記録される呼び出しタイプ:**
- `TERSE_CALL_WRITE_TEXT`
- `TERSE_CALL_MOVE_TO`
- `TERSE_CALL_CLEAR_SCREEN`
- `TERSE_CALL_CLEAR_LINE`
- `TERSE_CALL_SHOW_CURSOR`
- `TERSE_CALL_SET_STYLE`
- `TERSE_CALL_ENABLE_MOUSE`
- `TERSE_CALL_DISABLE_MOUSE`
- `TERSE_CALL_SET_TITLE`
- `TERSE_CALL_FLUSH`

### 能力のモック

テスト用にターミナル能力をオーバーライド:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Create mock capabilities
terse_capabilities_t mock_caps = {
    .profile = TERSE_P1,
    .colors = TERSE_COLOR_BASIC16,
    .mouse = TERSE_MOUSE_NONE,
    .images = TERSE_IMAGE_NONE
};

// Apply mock
terse_test_mock_capabilities(handle, &mock_caps);

// Test behavior with limited capabilities
terse_capabilities_t caps = terse_get_capabilities(handle);
assert(caps.colors == TERSE_COLOR_BASIC16);
assert(caps.mouse == TERSE_MOUSE_NONE);

// Reset to actual capabilities
terse_test_reset_mocks(handle);
#endif
```

### ターミナルサイズのモック

異なるターミナルサイズでUIレイアウトをテスト:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Mock 80x24 terminal
terse_test_mock_size(handle, 24, 80);

terse_size_t size = terse_get_size(handle);
assert(size.rows == 24);
assert(size.cols == 80);
assert(size.known == 1);

// Mock 120x40 terminal
terse_test_mock_size(handle, 40, 120);

size = terse_get_size(handle);
assert(size.rows == 40);
assert(size.cols == 120);

// Reset to actual size
terse_test_reset_mocks(handle);
#endif
```

### 入力イベントのモック

イベント処理のテスト用に合成入力イベントを注入:

```c
#ifdef TERSE_ENABLE_TEST_MODE
// Prepare synthetic events
terse_event_t events[3];

// Event 1: Character 'a'
events[0].type = TERSE_EVENT_CHAR;
events[0].data.ch.scalar = 'a';
events[0].data.ch.width = 1;
events[0].data.ch.mods = 0;

// Event 2: Arrow Up with Shift
events[1].type = TERSE_EVENT_ARROW_UP;
events[1].data.key.mods = TERSE_MOD_SHIFT;

// Event 3: Resize to 25x80
events[2].type = TERSE_EVENT_RESIZE;
events[2].data.resize.rows = 25;
events[2].data.resize.cols = 80;

// Inject events
terse_test_mock_events(handle, events, 3);

// Read injected events
terse_event_t event;
assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_CHAR);
assert(event.data.ch.scalar == 'a');

assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_ARROW_UP);
assert(event.data.key.mods == TERSE_MOD_SHIFT);

assert(terse_read_event(handle, 0, &event) == TERSE_OK);
assert(event.type == TERSE_EVENT_RESIZE);
assert(event.data.resize.rows == 25);

// No more events
assert(terse_read_event(handle, 0, &event) == TERSE_ERR_NO_EVENT);

// Reset to real input
terse_test_reset_mocks(handle);
#endif
```

### ユースケース

**1. UIリグレッションテスト:**
```c
// Record baseline rendering
terse_test_start_recording(handle);
render_ui(handle);
terse_test_stop_recording(handle);
int baseline_count;
const terse_call_record_t *baseline = terse_test_get_calls(handle, &baseline_count);
// Save baseline for comparison...

// Later: verify no unexpected changes
terse_test_clear_calls(handle);
terse_test_start_recording(handle);
render_ui(handle);
terse_test_stop_recording(handle);
int current_count;
const terse_call_record_t *current = terse_test_get_calls(handle, &current_count);
assert(current_count == baseline_count);
// Compare call sequences...
```

**2. 能力フォールバックのテスト:**
```c
// Test with TrueColor
terse_capabilities_t truecolor_caps = {...};
truecolor_caps.colors = TERSE_COLOR_TRUECOLOR;
terse_test_mock_capabilities(handle, &truecolor_caps);
test_color_rendering(handle);

// Test with 16 colors
terse_capabilities_t basic_caps = {...};
basic_caps.colors = TERSE_COLOR_BASIC16;
terse_test_mock_capabilities(handle, &basic_caps);
test_color_rendering(handle);

// Test with no color
terse_capabilities_t no_color_caps = {...};
no_color_caps.colors = TERSE_COLOR_NONE;
terse_test_mock_capabilities(handle, &no_color_caps);
test_color_rendering(handle);
```

**3. 自動イベントハンドラのテスト:**
```c
// Inject key sequence: "hello" + Enter
terse_event_t events[6];
events[0] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'h'};
events[1] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'e'};
events[2] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'l'};
events[3] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'l'};
events[4] = (terse_event_t){.type = TERSE_EVENT_CHAR, .data.ch.scalar = 'o'};
events[5] = (terse_event_t){.type = TERSE_EVENT_ENTER};

terse_test_mock_events(handle, events, 6);

// Run event loop and verify input buffer state
run_event_loop(handle);
assert(strcmp(input_buffer, "hello") == 0);
```

**例:** 完全な動作例については、`samples/test_mode_demo.c` を参照してください。

---

## ベストプラクティス

### 1. 常にハンドルを閉じる

```c
terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
if (!handle) {
    return 1;
}

// ... use handle ...

terse_close(handle);  // Always call this
```

### 2. 機能を使用する前に能力を確認する

```c
terse_capabilities_t caps = terse_get_capabilities(handle);

if (caps.colors >= TERSE_COLOR_TRUECOLOR) {
    // Use TrueColor
} else {
    // Fallback to basic colors
}

if (caps.mouse >= TERSE_MOUSE_SGR) {
    terse_enable_mouse(handle, TERSE_MOUSE_SGR);
}
```

### 3. 一時的な変更には状態管理を使用する

```c
// Save state
terse_push_state(handle);

// Temporary changes
terse_show_cursor(handle, 0);
terse_move_to(handle, 1, 1);
// ... draw popup ...

// Restore state
terse_pop_state(handle);
```

### 4. リサイズイベントを処理する

```c
terse_size_t size = terse_get_size(handle);
int current_rows = size.rows;
int current_cols = size.cols;

while (1) {
    terse_event_t event;
    if (terse_read_event(handle, 100, &event) == TERSE_OK) {
        if (event.type == TERSE_EVENT_RESIZE) {
            current_rows = event.data.resize.rows;
            current_cols = event.data.resize.cols;
            // Redraw UI with new dimensions
        }
    }
}
```

### 5. 適切なエラーハンドリング

```c
if (terse_write_text(handle, "Hello") < 0) {
    terse_error_info_t err = terse_get_last_error(handle);
    fprintf(stderr, "Write failed: category=%d, code=%d\n",
            err.category, err.code);
    // Handle error appropriately
}
```

### 6. 移植性のためにTERSE_PROFILE_AUTOを使用する

```c
// Automatically detect best profile for terminal
terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
```

### 7. リソースを正しい順序でクリーンアップする

```c
// Disable features before closing
terse_disable_mouse(handle);
terse_disable_bracketed_paste(handle);
terse_show_cursor(handle, 1);
terse_reset_style(handle, TERSE_RESET_ALL);

// Then close
terse_close(handle);
```

### 8. デフォルトでUTF-8を使用する

```c
terse_options_t options = {
    .input_fd = STDIN_FILENO,
    .output_fd = STDOUT_FILENO,
    .codec_name = "UTF-8",  // Recommended
    .disabled_caps = 0,
    .enabled_caps = 0
};
```

### 9. タイムアウトの考慮事項

```c
// Non-blocking read
terse_read_event(handle, 0, &event);

// Short timeout for responsive UI
terse_read_event(handle, 100, &event);  // 100ms

// Blocking read
terse_read_event(handle, -1, &event);
```

### 10. 適切な機能縮退の例

```c
void draw_colored_text(terse_handle_t handle, const char *text) {
    terse_capabilities_t caps = terse_get_capabilities(handle);

    if (caps.colors >= TERSE_COLOR_BASIC16) {
        terse_style_t style = terse_style_default();
        style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 1);
        terse_set_style(handle, &style);
        terse_write_text(handle, text);
        terse_reset_style(handle, TERSE_RESET_ALL);
    } else {
        // No color support
        terse_write_text(handle, text);
    }
}
```

### 11. 完全なアプリケーションテンプレート

```c
#include "terse.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    terse_options_t options = {
        .input_fd = STDIN_FILENO,
        .output_fd = STDOUT_FILENO,
        .codec_name = "UTF-8",
        .disabled_caps = 0,
        .enabled_caps = 0
    };

    terse_handle_t handle = terse_open(TERSE_PROFILE_AUTO, &options);
    if (!handle) {
        fprintf(stderr, "Failed to initialize terse\n");
        return 1;
    }

    // Setup
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_show_cursor(handle, 0);

    terse_capabilities_t caps = terse_get_capabilities(handle);
    if (caps.mouse >= TERSE_MOUSE_SGR) {
        terse_enable_mouse(handle, TERSE_MOUSE_SGR);
    }

    // Main loop
    int running = 1;
    while (running) {
        terse_event_t event;
        int result = terse_read_event(handle, 100, &event);

        if (result == TERSE_OK) {
            if (event.type == TERSE_EVENT_CHAR &&
                event.data.ch.scalar == 'q') {
                running = 0;
            }
            // Handle other events...
        }

        // Update UI periodically
    }

    // Cleanup
    if (caps.mouse >= TERSE_MOUSE_SGR) {
        terse_disable_mouse(handle);
    }
    terse_show_cursor(handle, 1);
    terse_clear_screen(handle, TERSE_CLEAR_ALL);
    terse_close(handle);

    return 0;
}
```

---

## 高度なトピック

### 文字エンコーディング

Terseは2つの文字エンコーディングをサポートします:

- **UTF-8** (推奨): 普遍的で現代的なエンコーディング
- **Shift_JIS**: 日本語のレガシーエンコーディング

**東アジアの文字幅処理:**

```c
terse_options_t options = {
    // ...
    .east_asian_ambiguous_as_wide = 1  // Treat ambiguous chars as 2 cells
};
```

`±`、`×`、`§` のような文字は、コンテキストに応じて1または2セル幅になる可能性があります。このオプションは動作を制御します。

### システムiconvなしでビルド

```bash
cmake -S . -B build -DTERSE_USE_SYSTEM_ICONV=OFF
```

内蔵のmini iconvを使用します(Shift_JIS ↔ UTF-8のみ)。

### ターミナル検出

Terseは以下のターミナルを自動検出します:

- **Apple Terminal**
- **GNOME Terminal / VTE**
- **iTerm2**
- **WezTerm**
- **kitty**
- **Ghostty**
- **Warp**

検出は環境変数(`TERM_PROGRAM`、`VTE_VERSION`など)とSecondary Device Attributes (DA)を使用します。

### プロファイルのクリッピング

特定のプロファイルを要求すると、能力はそのレベルにクリップされます:

```c
// Request P1, but terminal supports P3
terse_handle_t handle = terse_open(TERSE_P1, &options);

terse_capabilities_t caps = terse_get_capabilities(handle);
// caps.profile will be TERSE_P1
// P2/P3 features will be disabled
```

---

## 一般的なパターン

### ボックスの描画

```c
void draw_box(terse_handle_t handle, int row, int col,
              int width, int height) {
    const char *corner_tl = "┌";
    const char *corner_tr = "┐";
    const char *corner_bl = "└";
    const char *corner_br = "┘";
    const char *horizontal = "─";
    const char *vertical = "│";

    // Top border
    terse_move_to(handle, row, col);
    terse_write_text(handle, corner_tl);
    for (int i = 0; i < width - 2; i++) {
        terse_write_text(handle, horizontal);
    }
    terse_write_text(handle, corner_tr);

    // Sides
    for (int r = 1; r < height - 1; r++) {
        terse_move_to(handle, row + r, col);
        terse_write_text(handle, vertical);
        terse_move_to(handle, row + r, col + width - 1);
        terse_write_text(handle, vertical);
    }

    // Bottom border
    terse_move_to(handle, row + height - 1, col);
    terse_write_text(handle, corner_bl);
    for (int i = 0; i < width - 2; i++) {
        terse_write_text(handle, horizontal);
    }
    terse_write_text(handle, corner_br);
}
```

### プログレスバー

```c
void draw_progress_bar(terse_handle_t handle, int row, int col,
                       int width, float progress) {
    int filled = (int)(progress * width);

    terse_move_to(handle, row, col);
    terse_write_text(handle, "[");

    terse_style_t style = terse_style_default();
    style.foreground = terse_color_basic(TERSE_BASIC_COLOR_GREEN, 1);
    terse_set_style(handle, &style);

    for (int i = 0; i < filled; i++) {
        terse_write_text(handle, "█");
    }

    terse_reset_style(handle, TERSE_RESET_ALL);

    for (int i = filled; i < width; i++) {
        terse_write_text(handle, "░");
    }

    terse_write_text(handle, "]");
}
```

### メニュー選択

```c
void draw_menu(terse_handle_t handle, const char **items, int count,
               int selected) {
    for (int i = 0; i < count; i++) {
        terse_move_to(handle, 5 + i, 10);

        if (i == selected) {
            terse_style_t style = terse_style_default();
            style.effects = TERSE_STYLE_INVERSE;
            terse_set_style(handle, &style);
        }

        terse_write_text(handle, items[i]);

        if (i == selected) {
            terse_reset_style(handle, TERSE_RESET_ALL);
        }
    }
}
```

---

## トラブルシューティング

### ターミナルが正しく検出されない

環境変数を確認:
```bash
echo $TERM
echo $TERM_PROGRAM
echo $VTE_VERSION
```

能力フラグで検出をオーバーライド:
```c
options.enabled_caps = TERSE_CAP_ENABLE_TRUECOLOR;
```

### マウスイベントが機能しない

1. マウスがサポートされているか確認:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.mouse == TERSE_MOUSE_NONE) {
    printf("Mouse not supported\n");
}
```

2. マウスが有効になっているか確認:
```c
terse_enable_mouse(handle, TERSE_MOUSE_SGR);
```

3. ターミナルがrawモードになっているか確認(マウス入力には必須)

### 色が表示されない

色のサポートを確認:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
printf("Color support: %d\n", caps.colors);
// 0=none, 1=16, 2=256, 3=TrueColor
```

### 画像が表示されない

1. 画像サポートを確認:
```c
terse_capabilities_t caps = terse_get_capabilities(handle);
if (caps.images == TERSE_IMAGE_NONE) {
    printf("Images not supported\n");
}
```

2. 画像データが有効なPNG/JPEGであることを確認

3. ターミナルを確認(iTerm2、WezTerm、kittyが画像をサポート)

---

## APIリファレンス概要

### ライフサイクル
- `terse_open()` - セッションを開く
- `terse_close()` - セッションを閉じる
- `terse_validate_options()` - オプションを検証

### 能力
- `terse_get_capabilities()` - 能力を問い合わせる
- `terse_capabilities_enable()` - 機能を有効化
- `terse_capabilities_disable()` - 機能を無効化
- `terse_capabilities_reset_overrides()` - オーバーライドをリセット

### 出力 (P0)
- `terse_clear_screen()` - 画面をクリア
- `terse_clear_line()` - 行をクリア
- `terse_move_to()` - 絶対カーソル移動
- `terse_move_by()` - 相対カーソル移動
- `terse_show_cursor()` - カーソルの表示/非表示
- `terse_write_text()` - テキストを書く
- `terse_flush()` - 出力をフラッシュ(何もしない)

### 入力 (P0)
- `terse_read_event()` - 入力イベントを読む
- `terse_keyboard_enable()` - キーボード機能を有効化
- `terse_keyboard_disable()` - キーボード機能を無効化

### クエリ (P0)
- `terse_get_size()` - ターミナルサイズを取得
- `terse_get_cursor_position()` - カーソル位置を取得
- `terse_get_options()` - 現在のオプションを取得
- `terse_get_last_error()` - 最後のエラーを取得

### 状態 (P0)
- `terse_capture_state()` - 状態をキャプチャ
- `terse_restore_state()` - 状態を復元
- `terse_push_state()` - 状態をスタックにプッシュ
- `terse_pop_state()` - 状態をスタックからポップ
- `terse_state_override()` - 状態をオーバーライド
- `terse_state_clear()` - オーバーライドをクリア

### 色/スタイル (P1)
- `terse_color_default()` - デフォルト色
- `terse_color_basic()` - 基本16色
- `terse_color_palette()` - 256色パレット
- `terse_color_truecolor()` - 24ビットRGB
- `terse_style_default()` - デフォルトスタイル
- `terse_set_style()` - スタイルを適用
- `terse_reset_style()` - スタイルをリセット

### マウス/入力 (P2)
- `terse_enable_mouse()` - マウスを有効化
- `terse_disable_mouse()` - マウスを無効化
- `terse_enable_bracketed_paste()` - ペーストモードを有効化
- `terse_disable_bracketed_paste()` - ペーストモードを無効化

### ウィンドウ (P2)
- `terse_set_title()` - ウィンドウタイトルを設定
- `terse_set_hyperlink()` - ハイパーリンクを作成

### 拡張 (P3)
- `terse_set_clipboard()` - クリップボードに書き込む
- `terse_set_cursor_shape()` - カーソル形状を設定
- `terse_display_image()` - 画像を表示
- `terse_display_image_inline()` - 画像を表示(レガシー)
- `terse_notify()` - 通知を送信

---

## 追加リソース

- 仕様書: `docs/terse-specs.md`
- 進捗概要: `docs/progress-overview.md`
- グラフィックスロードマップ: `docs/graphics-roadmap.md`
- プラットフォーム移植: `docs/terse-platform-porting.md`
- mini iconv実装: `docs/mini-iconv-plan.md`

---

**Terse APIユーザーガイド 終**
