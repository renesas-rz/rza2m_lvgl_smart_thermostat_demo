/*
 * clut8.h
 *
 * 256-colour palette (VDC6 CLUT8) and RGB565 -> palette index conversion.
 *
 * LVGL renders in RGB565 into an internal-RAM buffer. The flush callback converts the changed areas to 8-bit palette
 * indices while writing them to the HyperRAM frame buffers (RZA2M_FB_CLUT8), which halves the HyperRAM traffic of both
 * the CPU and the VDC. The VDC expands the indices with its CLUT.
 */
#ifndef CLUT8_H
#define CLUT8_H

#include <stdint.h>

#define CLUT8_COLORS    (256u)

/** ARGB8888 palette handed to the VDC (R_RVAPI_GraphCreateSurfaceVDC, gr_surface_disp_config_t.clut_table) */
extern uint32_t g_clut8_palette[CLUT8_COLORS];

/**
 * Builds the palette and the 64 K entry RGB565 -> index table. The palette holds the exact colours of the thermostat UI,
 * three blend steps between each text/accent colour and each background (the anti-aliased edges of text and shapes) and
 * a coarse colour cube / grey ramp for everything else. Takes a few hundred milliseconds. Call once, before the display
 * is started.
 */
void clut8_init(void);

/**
 * Converts n RGB565 pixels to palette indices.
 * @param[out] p_dst  destination, 4-byte aligned (may be non-cacheable memory, written with 32-bit stores)
 * @param[in]  p_src  RGB565 pixels
 * @param[in]  n      number of pixels, a multiple of 4
 */
void clut8_convert_row(uint8_t * p_dst, const uint16_t * p_src, uint32_t n);

#endif /* CLUT8_H */
