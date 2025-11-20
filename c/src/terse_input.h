#ifndef TERSE_INPUT_H_INCLUDED
#define TERSE_INPUT_H_INCLUDED

#include "terse.h"

/*
 * Internal input processing functions for POSIX event handling.
 * These functions parse terminal escape sequences into terse_event_t.
 */

/* Parse a CSI (Control Sequence Introducer) escape sequence */
int terse_parse_csi_sequence(const unsigned char *seq, size_t len, int *values, size_t max_values, size_t *value_count, char *final);

/* Convert modifier parameter to TERSE_MOD_* bit flags */
int terse_modifier_bits_from_param(int param);

/* Convert kitty keyboard protocol modifier parameter to TERSE_MOD_* flags */
int terse_mods_from_kitty_param(int param);

/* Convert mouse event parameter to TERSE_MOD_* flags */
int terse_mouse_modifiers_from_param(int param);

/* Handle SGR (1006) mouse tracking sequence */
int terse_handle_sgr_mouse_sequence(terse_handle_t handle, terse_event_t *out_event, const int *values, size_t value_count, char final);

/* Decode a multi-byte character from input stream */
int terse_decode_stream_char(terse_handle_t handle, int fd, unsigned char first, terse_event_t *event);

/* Read a single input byte with timeout, handling pending byte buffer */
int terse_read_input_byte(terse_handle_t handle, int timeout_ms, unsigned char *out);

/* Handle escape-prefixed character (Alt+key combinations) */
int terse_handle_escape_prefixed_char(terse_handle_t handle, terse_event_t *event, const unsigned char *seq, size_t len);

/* Map function key code to function key number (F1-F24) */
int terse_function_number_from_code(int code);

#endif /* TERSE_INPUT_H_INCLUDED */
