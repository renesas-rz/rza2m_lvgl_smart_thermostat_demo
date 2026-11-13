/*
 * clut8.c
 *
 * 256-colour palette and RGB565 -> index conversion for the VDC6 CLUT8 frame buffer format. See clut8.h.
 *
 * Palette layout (256 entries, ARGB8888, alpha 0xFF):
 *   - black and white
 *   - the exact flat colours of the thermostat UI (thermostat_ui.c C_* colours; the UI uses no gradients or shadows)
 *   - 3 blend steps (1/4, 1/2, 3/4) between every UI foreground (text, accents) and every UI background (the
 *     anti-aliased edges of text, arcs and rounded shapes, and LV_OPA_40/80 text and arc colours)
 *   - a 4x4x4 colour cube and a grey ramp as a catch-all
 * Entries are only added once (duplicates are skipped) and unused entries are filled by the cube and the grey ramp.
 * If the UI colours change, update the two lists below.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "clut8.h"
#include "rza2m_memory_map.h"

/* ---- UI colours (keep in sync with thermostat_ui.c) ------------------------------------------------------------- */
static const uint32_t s_backgrounds[] =
{
    0x0B0F14u,  /* C_BG        */
    0x0F141Bu,  /* C_BAR       */
    0x161C24u,  /* C_CARD      */
    0x1F2731u,  /* C_CARD_HI   */
    0x252E3Au   /* C_BTN       */
};

static const uint32_t s_foregrounds[] =
{
    0xF2F5F8u,  /* C_TEXT        */
    0x8A94A3u,  /* C_TEXT_DIM    */
    0x5B6573u,  /* C_TEXT_MUTED / C_OFF */
    0xFF6A2Bu,  /* C_HEAT        */
    0x2E9BFFu,  /* C_COOL        */
    0xA77BFFu,  /* C_AUTO        */
    0x37C978u,  /* C_OK / C_HOME */
    0x5B8DB8u,  /* C_AWAY        */
    0x7C6CF0u,  /* C_SLEEP       */
    0x2A54F5u   /* C_RENESAS     */
};

#define N_BG    (sizeof(s_backgrounds) / sizeof(s_backgrounds[0]))
#define N_FG    (sizeof(s_foregrounds) / sizeof(s_foregrounds[0]))

uint32_t g_clut8_palette[CLUT8_COLORS];

/* RGB565 -> index. Lives in the uncached mirror of the internal RAM (the cached CPU RAM is full) and is accessed through
 * the L1-cacheable view of the same memory (0x80xxxxxx), like the render buffer. */
static uint8_t s_lut[65536u] __attribute__ ((section("Graphics_RAM"), aligned(32)));
#define LUT_VIEW   ((uint8_t *) ((uintptr_t) s_lut - (0x82000000u - 0x80000000u)))

static uint32_t s_count = 0;

static void pal_add(uint32_t rgb)
{
    uint32_t i;

    if (s_count >= CLUT8_COLORS)
    {
        return;
    }
    for (i = 0; i < s_count; i++)
    {
        if ((g_clut8_palette[i] & 0x00FFFFFFu) == rgb)
        {
            return;
        }
    }
    g_clut8_palette[s_count++] = 0xFF000000u | rgb;
}

static uint32_t blend(uint32_t fg, uint32_t bg, uint32_t quarters)    /* bg + (fg - bg) * quarters / 4 */
{
    uint32_t out = 0;

    for (uint32_t shift = 0; shift <= 16u; shift += 8u)
    {
        const int32_t f = (int32_t) ((fg >> shift) & 0xFFu);
        const int32_t b = (int32_t) ((bg >> shift) & 0xFFu);

        out |= (uint32_t) (b + (((f - b) * (int32_t) quarters) / 4)) << shift;
    }
    return out;
}

static inline void expand565(uint32_t v, int32_t * p_r, int32_t * p_g, int32_t * p_b)
{
    const uint32_t r5 = (v >> 11) & 0x1Fu;
    const uint32_t g6 = (v >> 5) & 0x3Fu;
    const uint32_t b5 = v & 0x1Fu;

    *p_r = (int32_t) ((r5 << 3) | (r5 >> 2));
    *p_g = (int32_t) ((g6 << 2) | (g6 >> 4));
    *p_b = (int32_t) ((b5 << 3) | (b5 >> 2));
}

