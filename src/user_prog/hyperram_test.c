/*
 * hyperram_test.c
 *
 * HyperRAM start-up self test.
 *
 * Two views of the same 1 KB window are used (see generate/drivers/r_mmu/inc/r_mmu_drv_sc_cfg.h):
 *   - UNCACHED alias 0xB0000000 : always MMU_ATTR_NORMAL (non-cacheable). Every access reaches the device,
 *                                 so it tells whether the RAM itself works.
 *   - NORMAL address 0x40000000 : cacheable (L1+L2) in the Hardware map, L1-only cacheable in the HyperRAM map
 *                                 (non-cacheable with RZA2M_HYPERRAM_CACHE=0).
 *                                 Used the way the application uses the memory, together with explicit cache
 *                                 maintenance, to prove the write-back and the re-fetch paths.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "r_typedefs.h"
#include "iodefine.h"
#include "iobitmask.h"
#include "r_cache_lld_rza2m.h"
#include "hyperram_test.h"
#include "rza2m_memory_map.h"

#define HYPERRAM_CACHED_BASE        (0x40000000u)
#define HYPERRAM_UNCACHED_BASE      (0xB0000000u)
#define TEST_WORDS                  (256u)                    /* 1 KB = 32 cache lines */
#define TEST_BYTES                  (TEST_WORDS * 4u)

#define DSB()   __asm__ volatile ("dsb sy" ::: "memory")

static volatile uint32_t * const sp_cached   = (volatile uint32_t *) HYPERRAM_CACHED_BASE;
static volatile uint32_t * const sp_uncached = (volatile uint32_t *) HYPERRAM_UNCACHED_BASE;

static uint32_t s_saved[TEST_WORDS];

/* Writes every word of the window through p_wr */
static void fill(volatile uint32_t * p_wr, uint32_t seed, uint32_t step)
{
    for (uint32_t i = 0; i < TEST_WORDS; i++)
    {
        p_wr[i] = seed + (i * step);
    }
    DSB();
}

/* Reads the window through p_rd and compares with the sequence written by fill(); returns the number of errors */
static uint32_t check(const volatile uint32_t * p_rd, uint32_t seed, uint32_t step, uint32_t * p_first_bad)
{
    uint32_t errors = 0;

    for (uint32_t i = 0; i < TEST_WORDS; i++)
    {
        if (p_rd[i] != (seed + (i * step)))
        {
            if (0 == errors)
            {
                *p_first_bad = i;
            }
            errors++;
        }
    }
    return errors;
}

/* Clean+invalidate L1 (by address) and L2 (whole cache) so that RAM and cache agree and the next cached read
 * has to fetch from the device. Clean-and-invalidate is used in both directions so that no dirty data of other
 * users of the caches can be lost. */
static void flush_caches(void)
{
    (void) R_CACHE_L1DataCleanInvalidLine((void *) HYPERRAM_CACHED_BASE, TEST_BYTES);
    R_CACHE_L2CleanInvalidAll();
    DSB();
}

/* ---- read bandwidth measurement (CPU cycle counter, ICLK = 528 MHz) -------------------------------------------- */
#define ICLK_MHZ             (528u)
#define BENCH_CACHED_BYTES   (256u * 1024u)   /* line-fill (32-byte burst) reads                      */
#define BENCH_UNCACHED_BYTES (64u * 1024u)    /* uncached 32-bit reads: one HyperBus transaction each */

static inline __attribute__((unused)) void pmu_start(void)
{
    uint32_t v;

    __asm__ volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (v));
    v = (v | 0x5u) & ~0x8u;                                                 /* E = 1, reset counter, every cycle */
    __asm__ volatile ("mcr p15, 0, %0, c9, c12, 0" :: "r" (v));
    __asm__ volatile ("mcr p15, 0, %0, c9, c12, 1" :: "r" (0x80000000u));   /* enable the cycle counter */
}

static inline __attribute__((unused)) uint32_t pmu_cycles(void)
{
    uint32_t v;

    __asm__ volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (v));
    return v;
}

