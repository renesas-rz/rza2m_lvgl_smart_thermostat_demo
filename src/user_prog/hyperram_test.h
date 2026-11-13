/*
 * hyperram_test.h
 *
 * Start-up self test for the external HyperRAM (0x40000000, uncached alias 0xB0000000).
 */
#ifndef HYPERRAM_TEST_H
#define HYPERRAM_TEST_H

#include <stdint.h>

#define HYPERRAM_TEST_OK            (0)
#define HYPERRAM_TEST_ERR_CLOCK     (1)   /* HyperBus module clock is stopped (CPG.STBCR9.MSTP93 = 1)          */
#define HYPERRAM_TEST_ERR_DEVICE    (2)   /* read-back mismatch through the uncached alias (device not working) */
#define HYPERRAM_TEST_ERR_CLEAN     (3)   /* cached write did not reach the RAM after a cache clean            */
#define HYPERRAM_TEST_ERR_INVALID   (4)   /* cached read returned stale data after a cache invalidate          */

/**
 * Checks that the HyperRAM can be written and read back, including through the data caches.
 *
 * The test is non-destructive (the 1 KB test window is saved and restored) and prints its progress with printf()
 * *before* each access, so that a data abort (HyperBus controller not initialised) can be attributed from the
 * terminal output.
 *
 * @return HYPERRAM_TEST_OK or one of the HYPERRAM_TEST_ERR_* codes
 */
int hyperram_selftest(void);

#endif /* HYPERRAM_TEST_H */
