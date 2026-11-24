/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>


#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#include "lvgl.h"
#include "rm_lvgl_port.h"

/* lvgl */
#include "src/misc/lv_types.h"
#include "src/display/lv_display.h"
#include "lv_version.h"

#include "r_typedefs.h"
#include "r_cache_lld_rza2m.h"
#include "r_intc_lld_rza2m.h"

#include "r_riic_drv_api.h"
#include "r_devlink_wrapper.h"
#include "r_riic_drv_sc_cfg.h"

//#include "vdc_portsetting.h"

#include "r_rvapi_ceu.h"
#include "r_rvapi_vdc.h"

#include "lcd_panel.h"
#include "src/display/lv_display_private.h"

//#include "lcd_ft5216_int.h"
#include "r_os_abstraction_api.h"

#include "thermostat/thermostat_app.h"
#include "rza2m_memory_map.h"
#if RZA2M_FB_CLUT8
#include "clut8.h"
#endif

#include "lvgl_middleware_version.h"

/***********************************************************************************************************************
 *
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define RM_LVGL_PORT_OPEN              (('L' << 24U) | ('V' << 16U) | ('G' << 8U) | ('L' << 0U))
#define RM_LVGL_PORT_PRV_SYSTICK_HZ    (1000U)


/* Converts a time in ticks to a time in milliseconds.  This macro can be
 * overridden by a macro of the same name defined in FreeRTOSConfig.h in case the
 * definition here is not suitable for your application. */
#ifndef pdTICKS_TO_MS
    #define pdTICKS_TO_MS( xTimeInTicks )    ( ( TickType_t ) ( ( ( uint64_t ) ( xTimeInTicks ) * ( uint64_t ) 1000U ) / ( uint64_t ) configTICK_RATE_HZ ) )
#endif

/***********************************************************************************************************************
 * External definitions
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
#define FSP_PARAMETER_NOT_USED(p)                     ((void)(p))

#define GRAPHICS_LAYER_WIDTH    (LCD_CH0_DISP_HW)
#define GRAPHICS_LAYER_HEIGHT   (LCD_CH0_DISP_VW)

#define GRAPHICS_BUFFER_NUM             (2u)
#if RZA2M_FB_32BPP
#define GRAPHICS_BUFFER_BYTE_PER_PIXEL  (4u)
#else
#define GRAPHICS_BUFFER_BYTE_PER_PIXEL  (2u)
#endif
#define GRAPHICS_BUFFER_STRIDE  ((( GRAPHICS_LAYER_WIDTH * GRAPHICS_BUFFER_BYTE_PER_PIXEL ) + (RZA2M_FB_ALIGN - 1u)) & ~(RZA2M_FB_ALIGN - 1u))
#define GRAPHICS_BUFFER_BYTES   (GRAPHICS_BUFFER_STRIDE * GRAPHICS_LAYER_HEIGHT)   /* one RGB565 frame, incl. stride padding */

/* The frame buffers the VDC reads: RGB565 (2 bytes per pixel) or CLUT8 (1 byte per pixel, RZA2M_FB_CLUT8). Identical to the
 * GRAPHICS_BUFFER_* values above unless RZA2M_FB_CLUT8 is set. */
#define FB_STRIDE               ((( GRAPHICS_LAYER_WIDTH * RZA2M_FB_BPP ) + (RZA2M_FB_ALIGN - 1u)) & ~(RZA2M_FB_ALIGN - 1u))
#define FB_BYTES                (FB_STRIDE * GRAPHICS_LAYER_HEIGHT)

#define DISPLAY_BITS_PER_PIXEL_INPUT0 (16)
#define DISPLAY_HSIZE_INPUT0                 (480)
#define DISPLAY_HSIZE_UV_INPUT0              (480 >> 1)
#define DISPLAY_VSIZE_INPUT0                 (800)
#define DISPLAY_VSIZE_UV_INPUT0              (800 >> 1)
#define DISPLAY_BUFFER_STRIDE_BYTES_INPUT0   ((DISPLAY_HSIZE_INPUT0 * DISPLAY_BITS_PER_PIXEL_INPUT0) >> 3)
#define DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0  ((DISPLAY_BUFFER_STRIDE_BYTES_INPUT0 * 8) / DISPLAY_BITS_PER_PIXEL_INPUT0)
#define DISPLAY_BUFFER_STRIDE_UV_BYTES_INPUT0   ((DISPLAY_HSIZE_UV_INPUT0 * DISPLAY_BITS_PER_PIXEL_INPUT0) >> 3)

#define LVGL_DISPLAY_HSIZE_INPUT                (DISPLAY_HSIZE_INPUT0)
#define LVGL_DISPLAY_VSIZE_INPUT                (DISPLAY_VSIZE_INPUT0)
#define LVGL_DISPLAY_BUFFER_STRIDE_BYTES_INPUT  (DISPLAY_BUFFER_STRIDE_BYTES_INPUT0)
#define LVGL_DISPLAY_BUFFER_STRIDE_PIXELS_INPUT (DISPLAY_BUFFER_STRIDE_PIXELS_INPUT0)


#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

extern int_t g_hi2c3;