static __attribute__((unused)) uint32_t mb_per_s(uint32_t bytes, uint32_t cycles)
{
    return (cycles == 0u) ? 0u : (uint32_t) (((uint64_t) bytes * ICLK_MHZ) / cycles);
}

/* Read-only, does not change the HyperRAM contents. The VDC needs about 46 MB/s. */
#if RZA2M_HYPERRAM_BENCH
static void bench_read(void)
{
    const volatile uint32_t * p_c = (const volatile uint32_t *) HYPERRAM_CACHED_BASE;
    const volatile uint32_t * p_u = (const volatile uint32_t *) HYPERRAM_UNCACHED_BASE;
    uint32_t sum = 0;
    uint32_t t0;
    uint32_t cyc_cached;
    uint32_t cyc_uncached;

    pmu_start();

    /* cached: lines must not be in the caches, then read every word (one 32-byte line fill per 8 words) */
    (void) R_CACHE_L1DataCleanInvalidLine((void *) HYPERRAM_CACHED_BASE, BENCH_CACHED_BYTES);
    R_CACHE_L2CleanInvalidAll();
    DSB();
    t0 = pmu_cycles();
    for (uint32_t i = 0; i < (BENCH_CACHED_BYTES / 4u); i++)
    {
        sum += p_c[i];
    }
    cyc_cached = pmu_cycles() - t0;

    t0 = pmu_cycles();
    for (uint32_t i = 0; i < (BENCH_UNCACHED_BYTES / 4u); i++)
    {
        sum += p_u[i];
    }
    cyc_uncached = pmu_cycles() - t0;

    /* do not leave bench lines in the caches */
    (void) R_CACHE_L1DataCleanInvalidLine((void *) HYPERRAM_CACHED_BASE, BENCH_CACHED_BYTES);
    R_CACHE_L2CleanInvalidAll();
    DSB();

    printf("HyperRAM bench: line-fill read %u MB/s (%u KB), uncached 32-bit read %u MB/s (%u KB), VDC needs ~46 MB/s [%08X]\r\n",
           (unsigned) mb_per_s(BENCH_CACHED_BYTES, cyc_cached), (unsigned) (BENCH_CACHED_BYTES / 1024u),
           (unsigned) mb_per_s(BENCH_UNCACHED_BYTES, cyc_uncached), (unsigned) (BENCH_UNCACHED_BYTES / 1024u),
           (unsigned) sum);
    fflush(stdout);
}
#endif /* RZA2M_HYPERRAM_BENCH */

static int fail(int code, const char * p_msg)
{
    printf("HyperRAM test: FAIL - %s\r\n", p_msg);
    fflush(stdout);
    return code;
}

