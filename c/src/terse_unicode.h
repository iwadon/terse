#ifndef TERSE_UNICODE_H_INCLUDED
#define TERSE_UNICODE_H_INCLUDED

#include "terse.h"

/*
 * Unicode character classification and width calculation.
 * This module provides East Asian Width-based cell width estimation
 * for proper terminal layout.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Compute cell width (0=combining, 1=narrow, 2=wide) for a Unicode scalar */
int terse_compute_cell_width(terse_handle_t handle, unsigned int scalar);

/* Helper: check if scalar is in a combining character range (width=0) */
int terse_is_combining(unsigned int scalar);

/* Helper: check if scalar is in a wide character range (width=2) */
int terse_is_wide(unsigned int scalar);

/* Helper: check if scalar is in an ambiguous width range */
int terse_is_ambiguous(unsigned int scalar);

#ifdef __cplusplus
}
#endif

#endif /* TERSE_UNICODE_H_INCLUDED */
