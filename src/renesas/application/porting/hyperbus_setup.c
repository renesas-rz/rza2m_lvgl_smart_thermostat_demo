/*
 * hyperbus_setup.c
 *
 * HyperBus controller set-up for the RZ/A2M (R7S921053): provides R_HYPERBUS_Setup(), the weak hook that
 * R_SC_HardwareSetup() (generate/system/hwsetup.c) calls and that the BSP leaves empty.
 *
 * Source of the sequence: the Renesas Smart Configurator r_hyperbus driver (R_HYPERBUS_Init() in
 * r_hyperbus_lld_rza2m.c). Its source is not part of the BSP, but it is linked into the precompiled
 * RZ_A2M_BOOTLOADER/Precompiled/rza2m_hyperflash_boot2_loader_gcc.elf of the Guiliani SDK; the register
 * sequence and the default configuration below were recovered from that image (disassembly + DWARF) and
 * produce the same register writes:
 *
 *     1. CPG.SCLKSEL.HYMCR      = 3      HyperBus memory clock source
 *     2. CPG.STBCR9.MSTP93      = 0      HyperBus module clock on (module standby released)
 *     3. GPIO.PHMOM0.HOSEL      = 0      select the HyperBus function (1 = OctaBus) of the shared pins
 *     4. HYPER.MCR0 = 0x00000003         CS0 (HyperFlash, 0x30000000): MAXEN off, MAXLEN 0
 *     5. HYPER.MCR1 = 0x00000013         CS1 (HyperRAM,   0x40000000): DEVTYPE = HyperRAM, MAXEN off
 *     6. HYPER.MTR0 = 0x00000001         CS0 timing (minimum CS setup/hold/inactive)
 *     7. HYPER.MTR1 = 0x00000001         CS1 timing, latency 6 clocks
 *     8. DSB + ISB
 *
 * It only touches registers, uses no writable globals and no stack-hungry code, because it runs from
 * R_SC_HardwareSetup() before the C run-time (.data/.bss) is initialised. It is executed in place from QSPI flash.
 *
 * Enabled by RZA2M_HYPERBUS_INIT (rza2m_memory_map.h): on in the HyperRAM map, off otherwise.
 */
#include <stdint.h>

#include "r_typedefs.h"
#include "iodefine.h"
#include "iobitmask.h"
#include "hwsetup.h"
#include "rza2m_memory_map.h"

#if RZA2M_HYPERBUS_INIT

/* ---- Configuration (Smart Configurator defaults used by the Renesas hyperflash boot loader) ---------------- */
/* Chip select 0: HyperFlash window 0x30000000 */
#define HYPERBUS_CS0_MAXEN      (0u)    /* 1 = limit the CS# low time (burst length) to MAXLEN                   */
#define HYPERBUS_CS0_MAXLEN     (0u)
#define HYPERBUS_CS0_RCSHI      (0u)    /* read  CS# inactive: 0 = 1.5 clocks ... 15 = 16.5 clocks               */
#define HYPERBUS_CS0_WCSHI      (0u)    /* write CS# inactive                                                    */
#define HYPERBUS_CS0_RCSS       (0u)    /* read  CS# setup  : 0 = 1 clock ... 15 = 16 clocks                     */
#define HYPERBUS_CS0_WCSS       (0u)
#define HYPERBUS_CS0_RCSH       (0u)    /* read  CS# hold   : 0 = 1 clock ... 15 = 16 clocks                     */
#define HYPERBUS_CS0_WCSH       (0u)

/* Chip select 1: HyperRAM window 0x40000000. For HyperRAM the CS# low time must not exceed the device's tCSM
 * (4 us / 1 us depending on the part); set MAXEN = 1 and a suitable MAXLEN if long bursts are used. */
#define HYPERBUS_CS1_MAXEN      (0u)
#define HYPERBUS_CS1_MAXLEN     (0u)
#define HYPERBUS_CS1_RCSHI      (0u)
#define HYPERBUS_CS1_WCSHI      (0u)
#define HYPERBUS_CS1_RCSS       (0u)
#define HYPERBUS_CS1_WCSS       (0u)
#define HYPERBUS_CS1_RCSH       (0u)
#define HYPERBUS_CS1_WCSH       (0u)
#define HYPERBUS_CS1_LTCY       (1u)    /* 0 = 5 clocks, 1 = 6 clocks (HyperRAM power-on default latency)        */

