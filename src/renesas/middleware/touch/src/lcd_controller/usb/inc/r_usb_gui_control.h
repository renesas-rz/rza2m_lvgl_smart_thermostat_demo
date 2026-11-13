/******************************************************************************
 * DISCLAIMER
 * This software is supplied by Renesas Electronics Corporation and is only
 * intended for use with Renesas products. No other uses are authorized. This
 * software is owned by Renesas Electronics Corporation and is protected under
 * all applicable laws, including copyright laws.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
 * THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT
 * LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED.
 * TO THE MAXIMUM EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS
 * ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES SHALL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR
 * ANY REASON RELATED TO THIS SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE
 * BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * Renesas reserves the right, without notice, to make changes to this
 * software and to discontinue the availability of this software. By using this
 * software, you agree to the additional terms and conditions found by
 * accessing the following link:
 * http://www.renesas.com/disclaimer
 *
 * Copyright (C) 2018 Renesas Electronics Corporation. All rights reserved.
 *****************************************************************************/
/******************************************************************************
 * File Name :   r_usb_gui_control.h
 * @file         r_usb_gui_control.h
 * @version      0.02
 * @brief        LVGL mouse cursor and "connect USB device" notice for the
 *               USB pointing devices (LCD_CONTROLL_DEV == LCD_CONTROLL_USB).
 *               Replaces the VDC sprite engine (SPEA) windows of the former
 *               non-LVGL HMI. All functions must be called from the LVGL task.
 ******************************************************************************/
#ifndef  R_USB_GUI_CONTROL_H
#define  R_USB_GUI_CONTROL_H

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#include "lcd_controller_if.h"

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)

#include "lvgl.h"
#include "tp_if.h"

/******************************************************************************
 Macro definitions
 ******************************************************************************/
/* The mouse cursor is hidden after this time without a mouse movement [ms], 0: never hide */
#define USB_GUI_CURSOR_HIDE_MS      (3000u)

/******************************************************************************
 Functions Prototypes
 ******************************************************************************/
/******************************************************************************
* Function Name: usb_gui_init
* Description  : Creates the mouse cursor (bound to the pointer indev) and the
*                notice that asks for a USB mouse / touch panel. The notice is
*                shown until a device is attached, the cursor until the first
*                mouse movement.
* Arguments    : p_indev - LVGL pointer input device
* Return Value : none
******************************************************************************/
extern void usb_gui_init( lv_indev_t *p_indev );

/******************************************************************************
* Function Name: usb_gui_update
* Description  : Shows / hides the notice and the cursor according to the
*                state of the USB pointer.
* Arguments    : p_ptr - state of the USB pointer
* Return Value : none
******************************************************************************/
extern void usb_gui_update( const tp_usb_pointer_t *p_ptr );

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */

#endif /* R_USB_GUI_CONTROL_H */