/** LVGL Port module control block instance */
extern  uint8_t   video_buffer[ GRAPHICS_BUFFER_NUM ][ FB_BYTES ];
#if RZA2M_FB_COPY_FLUSH
extern  uint8_t   render_buffer[ GRAPHICS_BUFFER_STRIDE * GRAPHICS_LAYER_HEIGHT ];

/* render_buffer lives in the uncached mirror 0x82xxxxxx of the internal RAM. The same RAM is mapped L1 write-back
 * cacheable at 0x80xxxxxx (r_mmu_drv_sc_cfg.h). Nothing but the CPU reads the render buffer (the VDC reads the HyperRAM
 * frame buffers), so LVGL renders and the flush reads through the cached view without any cache maintenance. */
#if RZA2M_RENDER_CACHED
#define RENDER_BUF   ((uint8_t *) ((uintptr_t) render_buffer - (0x82000000u - 0x80000000u)))
#else
#define RENDER_BUF   ((uint8_t *) render_buffer)
#endif
#endif
extern  uint8_t fb_partial[2][R_LVGL_USER_PARTIAL_BUFFER_SIZE];

extern vdc_error_t r_display_init(const vdc_channel_t vdc_ch);
extern vdc_error_t r_image_quality_adjustment(vdc_channel_t vdc_ch);

extern void VDC_LcdPortSetting(uint32_t param);




static void rm_lvgl_port_flush_cb(lv_display_t * p_lv_display, const lv_area_t * p_lv_area, uint8_t * p_px_map);
static void rm_lvgl_port_flush_wait_cb(lv_display_t * p_lv_display);

volatile uint16_t* g_memory_address_of_RAW_uint16;


void task_lvgl_main_task (void *parameters);
void board_init (void);

static uint32_t g_vpos_flag = 0;
#if RZA2M_VDC_UNDERFLOW_MONITOR
static volatile uint32_t g_vdc_underflow_count = 0;
static volatile uint32_t g_flush_count  = 0;   /* flush_cb calls (one per dirty area in DIRECT mode)      */
static volatile uint32_t g_flush_pixels = 0;   /* sum of the dirty area sizes in pixels                    */
static volatile uint32_t g_flush_frames = 0;   /* completed frames (last flush of a refresh)               */
#endif

/* Timing of the display path, accumulated between two reports (CPU cycle counter, ICLK = 528 MHz) */
#define PERF_ICLK_MHZ   (528u)
#define PERF_US(cyc)        ((unsigned) ((cyc) / PERF_ICLK_MHZ))                    /* cycles -> microseconds   */
#define PERF_MS(cyc)        (PERF_US(cyc) / 1000u)                                   /* whole milliseconds        */
#define PERF_MS10(cyc)      ((PERF_US(cyc) % 1000u) / 100u)                          /* tenths of a millisecond   */
#define PERF_MBS(bytes, cyc) (((cyc) == 0u) ? 0u : (unsigned) (((uint64_t) (bytes) * PERF_ICLK_MHZ) / (cyc)))
static volatile uint32_t g_perf_handler_cyc   = 0;   /* lv_timer_handler() calls that produced a frame            */
static volatile uint32_t g_perf_handler_max   = 0;   /* longest single call that produced a frame (cycles)        */
static volatile uint32_t g_perf_copy_cyc      = 0;   /* flush: copy of the changed areas to HyperRAM              */
static volatile uint32_t g_perf_copy_bytes    = 0;
static volatile uint32_t g_perf_wait_cyc      = 0;   /* flush wait: waiting for V-sync after the buffer swap      */
static volatile uint32_t g_perf_replay_cyc    = 0;   /* replay of the same areas into the previous frame buffer   */
static volatile uint32_t g_perf_replay_bytes  = 0;
static volatile uint32_t g_perf_copy_bytes_acc = 0;  /* bytes copied by fb_copy_area() since last cleared         */

static inline uint32_t perf_cycles(void)
{
    uint32_t v;
    __asm__ volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (v));
    return v;
}

static void perf_init(void)
{
    uint32_t v;
    __asm__ volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (v));
    v = (v | 0x1u) & ~0x8u;                                                  /* enable counters, count every cycle */
    __asm__ volatile ("mcr p15, 0, %0, c9, c12, 0" :: "r" (v));
    __asm__ volatile ("mcr p15, 0, %0, c9, c12, 1" :: "r" (0x80000000u));    /* enable the cycle counter */
}

static void * rotation_buffer = NULL;

#if RM_LVGL_PORT_CFG_PROVIDE_TICK_CALLBACK

#if (BSP_CFG_RTOS == 0)
void _rm_lvgl_port_setup_lvgl_tick( void );
#endif
static uint32_t rm_lvgl_port_tick_get_cb(void);

#endif // RM_LVGL_PORT_CFG_PROVIDE_TICK_CALLBACK

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/

static uint32_t      g_semaphore_vpos;
static uint32_t      s_semaphore_v_sync;  /* event_t */
static p_semaphore_t s_event_from_ISR;  /* event_t */


/*******************************************************************************************************************//**
 * @} (end addtogroup RM_LVGL_PORT)
 **********************************************************************************************************************/

