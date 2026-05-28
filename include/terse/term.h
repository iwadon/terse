#ifndef TERSE_TERM_H_INCLUDED
#define TERSE_TERM_H_INCLUDED

/*
 * terse-term: 低レベル層の公開 API（関数宣言）。
 *
 * このヘッダは端末に近い低レベル機能（カーソル制御、エスケープ生成、
 * デバイス制御、色構築、エンコーディング等）の関数を宣言する。
 * 型定義（terse_handle_t, terse_color_t, terse_capabilities_t 等）は
 * 共有資産として terse.h に集約されており、本ヘッダはそれを include して使う。
 *
 * 既存利用側は <terse.h> を include すれば本ヘッダも自動的に取り込まれる。
 * 低レベル層のみを意識したい利用側は本ヘッダを直接 include してもよい。
 *
 * 詳細な層分類の根拠は docs/redesign-phase3-plan.md を参照。
 */

#include "terse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 色・スタイル構築（ハンドル非依存の低レベルユーティリティ） */
terse_style_t terse_style_default(void);
terse_color_t terse_color_default(void);
terse_color_t terse_color_basic(terse_basic_color_t color, int bright);
terse_color_t terse_color_palette(unsigned char index);
terse_color_t terse_color_truecolor(unsigned char r, unsigned char g, unsigned char b);

/* ケイパビリティのランタイム上書き */
terse_error_t terse_capabilities_enable(terse_handle_t handle, unsigned int enable_mask);
terse_error_t terse_capabilities_disable(terse_handle_t handle, unsigned int disable_mask);
terse_error_t terse_capabilities_reset_overrides(terse_handle_t handle);

/* カーソル制御 */
terse_error_t terse_move_to(terse_handle_t handle, int row, int col);
terse_error_t terse_move_by(terse_handle_t handle, int drow, int dcol);
terse_error_t terse_show_cursor(terse_handle_t handle, int visible);
terse_error_t terse_set_cursor_shape(terse_handle_t handle, terse_cursor_shape_t shape, int blinking);

/* 代替スクリーンバッファ（DEC private mode 1049）。
 * has_alt_screen が偽の端末では no-op（TERSE_OK を返す）。 */
terse_error_t terse_enter_alt_screen(terse_handle_t handle);
terse_error_t terse_leave_alt_screen(terse_handle_t handle);

/* デバイス制御（マウス・ペースト・タイトル・ハイパーリンク） */
terse_error_t terse_enable_mouse(terse_handle_t handle, terse_mouse_mode_t mode);
terse_error_t terse_disable_mouse(terse_handle_t handle);
terse_error_t terse_enable_bracketed_paste(terse_handle_t handle);
terse_error_t terse_disable_bracketed_paste(terse_handle_t handle);
terse_error_t terse_set_title(terse_handle_t handle, const char *title);
terse_error_t terse_set_hyperlink(terse_handle_t handle, const char *url, const char *label);

/* グラフィクス・クリップボード・通知 */
terse_error_t terse_set_clipboard(terse_handle_t handle, const char *data);
terse_error_t terse_display_image(terse_handle_t handle, const terse_image_request_t *request);
terse_error_t terse_display_image_inline(terse_handle_t handle, const unsigned char *data, size_t size, const char *name);
terse_error_t terse_notify(terse_handle_t handle, terse_notification_kind_t kind, const char *payload);

/* キーボードプロトコル */
terse_error_t terse_keyboard_enable(terse_handle_t handle, unsigned int feature_mask);
terse_error_t terse_keyboard_disable(terse_handle_t handle, unsigned int feature_mask);
unsigned int terse_keyboard_get_enabled(terse_handle_t handle);
unsigned int terse_keyboard_get_supported(terse_handle_t handle);

/*
 * UTF-8 encoding utility
 * Encodes a Unicode scalar value to UTF-8 bytes.
 * Returns the number of bytes written (1-4), or 0 on error.
 * The out buffer must have room for at least 4 bytes.
 */
int terse_encode_utf8(unsigned int scalar, unsigned char *out);

/*
 * Character width utility
 * Returns the display width (in terminal cells) of a Unicode scalar value.
 * Returns 0 for control characters and combining marks.
 * Returns 1 for narrow characters.
 * Returns 2 for wide characters (CJK, etc.).
 * Ambiguous width characters respect the east_asian_ambiguous_as_wide option.
 */
int terse_char_width(terse_handle_t handle, unsigned int scalar);

#ifdef __cplusplus
}
#endif

#endif /* TERSE_TERM_H_INCLUDED */
