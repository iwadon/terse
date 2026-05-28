#ifndef TERSE_STYLE_H
#define TERSE_STYLE_H

#include "terse.h"

/* Internal header for style and color processing.
 * This module handles:
 * - Color space conversions (RGB, palette, basic16)
 * - Style comparison and degradation
 * - SGR sequence generation
 */

/* Color support ranking for degradation decisions. */
int terse_style_color_support_rank(terse_color_support_t support);

/* Compare two colors for equality. */
int terse_style_colors_equal(const terse_color_t *a, const terse_color_t *b);

/* Compare two styles for equality. */
int terse_style_styles_equal(const terse_style_t *a, const terse_style_t *b);

/* Degrade a color to match terminal capabilities. */
terse_color_t terse_style_degrade_color(terse_color_t color, terse_color_support_t support);

/* Sanitize style request by masking unsupported effects. */
terse_style_t terse_style_sanitize_request(const terse_style_t *style);

/* Create effective style based on capabilities. */
terse_style_t terse_style_make_effective(const terse_capabilities_t *caps, const terse_style_t *requested);

/* Emit SGR sequence for given style.
 * Returns 0 on success, negative terse_error_t on failure.
 */
int terse_style_emit_sequence(terse_handle_t handle, const terse_style_t *style);

#endif // TERSE_STYLE_H