static __attribute__((unused)) void _rm_lvgl_port_flush_partial_cb (lv_display_t * p_lv_display, const lv_area_t * p_lv_area, uint8_t * p_px_map)
{
    lv_color_format_t cf = lv_display_get_color_format(p_lv_display);
    uint32_t px_size = lv_color_format_get_size(cf);

    /*Calculate the position of the rotated area*/
    lv_area_t rotated_area = *p_lv_area;
    lv_display_rotate_area(p_lv_display, &rotated_area);

    /*Calculate the properties of the source buffer*/
    int32_t src_w = lv_area_get_width(p_lv_area);
    int32_t src_h = lv_area_get_height(p_lv_area);
    uint32_t src_stride = lv_draw_buf_width_to_stride((uint32_t)src_w, cf);

    /*Calculate the properties of the frame buffer*/
    uint32_t fb_stride = lv_draw_buf_width_to_stride((uint32_t)p_lv_display->hor_res, cf);

    static uint8_t * p_active_fb = NULL;
    static uint8_t * p_inactive_fb = NULL;

    static uint8_t initial = 1;

    if (initial) {
    	p_active_fb   = (uint8_t *)&video_buffer[0];
    	p_inactive_fb = (uint8_t *)&video_buffer[1];
        initial = 0;
    }

    uint8_t * fb_start = p_active_fb;
    fb_start += (uint32_t) rotated_area.y1 * fb_stride + (uint32_t) rotated_area.x1 * px_size;

    lv_display_rotation_t rotation = lv_display_get_rotation(p_lv_display);

    if (rotation == LV_DISPLAY_ROTATION_0) {
        for (int32_t y = p_lv_area->y1; y <= p_lv_area->y2; y++) {
            lv_memcpy(fb_start, p_px_map, src_stride);
            p_px_map += src_stride;
            fb_start += fb_stride;
        }
    }
    else {
        lv_draw_sw_rotate(p_px_map, fb_start, src_w, src_h, (int32_t) src_stride, (int32_t) fb_stride, rotation, cf);
    }

    if (lv_display_flush_is_last(p_lv_display)) {
    	R_CACHE_L1DataCleanLine(p_active_fb,  GRAPHICS_BUFFER_BYTES);
        R_RVAPI_GraphChangeSurfaceVDC( VDC_CHANNEL_0,  VDC_LAYER_ID_0_RD, (void*)p_active_fb );

#if (BSP_CFG_RTOS == 2)
		/* Try to take the semaphore once without waiting to clear it if already set, then take it again. */
        R_OS_SemaphoreWait(&s_semaphore_v_sync, 0);
        R_OS_SemaphoreWait(&s_semaphore_v_sync, portMAX_DELAY);

#else
		g_vpos_flag = 0;
		while (0 == g_vpos_flag)
		{
	        R_OS_TaskSleep(1);
		}
#endif
		lv_memcpy(p_inactive_fb, p_active_fb, GRAPHICS_BUFFER_BYTES);

		/* Swap buffers */
		uint8_t * p_temp = p_inactive_fb;
		p_inactive_fb = p_active_fb;
		p_active_fb = p_temp;
    }
}

static __attribute__((unused)) void _rm_lvgl_port_flush_direct_cb (lv_display_t * p_lv_display, const lv_area_t * p_lv_area, uint8_t * p_px_map) {
#if RZA2M_VDC_UNDERFLOW_MONITOR
    g_flush_count++;
    g_flush_pixels += (uint32_t) lv_area_get_width(p_lv_area) * (uint32_t) lv_area_get_height(p_lv_area);
    if (lv_display_flush_is_last(p_lv_display))
    {
        g_flush_frames++;
    }
#else
    FSP_PARAMETER_NOT_USED(p_lv_area);
#endif

    if (lv_display_flush_is_last(p_lv_display))
    {
        lv_display_rotation_t rotation = lv_display_get_rotation(p_lv_display);

        if (rotation != LV_DISPLAY_ROTATION_0) {
            lv_color_format_t cf = lv_display_get_color_format(p_lv_display);

            int32_t w = p_lv_display->ver_res;
            int32_t h = p_lv_display->hor_res;
            uint32_t w_stride = lv_draw_buf_width_to_stride((uint32_t)w, cf);
            uint32_t h_stride = lv_draw_buf_width_to_stride((uint32_t)h, cf);

            /* only allocate if rotation is actually being used */
            if (!rotation_buffer) {
                rotation_buffer = lv_malloc(2 * LVGL_DISPLAY_BUFFER_STRIDE_BYTES_INPUT * LVGL_DISPLAY_VSIZE_INPUT);
                LV_ASSERT_MALLOC(rotation_buffer);
            }

            static uint8_t buffer_id = 0;
            uint8_t * fb = (uint8_t *)(rotation_buffer) + buffer_id * (LVGL_DISPLAY_BUFFER_STRIDE_BYTES_INPUT * LVGL_DISPLAY_VSIZE_INPUT);

            if(rotation == LV_DISPLAY_ROTATION_180) {
                lv_draw_sw_rotate(p_px_map, fb, w, h, (int32_t) w_stride, (int32_t) w_stride, rotation, cf);
            }
            else /* 90 or 270 */
            {
                lv_draw_sw_rotate(p_px_map, fb, w, h, (int32_t) w_stride, (int32_t) h_stride, rotation, cf);
            }

            R_CACHE_L1DataCleanLine((uint32_t *) fb, (LVGL_DISPLAY_BUFFER_STRIDE_BYTES_INPUT * LVGL_DISPLAY_VSIZE_INPUT));

            R_RVAPI_GraphChangeSurfaceVDC( VDC_CHANNEL_0,  VDC_LAYER_ID_0_RD, (void*)fb );
            buffer_id = !buffer_id;
        }
        else
        {
        	R_CACHE_L1DataCleanLine((uint32_t *) p_px_map, GRAPHICS_BUFFER_BYTES);

            R_RVAPI_GraphChangeSurfaceVDC( VDC_CHANNEL_0,  VDC_LAYER_ID_0_RD, (void*)p_px_map );
        }
    }
}