int hyperram_selftest(void)
{
    uint32_t bad_idx = 0;
    uint32_t errors;
    int      result = HYPERRAM_TEST_OK;

    printf("HyperRAM test: start (cached 0x%08X, uncached alias 0x%08X, %u bytes)\r\n",
           (unsigned) HYPERRAM_CACHED_BASE, (unsigned) HYPERRAM_UNCACHED_BASE, (unsigned) TEST_BYTES);

    /* 0. Module clock. If the HyperBus module is in standby, any access aborts the CPU (it never returns). */
    if (0 != (CPG.STBCR9.BYTE & CPG_STBCR9_MSTP93))
    {
        printf("HyperRAM test: CPG.STBCR9.MSTP93 = 1, the HyperBus module clock is stopped.\r\n"
               "               R_HYPERBUS_Setup() (hwsetup.c) must enable and configure the controller.\r\n");
        return fail(HYPERRAM_TEST_ERR_CLOCK, "HyperBus clock is off, HyperRAM not accessible");
    }

    printf("HyperRAM test: HyperBus clock on, accessing HyperRAM (a hang here = data abort, controller not set up)\r\n");
    fflush(stdout);

    /* Save the window through the uncached view; restored at the end */
    for (uint32_t i = 0; i < TEST_WORDS; i++)
    {
        s_saved[i] = sp_uncached[i];
    }

    /* 1. Device test through the uncached alias: data patterns, address-in-data and a walking 1 */
    static const uint32_t patterns[] = { 0x00000000u, 0xFFFFFFFFu, 0xA5A5A5A5u, 0x5A5A5A5Au };
    for (uint32_t p = 0; p < (sizeof(patterns) / sizeof(patterns[0])); p++)
    {
        fill(sp_uncached, patterns[p], 0u);
        errors = check(sp_uncached, patterns[p], 0u, &bad_idx);
        if (0u != errors)
        {
            printf("HyperRAM test: pattern 0x%08X: %u errors, first at word %u (read 0x%08X)\r\n",
                   (unsigned) patterns[p], (unsigned) errors, (unsigned) bad_idx, (unsigned) sp_uncached[bad_idx]);
            result = fail(HYPERRAM_TEST_ERR_DEVICE, "read-back mismatch through the uncached alias");
            goto restore;
        }
    }

    fill(sp_uncached, 0x12345678u, 0x01010101u);               /* every word differs: catches address faults */
    errors = check(sp_uncached, 0x12345678u, 0x01010101u, &bad_idx);
    if (0u != errors)
    {
        printf("HyperRAM test: address-in-data: %u errors, first at word %u\r\n", (unsigned) errors, (unsigned) bad_idx);
        result = fail(HYPERRAM_TEST_ERR_DEVICE, "address/data line fault through the uncached alias");
        goto restore;
    }

    for (uint32_t bit = 0; bit < 32u; bit++)
    {
        sp_uncached[0] = (1u << bit);
        DSB();
        if (sp_uncached[0] != (1u << bit))
        {
            printf("HyperRAM test: walking 1: bit %u read back 0x%08X\r\n", (unsigned) bit, (unsigned) sp_uncached[0]);
            result = fail(HYPERRAM_TEST_ERR_DEVICE, "data line fault (walking 1)");
            goto restore;
        }
    }
    printf("HyperRAM test: device OK through the uncached alias\r\n");

    /* 2. Cached write -> clean -> must be visible through the uncached alias */
    fill(sp_cached, 0xC0DE0000u, 1u);
    flush_caches();
    errors = check(sp_uncached, 0xC0DE0000u, 1u, &bad_idx);
    if (0u != errors)
    {
        printf("HyperRAM test: after cache clean: %u errors, first at word %u (read 0x%08X)\r\n",
               (unsigned) errors, (unsigned) bad_idx, (unsigned) sp_uncached[bad_idx]);
        result = fail(HYPERRAM_TEST_ERR_CLEAN, "cached write did not reach the RAM after a cache clean");
        goto restore;
    }

    /* 3. Change the RAM behind the cache -> invalidate -> a cached read must return the new data (not stale lines) */
    (void) check(sp_cached, 0xC0DE0000u, 1u, &bad_idx);        /* pull the lines into the caches */
    fill(sp_uncached, 0xBEEF0000u, 3u);
    flush_caches();
    errors = check(sp_cached, 0xBEEF0000u, 3u, &bad_idx);
    if (0u != errors)
    {
        printf("HyperRAM test: after cache invalidate: %u errors, first at word %u (read 0x%08X)\r\n",
               (unsigned) errors, (unsigned) bad_idx, (unsigned) sp_cached[bad_idx]);
        result = fail(HYPERRAM_TEST_ERR_INVALID, "cached read returned stale data after an invalidate");
        goto restore;
    }

    printf("HyperRAM test: PASS (cached and uncached views agree)\r\n");

restore:
    for (uint32_t i = 0; i < TEST_WORDS; i++)
    {
        sp_uncached[i] = s_saved[i];
    }
    DSB();
    flush_caches();

#if RZA2M_HYPERRAM_BENCH
    if (HYPERRAM_TEST_OK == result)
    {
        bench_read();
    }
#endif
    fflush(stdout);
    return result;
}
