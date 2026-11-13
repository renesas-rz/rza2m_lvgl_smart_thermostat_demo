/********************************************************************************************************************** 
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
 * other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
 * applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
 * EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
 * SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO
 * THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
 * this software. By using this software, you agree to the additional terms and conditions found by accessing the
 * following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2019 Renesas Electronics Corporation. All rights reserved.
 *********************************************************************************************************************/
/**************************************************************************//**
 * File Name :   lcd_panel.h
 * @file         lcd_panel.h
 * @version      0.01
 * @brief        Defines for selecting the LCD panel type which is connected.
 ******************************************************************************/
/******************************************************************************
 Macro definitions
 ******************************************************************************/
/* The panel selection does not need any other header, so that it can also be used by
 * r_startup_config.h (define LCD_PANEL_ID_ONLY before including this file). */
#ifndef LCD_PANEL_IDS_H
#define LCD_PANEL_IDS_H

#define     LCD_PANEL_LVDS       (1)
#define     LCD_PANEL_RSK        (2)
#define     LCD_PANEL_DVI        (3)

/* Select the display / pointing-device combination:
 *   LCD_PANEL_RSK : RSK LCD panel, FT5216 capacitive touch over RIIC
 *   LCD_PANEL_DVI : HDMI/DVI monitor (monitor timing is chosen with SELECT_MONITOR in lcd/pc_monitor.h),
 *                   USB HID mouse or USB touch monitor (see lcd_controller_if.h, LCD_CONTROLL_DEV)
 * May be overridden from the compiler command line (-DLCD_PANEL=3). */
#ifndef     LCD_PANEL
#define     LCD_PANEL            (LCD_PANEL_RSK)
#endif

#endif  /*  LCD_PANEL_IDS_H */

#if !defined(LCD_PANEL_ID_ONLY) && !defined(LCD_PANEL_H)
#define LCD_PANEL_H

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#if     (LCD_PANEL==1)
#include "lcd/lcd_lvds_clk.h"
#include "lcd/lcd_lvds_ch0.h"
#elif     (LCD_PANEL==2)
#include "lcd/lcd_rsk_clk.h"
#include "lcd/lcd_rsk_ch0.h"
#elif     (LCD_PANEL==3)
#include "lcd/lcd_dvi_clk.h"
#include "lcd/lcd_dvi_ch0.h"
#else
    #error  Undefined LCD panel!
#endif

#include "rza2m_memory_map.h"

/* Frame buffer size for the selected panel: two RGB565 buffers, line stride aligned to RZA2M_FB_ALIGN (32 or 128 bytes).
 * (RSK 800x480 -> 0x177000, same value as before this was derived from the panel size.) */
/* Two frame buffers as the VDC reads them (RZA2M_FB_BPP bytes per pixel: 2 = RGB565, 1 = CLUT8) */
#define     R_LVGL_USER_FULL_BUFFER_SIZE    (2u * ((((LCD_CH0_DISP_HW) * RZA2M_FB_BPP) + (RZA2M_FB_ALIGN - 1u)) & ~(RZA2M_FB_ALIGN - 1u)) * (LCD_CH0_DISP_VW))
/* One RGB565 frame LVGL renders into (RZA2M_FB_COPY_FLUSH) */
#define     R_LVGL_RENDER_BUFFER_SIZE       (((((LCD_CH0_DISP_HW) * 2u) + (RZA2M_FB_ALIGN - 1u)) & ~(RZA2M_FB_ALIGN - 1u)) * (LCD_CH0_DISP_VW))

#endif  /*  !LCD_PANEL_ID_ONLY && !LCD_PANEL_H */