/*******************************************************************************************************************//**
 * @brief Display flush callback required for LVGL.
 * @param[in] p_lv_display Pointer to the lv_display_t which is being flushed.
 * @param[in] p_lv_area Pointer to an lv_area_t which is the destination region in the frame buffer to copy the render
 * buffer data to, given in pixel coordinates.
 * @param[in] p_px_map Pointer to the render buffer from which pixel data should be copied.
 **********************************************************************************************************************/
#if RZA2M_FB_COPY_FLUSH
/* ---------------------------------------------------------------------------------------------------------------------
 * HyperRAM frame buffers fed from an internal-RAM render buffer (see rza2m_memory_map.h, RZA2M_FB_COPY_FLUSH)
 *
 * LVGL renders in DIRECT mode into render_buffer (internal RAM). For every refresh the flush callback receives the
 * changed areas. Each area is copied to the frame buffer that is NOT displayed, writing through the uncached alias so
 * that the stores are merged into bursts by the write buffer and no cache line is ever read from HyperRAM. On the last
 * area the VDC is pointed at that frame buffer; after the next V-sync the same areas are copied to the buffer that was
 * displayed before, so both frame buffers stay identical without reading HyperRAM.
 * ------------------------------------------------------------------------------------------------------------------ */
#define FB_ALIAS_OFFSET      (0xB0000000u - 0x40000000u)   /* non-cacheable alias of the HyperRAM, see r_mmu_drv_sc_cfg.h */
#define FB_DIRTY_MAX         (48u)
#define FB_COPY_ALIGN_PX     (32u / RZA2M_FB_BPP)           /* 32-byte aligned writes to the frame buffer */

typedef struct { int32_t x1; int32_t y1; int32_t x2; int32_t y2; } fb_rect_t;

static uint8_t *    s_fb_displayed = NULL;                  /* frame buffer the VDC is reading               */
static uint8_t *    s_fb_back      = NULL;                  /* frame buffer being updated                    */
static fb_rect_t    s_dirty[FB_DIRTY_MAX];
static uint32_t     s_dirty_count  = 0;
static bool_t       s_dirty_full   = false;                 /* too many areas: copy the whole frame          */
static bool_t       s_replay_pending = false;

static inline void fb_dsb(void)
{
    __asm__ volatile ("dsb sy" ::: "memory");
}

/* Copies the area (widened to 32-byte aligned columns; pixels outside the area are identical in both buffers) from the
 * render buffer to the frame buffer fb through the uncached alias. */
static void fb_copy_area(uint8_t * fb, const fb_rect_t * p_r)
{
    int32_t x1 = p_r->x1 & ~(int32_t) (FB_COPY_ALIGN_PX - 1u);
    int32_t x2 = p_r->x2 | (int32_t) (FB_COPY_ALIGN_PX - 1u);
    int32_t y;

    if (x2 > (int32_t) (GRAPHICS_LAYER_WIDTH - 1u))
    {
        x2 = (int32_t) (GRAPHICS_LAYER_WIDTH - 1u);
    }

    const uint32_t px     = (uint32_t) (x2 - x1 + 1);
    const uint32_t bytes  = px * RZA2M_FB_BPP;                   /* bytes written per row to the frame buffer */
    uint8_t *      p_dst  = fb + FB_ALIAS_OFFSET + ((uint32_t) p_r->y1 * FB_STRIDE) + ((uint32_t) x1 * RZA2M_FB_BPP);
    const uint8_t * p_src = RENDER_BUF + ((uint32_t) p_r->y1 * GRAPHICS_BUFFER_STRIDE) + ((uint32_t) x1 * GRAPHICS_BUFFER_BYTE_PER_PIXEL);

    for (y = p_r->y1; y <= p_r->y2; y++)
    {
#if RZA2M_FB_CLUT8
        clut8_convert_row(p_dst, (const uint16_t *) p_src, px);  /* RGB565 -> palette index */
#else
        memcpy(p_dst, p_src, bytes);
#endif
        p_dst += FB_STRIDE;
        p_src += GRAPHICS_BUFFER_STRIDE;
    }
    g_perf_copy_bytes_acc += bytes * (uint32_t) (p_r->y2 - p_r->y1 + 1);
}

static void fb_copy_all(uint8_t * fb)
{
    const fb_rect_t full = { 0, 0, (int32_t) GRAPHICS_LAYER_WIDTH - 1, (int32_t) GRAPHICS_LAYER_HEIGHT - 1 };

    fb_copy_area(fb, &full);
}

