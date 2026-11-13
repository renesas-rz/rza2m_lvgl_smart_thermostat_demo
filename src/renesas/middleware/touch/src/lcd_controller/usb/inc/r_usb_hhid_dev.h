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
 * File Name :   r_usb_hhid_dev.h
 * @file         r_usb_hhid_dev.h
 * @version      1.1
 * @brief        Parameters of the supported USB HID pointing devices
 ******************************************************************************/
#ifndef  R_USB_HHID_DEV_H
#define  R_USB_HHID_DEV_H

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#include "r_typedefs.h"
#include "r_usbh1_basic_local.h"

/******************************************************************************
 Macro definitions
 ******************************************************************************/
#define USB_IF_MOUSE       0
#define USB_IF_TOUCH       1
#define USB_IF_KEYBOARD    2

#define USB_IF_NODEVICE    0xFF

#define USB_POS_TYPE_ABSOLUTE    0
#define USB_POS_TYPE_RELATIVE    1

/* USB touchpanel define */
#define USB_TOUCH_ONLAP_1102I     0   /* ON-Lap 1102I */

#define USB_TOUCH_DEVICE          USB_TOUCH_ONLAP_1102I

/* Mouse cursor speed magnification (counts -> pixels) */
#define USB_MOUSE_CURSOR_SPEED    (2)

/* The absolute position of the USB touch monitor is mirrored relative to the display
 * (X' = width - X, Y' = height - Y). Set to 0 if the touch monitor is mounted the same
 * way up as the display. */
#define USB_TOUCH_INVERT_XY       (1)

/******************************************************************************
 Structures
 ******************************************************************************/

typedef struct {
    uint32_t     buffer_size;         /* copy data size to TP task from USB */
} r_usb_keyboard_info_t;

typedef struct {
    uint32_t     buffer_size;         /* copy data size to TP task from USB */
    uint32_t     pos_data_type;       /* position data type (absolute / relative) */
    uint32_t     data_len_pos;        /* byte length of X coodinate value */
    uint32_t     data_offset_posx1;   /* position of X coordinate in buffer */
    uint32_t     data_offset_posy1;   /* position of Y coordinate in buffer */
    uint32_t     dev_resolutionx;     /* X resolution value of device */
    uint32_t     dev_max_posx;        /* max X coordinate value of device */
    uint32_t     dev_resolutiony;     /* Y resolution value of device */
    uint32_t     dev_max_posy;        /* max Y coordinate value of device */
    uint32_t     data_offset_click;   /* position of click data, mouse only */
    uint8_t      left_click_bit;      /* position of left click bit, mouse only */
    uint8_t      right_click_bit;     /* position of right click bit, mouse only */
    uint8_t      center_button_bit;   /* position of center button bit, mouse only */
    uint32_t     data_offset_wheel;   /* position of wheel in buffer, mouse only */
    uint32_t     cursor_speed;        /* mouse cursor speed magnification, mouse only */
} r_usb_mouse_info_t;

typedef struct {
    uint32_t     buffer_size;         /* copy data size to TP task from USB */
    uint32_t     pos_data_type;       /* position data type (absolute / relative) */
    uint32_t     data_len_pos;        /* byte length of X coodinate value */
    uint32_t     data_offset_posx1;   /* position of X coordinate in buffer */
    uint32_t     data_offset_posy1;   /* position of Y coordinate in buffer */
    uint32_t     dev_resolutionx;     /* X resolution value of device (= display width) */
    uint32_t     dev_max_posx;        /* max X coordinate value of device */
    uint32_t     dev_resolutiony;     /* Y resolution value of device (= display height) */
    uint32_t     dev_max_posy;        /* max Y coordinate value of device */
    uint32_t     finger_info_pos;     /* position of finger info in buffer, touch only */
} r_usb_touch_info_t;

typedef struct {
    uint32_t                     interface;           /* control interface */
    uint32_t                     copy_data_len;       /* copy data size to TP task from USB */
    const r_usb_keyboard_info_t  *keyboard;           /* parameter of keyboard */
    const r_usb_mouse_info_t     *mouse;              /* parameter of mouse */
    const r_usb_touch_info_t     *touch;              /* parameter of touch panel */
} r_usb_dev_info_t;

/******************************************************************************
 Variable Externs
 ******************************************************************************/
extern r_usb_dev_info_t             gs_usb_dev_info[USBH1_MAXDEVADDR];

/* parameters of the USB devices, see r_usb_hhid_dev.c */
extern const r_usb_keyboard_info_t  g_usb_keyboard_info;
extern const r_usb_mouse_info_t     g_usb_mouse_info;
extern const r_usb_touch_info_t     g_usb_touch_info;

#endif /* R_USB_HHID_DEV_H */
