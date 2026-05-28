#ifndef TERSE_DETECTION_H
#define TERSE_DETECTION_H

#include "terse.h"

/**
 * @file terse_detection.h
 * @brief Terminal capability detection module.
 *
 * This module handles automatic detection of terminal capabilities by
 * examining environment variables and probing terminal responses.
 */

/**
 * Detect terminal capabilities based on environment and requested profile.
 *
 * @param requested_profile The profile level requested by the user.
 * @param options The options structure containing file descriptors for probing.
 * @return A capabilities structure with detected features.
 */
terse_capabilities_t detect_environment_capabilities(terse_profile_t requested_profile, const terse_options_t *options);

/**
 * Create a baseline P0 capabilities structure.
 *
 * P0 provides only basic output: cursor movement, screen/line clearing,
 * and text output. No colors, mouse, or advanced features.
 *
 * @return A capabilities structure initialized to P0 level.
 */
terse_capabilities_t terse_make_p0_capabilities(void);

/**
 * Return a numeric rank for a color kind (for comparison purposes).
 *
 * @param kind The color kind to rank.
 * @return An integer rank (0=default, 1=basic16, 2=palette256, 3=truecolor).
 */
int terse_color_kind_rank(terse_color_kind_t kind);

#endif /* TERSE_DETECTION_H */