static void _rm_lvgl_port_flush_copy_cb (lv_display_t * p_lv_display, const lv_area_t * p_lv_area, uint8_t * p_px_map)
{
    LV_UNUSED(p_px_map);                                    /* DIRECT mode: always the start of render_buffer */

#if RZA2M_VDC_UNDERFLOW_MONITOR
    g_flush_count++;
    g_flush_pixels += (uint32_t) lv_area_get_width(p_lv_area) * (uint32_t) lv_area_get_height(p_lv_area);
    if (lv_display_flush_is_last(p_lv_display))
    {
        g_flush_frames++;
    }
#endif

    if (NULL == s_fb_displayed)
    {
        s_fb_displayed = (uint8_t *) &video_buffer[1];      /* RM_LVGL_PORT_Open() shows buffer 1 first */
        s_fb_back      = (uint8_t *) &video_buffer[0];
    }

    const fb_rect_t r = { p_lv_area->x1, p_lv_area->y1, p_lv_area->x2, p_lv_area->y2 };

    {
        const uint32_t t0 = perf_cycles();

        g_perf_copy_bytes_acc = 0;
        fb_copy_area(s_fb_back, &r);
        fb_dsb();                                           /* include draining the write buffer in the time */
        g_perf_copy_cyc   += perf_cycles() - t0;
        g_perf_copy_bytes += g_perf_copy_bytes_acc;
    }

    if (s_dirty_count < FB_DIRTY_MAX)
    {
        s_dirty[s_dirty_count++] = r;
    }
    else
    {
        s_dirty_full = true;
    }

    if (lv_display_flush_is_last(p_lv_display))
    {
        uint8_t * p_tmp;

        fb_dsb();                                           /* all stores have left the write buffer */
        R_RVAPI_GraphChangeSurfaceVDC(VDC_CHANNEL_0, VDC_LAYER_ID_0_RD, (void *) s_fb_back);

        p_tmp          = s_fb_displayed;
        s_fb_displayed = s_fb_back;
        s_fb_back      = p_tmp;
        s_replay_pending = true;
    }
}

/* Called after the V-sync that made the new frame buffer visible: update the buffer that is no longer displayed */
static void fb_replay_dirty(void)
{
    if (!s_replay_pending)
    {
        return;
    }

    const uint32_t t0 = perf_cycles();

    g_perf_copy_bytes_acc = 0;
    if (s_dirty_full)
    {
        fb_copy_all(s_fb_back);
    }
    else
    {
        for (uint32_t i = 0; i < s_dirty_count; i++)
        {
            fb_copy_area(s_fb_back, &s_dirty[i]);
        }
    }
    fb_dsb();
    g_perf_replay_cyc   += perf_cycles() - t0;
    g_perf_replay_bytes += g_perf_copy_bytes_acc;

    s_dirty_count    = 0;
    s_dirty_full     = false;
    s_replay_pending = false;
}
#endif /* RZA2M_FB_COPY_FLUSH */

static void rm_lvgl_port_flush_cb (lv_display_t * p_lv_display, const lv_area_t * p_lv_area, uint8_t * p_px_map)
{
#if RZA2M_FB_COPY_FLUSH
    _rm_lvgl_port_flush_copy_cb(p_lv_display, p_lv_area, p_px_map);
#else
    if (LV_DISPLAY_RENDER_MODE_PARTIAL == p_lv_display->render_mode) {
        _rm_lvgl_port_flush_partial_cb(p_lv_display, p_lv_area, p_px_map);
    }
    else {
        _rm_lvgl_port_flush_direct_cb(p_lv_display, p_lv_area, p_px_map);
    }
#endif
}

/*******************************************************************************************************************//**
 * @brief Display flush wait callback required for LVGL.
 * @param[in] p_lv_display Pointer to the lv_display_t which is being waited upon.
 **********************************************************************************************************************/
static void rm_lvgl_port_flush_wait_cb (lv_display_t * p_lv_display)
{
    if (lv_display_flush_is_last(p_lv_display))
    {
        const uint32_t t_wait0 = perf_cycles();
#if (BSP_CFG_RTOS == 2)

        /* Try to take the semaphore once without waiting to clear it if already set, then take it again. */
    	R_OS_SemaphoreWait(&s_semaphore_v_sync, 0);
    	R_OS_SemaphoreWait(&s_semaphore_v_sync, portMAX_DELAY);
#else
        g_vpos_flag = 0;
        while (0 == g_vpos_flag)
        {
            R_OS_TaskSleep(1);
        }
#endif
        g_perf_wait_cyc += perf_cycles() - t_wait0;
#if RZA2M_FB_COPY_FLUSH
        /* the new frame buffer is visible now: bring the previously displayed one up to date */
        fb_replay_dirty();
#endif
    }
    else
    {
    }
}

#define TP_INT_PRI              (30uL)
#define TP_TSK_PRI              (R_OS_TASK_MAIN_TASK_PRI+1)

extern void touchpad_read (lv_indev_t * indev_drv, lv_indev_data_t * data);
extern void tp_set_screen_size( int_t nWidth, int_t nHeight );
extern int_t lcd_ft5216_lvgl_int_open( const uint32_t unIrqLv );
extern int_t lcd_ft5216_int_start( void );

