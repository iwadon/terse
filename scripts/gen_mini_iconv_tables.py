#!/usr/bin/env python3
"""Generate mini_iconv_tables.h for the bundled Shift_JIS converter."""

import argparse
import textwrap

LEADS = list(range(0x81, 0x9F + 1)) + list(range(0xE0, 0xFC + 1))
TRAIL_PRIMARY = list(range(0x40, 0x7F))
TRAIL_SECONDARY = list(range(0x80, 0xFD))
TRAILS = TRAIL_PRIMARY + TRAIL_SECONDARY
HALFWIDTH_BASE = 0xA1
HALFWIDTH_COUNT = 0xDF - 0xA1 + 1

HEADER_TEMPLATE = """#ifndef MINI_ICONV_TABLES_H
#define MINI_ICONV_TABLES_H

#include <stdint.h>

#define MINI_ICONV_SJIS_LEAD_COUNT {lead_count}
#define MINI_ICONV_SJIS_TRAIL_COUNT {trail_count}
#define MINI_ICONV_HALF_WIDTH_COUNT {halfwidth_count}
#define MINI_ICONV_UNICODE_MAP_COUNT {unicode_count}

static inline int mini_iconv_lead_index(unsigned char lead) {{
	if (lead >= 0x81 && lead <= 0x9f) {{
		return (int)(lead - 0x81);
	}}
	if (lead >= 0xe0 && lead <= 0xfc) {{
		return (int)(lead - 0xe0 + {lead_split});
	}}
	return -1;
}}

static inline int mini_iconv_trail_index(unsigned char trail) {{
	if (trail >= 0x40 && trail <= 0x7e) {{
		return (int)(trail - 0x40);
	}}
	if (trail >= 0x80 && trail <= 0xfc) {{
		return (int)(trail - 0x80 + {primary_count});
	}}
	return -1;
}}

static const uint16_t mini_iconv_sjis_double_map[MINI_ICONV_SJIS_LEAD_COUNT * MINI_ICONV_SJIS_TRAIL_COUNT] = {{
{double_map}
}};

static const uint16_t mini_iconv_halfwidth_table[MINI_ICONV_HALF_WIDTH_COUNT] = {{
{halfwidth}
}};

static const uint32_t mini_iconv_unicode_keys[MINI_ICONV_UNICODE_MAP_COUNT] = {{
{unicode_keys}
}};

static const uint16_t mini_iconv_unicode_values[MINI_ICONV_UNICODE_MAP_COUNT] = {{
{unicode_values}
}};

#endif /* MINI_ICONV_TABLES_H */
"""


def chunk(seq, width):
    for i in range(0, len(seq), width):
        yield seq[i : i + width]


def main(output_path):
    double_map = [[0] * len(TRAILS) for _ in range(len(LEADS))]
    halfwidth = [0] * HALFWIDTH_COUNT
    unicode_to_sjis = {}

    def add_mapping(cp, sjis):
        if cp == 0xFFFD:
            return
        if cp <= 0x7F and sjis == cp:
            return
        current = unicode_to_sjis.get(cp)
        if current is None or sjis < current:
            unicode_to_sjis[cp] = sjis

    for byte in range(HALFWIDTH_BASE, HALFWIDTH_BASE + HALFWIDTH_COUNT):
        try:
            char = bytes([byte]).decode("shift_jis")
        except UnicodeDecodeError:
            continue
        if len(char) != 1:
            continue
        codepoint = ord(char)
        halfwidth[byte - HALFWIDTH_BASE] = codepoint
        add_mapping(codepoint, byte)

    for li, lead in enumerate(LEADS):
        for ti, trail in enumerate(TRAILS):
            if trail == 0x7F:
                continue
            try:
                char = bytes([lead, trail]).decode("shift_jis")
            except UnicodeDecodeError:
                continue
            if len(char) != 1:
                continue
            codepoint = ord(char)
            double_map[li][ti] = codepoint
            add_mapping(codepoint, (lead << 8) | trail)

    items = sorted(unicode_to_sjis.items())
    keys = [f"0x{cp:04X}u" for cp, _ in items]
    values = [f"0x{sjis:04X}u" for _, sjis in items]

    flat_double = [f"0x{value:04X}u" for row in double_map for value in row]
    halfwidth_values = [f"0x{value:04X}u" for value in halfwidth]

    double_lines = ["\t" + ", ".join(group) + ("," if group is not None else "") for group in chunk(flat_double, 8)]
    halfwidth_lines = ["\t" + ", ".join(group) + ("," if group is not None else "") for group in chunk(halfwidth_values, 8)]
    key_lines = ["\t" + ", ".join(group) + ("," if group is not None else "") for group in chunk(keys, 6)]
    value_lines = ["\t" + ", ".join(group) + ("," if group is not None else "") for group in chunk(values, 6)]

    content = HEADER_TEMPLATE.format(
        lead_count=len(LEADS),
        trail_count=len(TRAILS),
        halfwidth_count=HALFWIDTH_COUNT,
        unicode_count=len(items),
        lead_split=(0x9F - 0x81 + 1),
        primary_count=len(TRAIL_PRIMARY),
        double_map="\n".join(double_lines),
        halfwidth="\n".join(halfwidth_lines),
        unicode_keys="\n".join(key_lines),
        unicode_values="\n".join(value_lines),
    )

    if output_path == "-":
        print(content)
    else:
        with open(output_path, "w", encoding="utf-8") as out:
            out.write(content)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate mini_iconv tables")
    parser.add_argument("output", nargs="?", default="c/src/mini_iconv_tables.h")
    args = parser.parse_args()
    main(args.output)
