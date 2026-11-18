#ifndef TINYOS_FB_H
#define TINYOS_FB_H

#include <stdint.h>

typedef enum {
    FB_FMT_BGRX = 0, /* UEFI PixelBlueGreenRedReserved8BitPerColor */
    FB_FMT_RGBX = 1, /* UEFI PixelRedGreenBlueReserved8BitPerColor */
} fb_format_t;

typedef struct {
    uint32_t   *base;
    uint32_t    width;
    uint32_t    height;
    uint32_t    pitch_pixels; /* PixelsPerScanLine */
    fb_format_t format;
    uint32_t    cursor_col;
    uint32_t    cursor_row;
    uint32_t    fg_rgb;
    uint32_t    bg_rgb;
} fb_console_t;

int  fb_init(fb_console_t *fb, uint64_t base, uint32_t width, uint32_t height,
             uint32_t pitch_pixels, fb_format_t format);
void fb_clear(fb_console_t *fb);
void fb_putc(fb_console_t *fb, char c);
void fb_puts(fb_console_t *fb, const char *s);

#endif /* TINYOS_FB_H */