void board_init (void)
{
    int_t nret = 0;
    //    int_t error = -1;

    R_CACHE_L1DataEnable();

    RM_LVGL_PORT_Open();

    /* TODO NEEDs I2C */
    /* open RIIC */
    if( g_hi2c3 < 0 )
    {
        nret = open(DEVICE_INDENTIFIER "riic3", O_RDWR);

        if (nret >= 0)
        {
            g_hi2c3 = nret;
        }
        else
        {
//            R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
//            R_OS_SemaphoreDelete( &g_slcd_sem_idt_sk);
        }
    }


    lcd_ft5216_lvgl_int_open(TP_INT_PRI);

    lcd_ft5216_int_start();


    lv_indev_t * p_indev_touchpad;

    /* Register touch controller */
    p_indev_touchpad = lv_indev_create();
    lv_indev_set_type(p_indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(p_indev_touchpad, touchpad_read);

    tp_set_screen_size(GRAPHICS_LAYER_WIDTH, GRAPHICS_LAYER_HEIGHT);

}

/****************************************************************************/
/* Function Name : R_LVGL_v_sync_fromISR                                */
/* Explanation   : on_v_sync                                                */
/* PARAMETERS    : none                                                     */
/* RETURNS       : none                                                     */
/* NOTES         : Especially, none.                                        */
/****************************************************************************/
static void  R_LVGL_v_sync_fromISR( vdc_int_type_t  in_InterruptType )
{
	switch (in_InterruptType)
	{
        case VDC_INT_TYPE_VLINE:
            R_OS_SemaphoreRelease( &s_semaphore_v_sync );
	        break;
#if RZA2M_VDC_UNDERFLOW_MONITOR
        case VDC_INT_TYPE_IV3_VBUFERR:
            /* the VDC could not read the frame buffer fast enough (graphics layer 0) */
            g_vdc_underflow_count++;
            break;
#endif
        default:

	}
} /* End of function R_LVGL_v_sync_fromISR() */

#if RZA2M_VDC_UNDERFLOW_MONITOR
/* Reports VDC frame-buffer read underflows (visible as flicker / torn lines) once per second, only when they occur */
static void vdc_underflow_report_cb(lv_timer_t * p_timer)
{
    static uint32_t last_count  = 0;
    static uint32_t last_flush  = 0;
    static uint32_t last_px     = 0;
    static uint32_t last_frames = 0;
    uint32_t        count       = g_vdc_underflow_count;
    uint32_t        flushes     = g_flush_count;
    uint32_t        px          = g_flush_pixels;
    uint32_t        frames      = g_flush_frames;

    LV_UNUSED(p_timer);
    if ((count != last_count) || (flushes != last_flush))
    {
        /* underflows next to what LVGL redrew in the same second: shows whether they come from redraws */
        printf("VDC underflow: +%u (total %u) | LVGL redraw: %u frame(s), %u area(s), %u px (%u%% of screen)\r\n",
               (unsigned) (count - last_count), (unsigned) count,
               (unsigned) (frames - last_frames), (unsigned) (flushes - last_flush), (unsigned) (px - last_px),
               (unsigned) (((px - last_px) * 100u) / (GRAPHICS_LAYER_WIDTH * GRAPHICS_LAYER_HEIGHT)));
        {
            /* where the time of the refresh went (ms, one decimal): total = LVGL render + copy + V-sync wait + replay */
            const uint32_t h  = g_perf_handler_cyc,  hm = g_perf_handler_max;
            const uint32_t c  = g_perf_copy_cyc,     cb = g_perf_copy_bytes;
            const uint32_t w  = g_perf_wait_cyc;
            const uint32_t r  = g_perf_replay_cyc,   rb = g_perf_replay_bytes;
            const uint32_t render = (h > (c + w + r)) ? (h - c - w - r) : 0u;

            printf("  time/s: refresh %u.%u ms (max %u.%u) = render %u.%u + copy %u.%u + vsync wait %u.%u + replay %u.%u"
                   " | copy %u KB @ %u MB/s, replay %u KB @ %u MB/s\r\n",
                   PERF_MS(h), PERF_MS10(h), PERF_MS(hm), PERF_MS10(hm), PERF_MS(render), PERF_MS10(render),
                   PERF_MS(c), PERF_MS10(c), PERF_MS(w), PERF_MS10(w), PERF_MS(r), PERF_MS10(r),
                   (unsigned) (cb / 1024u), (unsigned) PERF_MBS(cb, c), (unsigned) (rb / 1024u), (unsigned) PERF_MBS(rb, r));
            g_perf_handler_cyc = 0u; g_perf_handler_max = 0u; g_perf_copy_cyc = 0u; g_perf_copy_bytes = 0u;
            g_perf_wait_cyc = 0u; g_perf_replay_cyc = 0u; g_perf_replay_bytes = 0u;
        }
        fflush(stdout);
        last_count  = count;
        last_flush  = flushes;
        last_px     = px;
        last_frames = frames;
    }
}
#endif /* RZA2M_VDC_UNDERFLOW_MONITOR */


void task_lvgl_main_task (void *parameters)
{
    /* unused argument */
    UNUSED_PARAM(parameters);

    printf( "Light and Versatile Graphics Library Version [%d.%d.%d]\n",
    		LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    fflush(stdout);

    /* Initialize LVGL. */
    lv_init();

    board_init();

/* Thermostat demo: starts the "Tstat Ctrl" FreeRTOS task and builds the thermostat UI */
    printf("LVGL Smart Thermostat demo Ver. %u.%u.%u\r\n", LVGL_MIDDLEWARE_INFO_RELEASE, LVGL_MIDDLEWARE_INFO_VERSION, LVGL_MIDDLEWARE_INFO_BUILD);

    thermostat_app_start();

    lv_sysmon_hide_memory(NULL);
    lv_sysmon_hide_performance(NULL);

    /* LVGL heart beat each loop repesents one execution cycle */
    while (true)
    {
#if RZA2M_VDC_UNDERFLOW_MONITOR
        const uint32_t frames_before = g_flush_frames;
        const uint32_t t_handler0    = perf_cycles();
#endif
        lv_timer_handler();
#if RZA2M_VDC_UNDERFLOW_MONITOR
        if (g_flush_frames != frames_before)                /* this call refreshed the display */
        {
            const uint32_t dt = perf_cycles() - t_handler0;

            g_perf_handler_cyc += dt;
            if (dt > g_perf_handler_max)
            {
                g_perf_handler_max = dt;
            }
        }
#endif
        R_OS_TaskSleep(1);
    }
}

/*******************************************************************************************************************//**
 * Open the LVGL port driver module.
 *
 * Example:
 * @snippet rza_rm_lvgl_port_example.c RM_LVGL_PORT_Open
 *
 * @note This function should be called after the LVGL function lv_init() has been called.
 *
 * @retval FSP_SUCCESS LVGL port successfully opened.
 * @retval FSP_ERR_OUT_OF_MEMORY Internal call to lv_display_create() failed to allocate memory for a display.
 *
 * See @ref Common_Error_Codes for other possible return codes. This function calls R_LCDC_Open(), R_LCDC_Start(), and
 * R_LCDC_BufferChange().
 **********************************************************************************************************************/
bool_t RM_LVGL_PORT_Open (void)
{
    perf_init();
#if RZA2M_FB_CLUT8
    clut8_init();                                           /* palette must exist before the VDC surface is created */
#endif

    uint8_t * p_fb_partial0 = (uint8_t *) &fb_partial[0];
    uint8_t * p_fb_partial1 = (uint8_t *) &fb_partial[1];

    vdc_error_t error;

    R_OS_SemaphoreCreate(&g_semaphore_vpos, 1);
    R_OS_SemaphoreCreate(&s_semaphore_v_sync, 1);  /* R_OS_CreateEvent */
    R_OS_SemaphoreCreate(s_event_from_ISR,  0);  /* R_OS_CreateEvent */


    vdc_channel_t vdc_ch = VDC_CHANNEL_0;

    /* Clear Back Buffers */
    memset(video_buffer, 0, R_LVGL_USER_FULL_BUFFER_SIZE);
	R_CACHE_L1DataCleanLine(video_buffer,  R_LVGL_USER_FULL_BUFFER_SIZE);

    /* Clear Render Buffers */
    memset(p_fb_partial0, 0, R_LVGL_USER_PARTIAL_BUFFER_SIZE);
    memset(p_fb_partial1, 0, R_LVGL_USER_PARTIAL_BUFFER_SIZE);

    error = r_display_init (vdc_ch);

    /***********************************************************************/
    /* Graphic Layer 0 RGB565 */
    /***********************************************************************/
    if (error == VDC_OK)
    {
        gr_surface_disp_config_t gr_disp_cnf;

        gr_disp_cnf.layer_id         = VDC_LAYER_ID_0_RD;
        gr_disp_cnf.disp_area.hs_rel = 0;
        gr_disp_cnf.disp_area.hw_rel = GRAPHICS_LAYER_WIDTH;
        gr_disp_cnf.disp_area.vs_rel = 0;
        gr_disp_cnf.disp_area.vw_rel = GRAPHICS_LAYER_HEIGHT;
        gr_disp_cnf.fb_buff          = &video_buffer[0];
        gr_disp_cnf.fb_stride        = FB_STRIDE;
#if RZA2M_FB_CLUT8
        gr_disp_cnf.read_format      = VDC_GR_FORMAT_CLUT8;
        gr_disp_cnf.clut_table       = g_clut8_palette;
#elif RZA2M_FB_32BPP
        gr_disp_cnf.read_format      = VDC_GR_FORMAT_ARGB8888;
        gr_disp_cnf.clut_table       = NULL;
#else
        gr_disp_cnf.read_format      = VDC_GR_FORMAT_RGB565;
        gr_disp_cnf.clut_table       = NULL;
#endif
        gr_disp_cnf.read_ycc_swap    = VDC_GR_YCCSWAP_Y0CBY1CR;
#if RZA2M_FB_CLUT8
        gr_disp_cnf.read_swap        = VDC_WR_RD_WRSWA_32_16_8BIT;   /* 8-bit pixels: reverse all 8 bytes of a 64-bit word */
#elif RZA2M_FB_32BPP
        gr_disp_cnf.read_swap        = VDC_WR_RD_WRSWA_32_32BIT;   /* 32-bit pixels: reverse all 4 bytes of a 32-bit word */
#else
        gr_disp_cnf.read_swap        = VDC_WR_RD_WRSWA_32_16BIT;
#endif
        gr_disp_cnf.disp_mode        = VDC_DISPSEL_CURRENT;
        error = R_RVAPI_GraphCreateSurfaceVDC(vdc_ch, &gr_disp_cnf);
    }

    /* Image Quality Adjustment */
    if (VDC_OK == error)
    {
        error = r_image_quality_adjustment(vdc_ch);
    }


    /* Enable signal output */
    if (VDC_OK == error)
    {
        /* Wait for register update */
        R_OS_TaskSleep(5);

        R_RVAPI_DispPortSettingVDC(vdc_ch, &VDC_LcdPortSetting);
    }

    /* Display the inactive buffer if double-buffering is used. */
    if (NULL != p_fb_partial1)
    {
        /* Display the inactive buffer if double-buffering is used. */
        error = R_RVAPI_GraphChangeSurfaceVDC( VDC_CHANNEL_0,  VDC_LAYER_ID_0_RD, video_buffer[1]);

        if (VDC_OK != error)
        {
            R_RVAPI_GraphDestroySurfaceVDC( VDC_CHANNEL_0,  VDC_LAYER_ID_2_RD);
            return error;
        }
    }

    /* LVGL initialise */
    lv_display_t        * p_lv_display;

    p_lv_display = lv_display_create(GRAPHICS_LAYER_WIDTH, GRAPHICS_LAYER_HEIGHT);

    if (NULL == p_lv_display)
    {
        return FALSE;
    }
#if RZA2M_FB_32BPP
    lv_display_set_color_format (p_lv_display, LV_COLOR_FORMAT_ARGB8888);
#endif

//    p_lv_display->render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
    p_lv_display->render_mode = LV_DISPLAY_RENDER_MODE_DIRECT;

    lv_display_set_rotation(p_lv_display, LV_DISPLAY_ROTATION_0);
    lv_display_set_flush_cb(p_lv_display, rm_lvgl_port_flush_cb);
    lv_display_set_flush_wait_cb(p_lv_display, rm_lvgl_port_flush_wait_cb);
    lv_display_set_default(p_lv_display);

    if (LV_DISPLAY_RENDER_MODE_PARTIAL == p_lv_display->render_mode) {
        lv_display_set_buffers(p_lv_display,
        					   &fb_partial[0],
							   &fb_partial[1],
                               (R_LVGL_USER_PARTIAL_BUFFER_SIZE),
							   LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
#if RZA2M_FB_COPY_FLUSH
        /* one frame-sized render buffer in internal RAM; the flush callback copies the changed areas to HyperRAM */
        memset(RENDER_BUF, 0, GRAPHICS_BUFFER_BYTES);
        lv_display_set_buffers_with_stride(p_lv_display,
							   RENDER_BUF,
							   NULL,
                               GRAPHICS_BUFFER_BYTES,
                               GRAPHICS_BUFFER_STRIDE,
							   LV_DISPLAY_RENDER_MODE_DIRECT);
#else
        /* explicit stride: the VDC reads the frame buffer with GRAPHICS_BUFFER_STRIDE bytes per line (padded to
         * RZA2M_FB_ALIGN), LVGL must render with the same line stride */
        lv_display_set_buffers_with_stride(p_lv_display,
							   &video_buffer[0],
							   &video_buffer[1],
                               GRAPHICS_BUFFER_BYTES,
                               GRAPHICS_BUFFER_STRIDE,
							   p_lv_display->render_mode);
#endif
    }

#if RM_LVGL_PORT_CFG_PROVIDE_TICK_CALLBACK
    lv_tick_set_cb(rm_lvgl_port_tick_get_cb);
#endif

    /* Enable V-sync interrupt handler */
    if (VDC_OK == error)
    {
       error = R_RVAPI_InterruptEnableVDC(vdc_ch, VDC_INT_TYPE_VLINE, 0, R_LVGL_v_sync_fromISR);
    }

#if RZA2M_VDC_UNDERFLOW_MONITOR
    if (VDC_OK == error)
    {
        error = R_RVAPI_InterruptEnableVDC(vdc_ch, VDC_INT_TYPE_IV3_VBUFERR, 0, R_LVGL_v_sync_fromISR);
        (void) lv_timer_create(vdc_underflow_report_cb, 1000, NULL);
    }
#endif


    return true;

}


#if RM_LVGL_PORT_CFG_PROVIDE_TICK_CALLBACK
/**********************************************************************************************************************
 * Function Name: rm_lvgl_port_tick_get_cb
 * Description  : Millisecond tick get callback required for LVGL.
 * Return Value : None
 *********************************************************************************************************************/
static uint32_t rm_lvgl_port_tick_get_cb (void)
{
   return (uint32_t) (pdTICKS_TO_MS(xTaskGetTickCount()));
}

#endif /* RM_LVGL_PORT_CFG_PROVIDE_TICK_CALLBACK */