void clut8_init(void)
{
    uint32_t i;
    uint32_t j;
    uint8_t * const lut = LUT_VIEW;
    int16_t pr[CLUT8_COLORS];
    int16_t pg[CLUT8_COLORS];
    int16_t pb[CLUT8_COLORS];

    /* 1. palette */
    s_count = 0;
    memset(g_clut8_palette, 0, sizeof(g_clut8_palette));
    pal_add(0x000000u);                                         /* index 0 = black (frame buffers are cleared to 0) */
    pal_add(0xFFFFFFu);
    for (i = 0; i < N_BG; i++) { pal_add(s_backgrounds[i]); }
    for (j = 0; j < N_FG; j++) { pal_add(s_foregrounds[j]); }
    for (i = 0; i < N_BG; i++)
    {
        for (j = 0; j < N_FG; j++)
        {
            pal_add(blend(s_foregrounds[j], s_backgrounds[i], 1u));
            pal_add(blend(s_foregrounds[j], s_backgrounds[i], 2u));
            pal_add(blend(s_foregrounds[j], s_backgrounds[i], 3u));
        }
    }
    for (i = 0; i < 4u; i++)                                    /* 4x4x4 colour cube */
    {
        for (j = 0; j < 16u; j++)
        {
            pal_add((((i * 85u) & 0xFFu) << 16) | ((((j >> 2) * 85u) & 0xFFu) << 8) | (((j & 3u) * 85u) & 0xFFu));
        }
    }
    for (i = 1; (i < 255u) && (s_count < CLUT8_COLORS); i += 5u)    /* grey ramp for what is left */
    {
        pal_add((i << 16) | (i << 8) | i);
    }
    for (i = 0; (i < 255u) && (s_count < CLUT8_COLORS); i += 3u)
    {
        pal_add((i << 16) | (i << 8) | i);
    }
    for (i = s_count; i < CLUT8_COLORS; i++)
    {
        g_clut8_palette[i] = 0xFF000000u;
    }

    /* 2. RGB565 -> nearest palette entry (weighted squared distance in RGB888) */
    for (i = 0; i < CLUT8_COLORS; i++)
    {
        pr[i] = (int16_t) ((g_clut8_palette[i] >> 16) & 0xFFu);
        pg[i] = (int16_t) ((g_clut8_palette[i] >> 8) & 0xFFu);
        pb[i] = (int16_t) (g_clut8_palette[i] & 0xFFu);
    }
    for (uint32_t v = 0; v < 65536u; v++)
    {
        int32_t  r;
        int32_t  g;
        int32_t  b;
        uint32_t best      = 0;
        int32_t  best_dist = 0x7FFFFFFF;

        expand565(v, &r, &g, &b);
        for (i = 0; i < s_count; i++)
        {
            const int32_t dr = r - pr[i];
            const int32_t dg = g - pg[i];
            const int32_t db = b - pb[i];
            const int32_t d  = (2 * dr * dr) + (4 * dg * dg) + (3 * db * db);

            if (d < best_dist)
            {
                best_dist = d;
                best      = i;
                if (0 == d)
                {
                    break;
                }
            }
        }
        lut[v] = (uint8_t) best;
    }

    printf("CLUT8: %u palette colours, RGB565 table built\r\n", (unsigned) s_count);
    fflush(stdout);
}

void clut8_convert_row(uint8_t * p_dst, const uint16_t * p_src, uint32_t n)
{
    const uint8_t * const lut = LUT_VIEW;
    uint32_t *            p_d = (uint32_t *) p_dst;

    for (uint32_t i = 0; i < n; i += 4u)
    {
        *p_d++ = (uint32_t) lut[p_src[i]] |
                 ((uint32_t) lut[p_src[i + 1u]] << 8) |
                 ((uint32_t) lut[p_src[i + 2u]] << 16) |
                 ((uint32_t) lut[p_src[i + 3u]] << 24);
    }
}