/* Register images. The low bits are fixed values written by the Renesas driver (MCR: bits[1:0] = 3, MCR1.DEVTYPE =
 * bit 4 = 1 for HyperRAM; MTR: bit 0 = 1). */
#define HYPERBUS_MCR_MAXEN_SHIFT    (31u)
#define HYPERBUS_MCR_MAXLEN_SHIFT   (18u)
#define HYPERBUS_MCR0_VALUE   (((uint32_t)HYPERBUS_CS0_MAXEN << HYPERBUS_MCR_MAXEN_SHIFT)   | \
                               ((uint32_t)HYPERBUS_CS0_MAXLEN << HYPERBUS_MCR_MAXLEN_SHIFT) | 0x03u)
#define HYPERBUS_MCR1_VALUE   (((uint32_t)HYPERBUS_CS1_MAXEN << HYPERBUS_MCR_MAXEN_SHIFT)   | \
                               ((uint32_t)HYPERBUS_CS1_MAXLEN << HYPERBUS_MCR_MAXLEN_SHIFT) | 0x13u)

#define HYPERBUS_MTR_VALUE(rcshi, wcshi, rcss, wcss, rcsh, wcsh) \
                              (((uint32_t)(rcshi) << 28) | ((uint32_t)(wcshi) << 24) | \
                               ((uint32_t)(rcss)  << 20) | ((uint32_t)(wcss)  << 16) | \
                               ((uint32_t)(rcsh)  << 12) | ((uint32_t)(wcsh)  <<  8))
#define HYPERBUS_MTR0_VALUE   (HYPERBUS_MTR_VALUE(HYPERBUS_CS0_RCSHI, HYPERBUS_CS0_WCSHI, HYPERBUS_CS0_RCSS, \
                                                  HYPERBUS_CS0_WCSS, HYPERBUS_CS0_RCSH, HYPERBUS_CS0_WCSH) | 0x01u)
#define HYPERBUS_MTR1_VALUE   (HYPERBUS_MTR_VALUE(HYPERBUS_CS1_RCSHI, HYPERBUS_CS1_WCSHI, HYPERBUS_CS1_RCSS, \
                                                  HYPERBUS_CS1_WCSS, HYPERBUS_CS1_RCSH, HYPERBUS_CS1_WCSH) | \
                               ((uint32_t)HYPERBUS_CS1_LTCY & 0x0Fu) | 0x01u)

#define HYPERBUS_CPG_HYMCR_SOURCE   (3u)

/*
 * R_HYPERBUS_Setup - overrides the weak hook declared in hwsetup.h
 */
void R_HYPERBUS_Setup(void)
{
    /* 1. clock source of the HyperBus memory clock */
    CPG.SCLKSEL.BIT.HYMCR = HYPERBUS_CPG_HYMCR_SOURCE;

    /* 2. release the HyperBus module from standby; read back so the write has completed before the next access */
    CPG.STBCR9.BIT.MSTP93 = 0u;
    (void) CPG.STBCR9.BYTE;

    /* 3. pin function: HyperBus (0) instead of OctaBus (1) */
    GPIO.PHMOM0.BIT.HOSEL = 0u;

    /* 4.-7. memory control and timing registers for both chip selects */
    HYPER.MCR0.LONG = HYPERBUS_MCR0_VALUE;
    HYPER.MCR1.LONG = HYPERBUS_MCR1_VALUE;
    HYPER.MTR0.LONG = HYPERBUS_MTR0_VALUE;
    HYPER.MTR1.LONG = HYPERBUS_MTR1_VALUE;

    /* 8. make sure the configuration is in effect before any access to 0x30000000 / 0x40000000 */
    __asm__ volatile ("dsb sy" ::: "memory");
    __asm__ volatile ("isb sy" ::: "memory");
}

#endif /* RZA2M_HYPERBUS_INIT */
