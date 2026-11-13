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
 * File Name :   r_usb_hhid_dev.c
 * @file         r_usb_hhid_dev.c
 * @brief        Parameters of the supported USB HID pointing devices
 ******************************************************************************/
#include "lcd_controller_if.h"

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#include "lcd_panel.h"
#include "inc/r_usb_hhid_dev.h"

/******************************************************************************
 Exported global variables
 ******************************************************************************/
/* no support, dummy define */
const r_usb_keyboard_info_t g_usb_keyboard_info =
{
    0,
};

/* USB mouse 853-410151-201-A (HID boot protocol report: buttons, X, Y, wheel) */
const r_usb_mouse_info_t g_usb_mouse_info =
{
    4,                       /* data buffer length */
    USB_POS_TYPE_RELATIVE,   /* position data type */
    1,                       /* codinate data byte length */
    1,                       /* x coordinate data offset */
    2,                       /* y coordinate data offset */
    0,                       /* x resolution is none */
    800,                     /* max x coordnate value */
    0,                       /* y resolution is none */
    480,                     /* max y coordnate value */
    0,                       /* click data offset */
    0x01,                    /* left click data bit */
    0x02,                    /* right click data bit */
    0x04,                    /* center click data bit */
    3,                       /* wheel data offset */
    USB_MOUSE_CURSOR_SPEED   /* mouse cursor speed */
};

#if (USB_TOUCH_DEVICE == USB_TOUCH_ONLAP_1102I)
/* USB touch On-Lap 1102I */
const r_usb_touch_info_t g_usb_touch_info =
{
    64,                      /* data buffer length */
    USB_POS_TYPE_ABSOLUTE,   /* position data type */
    2,                       /* codinate data byte length */
    2,                       /* x coordinate data offset */
    4,                       /* y coordinate data offset */
    LCD_CH0_DISP_HW,         /* x resolution = width of the display */
    16383,                   /* max x coordnate value */
    LCD_CH0_DISP_VW,         /* y resolution = height of the display */
    9599,                    /* max y coordnate value */
    56                       /* position finger information */
};
#endif /* (USB_TOUCH_DEVICE == USB_TOUCH_ONLAP_1102I) */

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */
