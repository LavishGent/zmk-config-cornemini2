#pragma once

/*
 * Chibi Killua Zoldyck - 24x40 monochrome pixel art
 * Based on kandipad bead pattern reference.
 *
 * Each row is 3 bytes (24 pixels), MSB = leftmost pixel.
 * 1 = black (drawn), 0 = white (background).
 *
 * Layout:
 *   Rows  0-9:  Spiky hair (two upward spikes, outline only)
 *   Rows 10-17: Face visible through hair opening
 *   Rows 18-20: Collar / bow detail
 *   Rows 21-25: Upper body with arms out
 *   Rows 26-29: Torso / waist
 *   Rows 30-34: Legs
 *   Rows 35-39: Shoes (filled)
 */

#define KILLUA_WIDTH  24
#define KILLUA_HEIGHT 40
#define KILLUA_STRIDE 3 /* bytes per row */

static const uint8_t killua_map[] = {
    /* Row  0 */ 0x00, 0x20, 0x40, /* ..........#......#...... */
    /* Row  1 */ 0x00, 0x60, 0x60, /* .........##......##..... */
    /* Row  2 */ 0x00, 0xE6, 0x70, /* ........###..##..###.... */
    /* Row  3 */ 0x01, 0xEF, 0x78, /* .......####.####.####... */
    /* Row  4 */ 0x03, 0xEF, 0x7C, /* ......#####.####.#####.. */
    /* Row  5 */ 0x07, 0xE0, 0x7E, /* .....######......######. */
    /* Row  6 */ 0x0F, 0xF0, 0xFF, /* ....########....######## */
    /* Row  7 */ 0x1F, 0xF9, 0xFF, /* ...##########..######### */
    /* Row  8 */ 0x3F, 0xFF, 0xFF, /* ..###################### */
    /* Row  9 */ 0x3F, 0xFF, 0xFF, /* ..###################### */
    /* Row 10 */ 0x70, 0x00, 0x3F, /* .###..............###### */
    /* Row 11 */ 0x60, 0x00, 0x1F, /* .##................##### */
    /* Row 12 */ 0x60, 0xFC, 0x1E, /* .##.....######.....####. */
    /* Row 13 */ 0x61, 0x86, 0x1E, /* .##....##....##....####. */
    /* Row 14 */ 0x61, 0x66, 0x1E, /* .##....#.##..##....####. */
    /* Row 15 */ 0x61, 0x02, 0x1E, /* .##....#......#....####. */
    /* Row 16 */ 0x61, 0x86, 0x1E, /* .##....##....##....####. */
    /* Row 17 */ 0x60, 0xFC, 0x1E, /* .##.....######.....####. */
    /* Row 18 */ 0x70, 0x78, 0x3C, /* .###.....####.....####.. */
    /* Row 19 */ 0x38, 0x5A, 0x38, /* ..###....#.##.#...###... */
    /* Row 20 */ 0x3C, 0x7E, 0x38, /* ..####...######...###... */
    /* Row 21 */ 0x1E, 0x7E, 0x78, /* ...####..######..####... */
    /* Row 22 */ 0x3E, 0x7E, 0x7C, /* ..#####..######..#####.. */
    /* Row 23 */ 0x76, 0x7E, 0x6E, /* .###.##..######..##.###. */
    /* Row 24 */ 0xC6, 0x7E, 0x63, /* ##...##..######..##...## */
    /* Row 25 */ 0x06, 0x7E, 0x60, /* .....##..######..##..... */
    /* Row 26 */ 0x07, 0x7E, 0xE0, /* .....###.######.###..... */
    /* Row 27 */ 0x03, 0x7E, 0xC0, /* ......##.######.##...... */
    /* Row 28 */ 0x03, 0x7E, 0xC0, /* ......##.######.##...... */
    /* Row 29 */ 0x01, 0x7E, 0x80, /* .......#.######.#....... */
    /* Row 30 */ 0x01, 0xBD, 0x80, /* .......##.####.##....... */
    /* Row 31 */ 0x03, 0x3C, 0xC0, /* ......##..####..##...... */
    /* Row 32 */ 0x03, 0x3C, 0xC0, /* ......##..####..##...... */
    /* Row 33 */ 0x06, 0x18, 0x60, /* .....##....##....##..... */
    /* Row 34 */ 0x06, 0x18, 0x60, /* .....##....##....##..... */
    /* Row 35 */ 0x0E, 0x3C, 0x70, /* ....###...####...###.... */
    /* Row 36 */ 0x1F, 0x3C, 0xF8, /* ...#####..####..#####... */
    /* Row 37 */ 0x1F, 0x3C, 0xF8, /* ...#####..####..#####... */
    /* Row 38 */ 0x1F, 0x3C, 0xF8, /* ...#####..####..#####... */
    /* Row 39 */ 0x0F, 0x3C, 0xF0, /* ....####..####..####.... */
};

/* Check if pixel (x, y) is set in the bitmap */
static inline bool killua_pixel(int x, int y) {
    if (x < 0 || x >= KILLUA_WIDTH || y < 0 || y >= KILLUA_HEIGHT) {
        return false;
    }
    int byte_idx = y * KILLUA_STRIDE + (x / 8);
    int bit_idx = 7 - (x % 8);
    return (killua_map[byte_idx] >> bit_idx) & 1;
}
