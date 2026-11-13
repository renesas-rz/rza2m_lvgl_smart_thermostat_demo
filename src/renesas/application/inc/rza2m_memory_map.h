/*
 * rza2m_memory_map.h
 *
 * Build-time memory map selection. The map is chosen by the CMake cache variable RZA2M_MEMORY_MAP
 * (see CMakeLists.txt / CMakePresets.json) and the linker script template generate/linker_script.ld.in.
 *
 *   Hardware  : frame buffers in internal RAM (Graphics_RAM, uncached mirror). Resources placed with
 *               RZA2M_RESOURCE_SECTION live in HyperRAM (NOLOAD, filled by software at run time).
 *   QSPIFlash : HyperRAM is not used at all. RZA2M_RESOURCE_SECTION data is linked into QSPI flash and
 *               read in place (XIP), so it must be const.
 *   HyperRAM  : the two VDC frame buffers are in HyperRAM (0x40000000, mapped L1 write-back cacheable by the MMU;
 *               rm_lvgl_port.c cleans the frame before it is displayed; -DRZA2M_HYPERRAM_CACHE=0 = non-cacheable).
 *               Resources behave as in the Hardware map.
 */
#ifndef RZA2M_MEMORY_MAP_H
#define RZA2M_MEMORY_MAP_H

#if defined(RZA2M_MEMMAP_HYPERRAM)
    #define RZA2M_FRAMEBUFFER_SECTION   "Hyper_RAM_Framebuffer"
#else
    #define RZA2M_FRAMEBUFFER_SECTION   "Graphics_RAM"
#endif

#if defined(RZA2M_MEMMAP_QSPIFLASH)
    /* Const, read-only assets (e.g. images) are fetched straight from QSPI flash */
    #define RZA2M_RESOURCE_SECTION      "DEMO_IMAGES_ROM"
#else
    #define RZA2M_RESOURCE_SECTION      "uncached_Graphics_RAM"
#endif

/* HyperBus controller initialisation (src/renesas/application/porting/hyperbus_setup.c provides R_HYPERBUS_Setup(),
 * called from R_SC_HardwareSetup()). On by default in the HyperRAM map only; override with
 * -DRZA2M_HYPERBUS_INIT=0/1, e.g. 1 to make HyperRAM usable in the Hardware map. */
#ifndef RZA2M_HYPERBUS_INIT
    #if defined(RZA2M_MEMMAP_HYPERRAM)
        #define RZA2M_HYPERBUS_INIT 1
    #else
        #define RZA2M_HYPERBUS_INIT 0
    #endif
#endif

/* HyperRAM start-up self test (src/user_prog/hyperram_test.c), run from os_main_task_t() before the display is
 * started. On by default only in the HyperRAM map (the frame buffers depend on it); override with
 * -DRZA2M_HYPERRAM_SELFTEST=0/1. In the Hardware map it is off because an uninitialised HyperBus would abort. */
#ifndef RZA2M_HYPERRAM_SELFTEST
    #if defined(RZA2M_MEMMAP_HYPERRAM)
        #define RZA2M_HYPERRAM_SELFTEST 1
    #else
        #define RZA2M_HYPERRAM_SELFTEST 0
    #endif
#endif

/* VDC frame buffer read burst and line stride alignment (CMake option RZA2M_VDC_BURST128, AUTO = on in the HyperRAM
 * map). 128-byte bursts need a 128-byte aligned base address and line stride (800 px * 2 B = 1600 -> 1664 B). */
#ifndef RZA2M_VDC_BURST128
    #define RZA2M_VDC_BURST128 0
#endif
#if RZA2M_VDC_BURST128
    #define RZA2M_FB_ALIGN      (128u)
#else
    #define RZA2M_FB_ALIGN      (32u)
#endif

/* HyperRAM map: LVGL renders into one frame-sized buffer in internal RAM (DIRECT mode, no HyperRAM reads) and the
 * flush callback copies only the changed areas to the two HyperRAM frame buffers (write-only, through the uncached
 * alias 0xB0000000), flipping the displayed buffer and replaying the same areas on the other one. Rendering and
 * blending by the CPU then never touch HyperRAM, so the VDC is not starved. Override: -DRZA2M_FB_COPY_FLUSH=0/1. */
#ifndef RZA2M_FB_COPY_FLUSH
    #if defined(RZA2M_MEMMAP_HYPERRAM)
        #define RZA2M_FB_COPY_FLUSH 1
    #else
        #define RZA2M_FB_COPY_FLUSH 0
    #endif
#endif

/* RZA2M_FB_COPY_FLUSH: render into / copy from the L1-cacheable view (0x80xxxxxx) of the internal-RAM render buffer
 * instead of its uncached mirror (0x82xxxxxx). Faster rendering and faster copy reads; no cache maintenance needed
 * because only the CPU reads that buffer. Override: -DRZA2M_RENDER_CACHED=0. */
#ifndef RZA2M_RENDER_CACHED
    #define RZA2M_RENDER_CACHED 1
#endif

/* HyperRAM frame buffers as 8 bits per pixel (VDC6 CLUT8, 256-colour palette, see clut8.c). LVGL still renders RGB565 into
 * the internal-RAM render buffer; the flush callback converts the changed areas to palette indices while writing them to
 * HyperRAM, so the CPU writes and the VDC reads half the bytes. Needs RZA2M_FB_COPY_FLUSH. CMake option RZA2M_FB_CLUT8. */
#ifndef RZA2M_FB_CLUT8
    #define RZA2M_FB_CLUT8 0
#endif
#if RZA2M_FB_CLUT8 && !RZA2M_FB_COPY_FLUSH
    #error "RZA2M_FB_CLUT8 needs RZA2M_FB_COPY_FLUSH (HyperRAM memory map)"
#endif
#if RZA2M_FB_CLUT8
    #define RZA2M_FB_BPP        (1u)       /* bytes per pixel in the frame buffers the VDC reads */
#else
    #define RZA2M_FB_BPP        (2u)
#endif

/* Measure and print the HyperRAM read bandwidth at start-up (part of hyperram_selftest()). */
#ifndef RZA2M_HYPERRAM_BENCH
    #define RZA2M_HYPERRAM_BENCH 1
#endif

/* Count VDC frame-buffer read underflows (the VDC starved of memory bandwidth: flicker, torn lines) and print
 * "VDC underflow: +n ..." on the console once per second when they occur. Silent when there are none. */
#ifndef RZA2M_VDC_UNDERFLOW_MONITOR
    #define RZA2M_VDC_UNDERFLOW_MONITOR 1
#endif

#ifndef RZA2M_MEMORY_MAP_NAME
    #define RZA2M_MEMORY_MAP_NAME       "Hardware"
#endif

#endif /* RZA2M_MEMORY_MAP_H */
