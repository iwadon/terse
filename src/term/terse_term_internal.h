#ifndef TERSE_TERM_INTERNAL_H
#define TERSE_TERM_INTERNAL_H

/*
 * terse-term (低レベル層) が中レベル層に公開する内部 API の集約ヘッダ。
 *
 * core 層 (中レベル) はこのヘッダ 1 本を include することで term 層 (低レベル)
 * の機能にアクセスする。これにより「core が term の何を使っているか」が
 * この include リストに一望でき、層間の依存が明示される。
 *
 * 個別ヘッダ (terse_codec.h 等) は term 層内部での利用のために残す。
 * core 層は個別ヘッダを直接 include せず、必ずこの集約ヘッダを経由する。
 *
 * 注: mini_iconv.h は codec 実装の下請けであり core からは呼ばないため
 * 含めない (term 内部の terse_codec.c のみが include する)。
 *
 * 注: terse_handle.h は core 所属だが、terse_capabilities.h /
 * terse_keyboard.h が struct terse_handle に依存しているため、この集約ヘッダを
 * include すると間接的に terse_handle.h も引き込まれる。これは term → core の
 * 逆流依存 (Phase 8 で解消予定) の現れであり、Phase 2 では現状のままとする。
 * 詳細は docs/redesign-phase2-plan.md §4 を参照。
 */

#include "terse_capabilities.h"
#include "terse_codec.h"
#include "terse_cursor.h"
#include "terse_detection.h"
#include "terse_device.h"
#include "terse_event_helpers.h"
#include "terse_graphics.h"
#include "terse_input.h"
#include "terse_keyboard.h"
#include "terse_style.h"
#include "terse_unicode.h"

#endif /* TERSE_TERM_INTERNAL_H */
