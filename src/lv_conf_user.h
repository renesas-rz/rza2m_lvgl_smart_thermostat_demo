#ifndef LV_CONF_USER_H_
#define LV_CONF_USER_H_

#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE (0x60000)
#define LV_DRAW_LAYER_MAX_MEMORY (LV_DRAW_LAYER_SIMPLE_BUF_SIZE)

#define LV_USE_DRAW_SW_ASM        (LV_DRAW_SW_ASM_NONE)


/** Build the demos */
#define LV_BUILD_DEMOS 1

#if LV_BUILD_DEMOS
    /** Show some widgets. This might be required to increase `LV_MEM_SIZE`. */
    #define LV_USE_DEMO_WIDGETS 1
    #define LV_USE_DEMO_BENCHMARK 1
#endif

#define LV_USE_SYSMON 1
#if LV_USE_SYSMON
    #define LV_USE_PERF_MONITOR 1
    #define LV_USE_MEM_MONITOR 1
#endif

#define LV_USE_LOG 1
#if LV_USE_LOG
    #define LV_LOG_PRINTF 1
#endif

/* Aiming for 60 fps  */
#define LV_DEF_REFR_PERIOD          16

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

#define LV_ASSERT_HANDLER while(1);

#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_26 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_12 1

#define LV_USE_OBJ_NAME 1

//#define OSPI_SECTION __attribute__((section(".ospi0_cs1"), aligned(32)))
#define GRAPHICS_ROM_SECTION __attribute__ ((section("DEMO_IMAGES_ROM"), aligned(32)))

#define LV_ATTRIBUTE_CHICKEN_DATA           GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_FIRE_SPARKS_DATA       GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_GANOUSH_BG_DATA        GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_LAVA_CAKE_DATA         GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_PIZZA_DATA             GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_SALMON_DATA            GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_VEGETABLE_DATA         GRAPHICS_ROM_SECTION
#define LV_ATTRIBUTE_WHIRLPOOL_LOGO_DATA    GRAPHICS_ROM_SECTION


#endif /* LV_CONF_USER_H_ */
