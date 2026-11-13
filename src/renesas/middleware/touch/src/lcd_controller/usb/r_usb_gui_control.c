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
 * File Name :    r_usb_gui_control.c
 * @file          r_usb_gui_control.c
 * @brief         LVGL mouse cursor and "connect USB device" notice
 ******************************************************************************/
#include "lcd_controller_if.h"

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#include "inc/r_usb_gui_control.h"
#include "usb_gui_data/arrow_data.h"
#include "usb_gui_data/caution_data.h"

/******************************************************************************
 Macro definitions
 ******************************************************************************/
#define    ARROW_WIDTH           (32u)
#define    ARROW_HEIGHT          (32u)
#define    CAUTION_WINDOW_WIDTH  (544u)
#define    CAUTION_WINDOW_HEIGHT (128u)
/* vertical offset of the notice from the screen centre, as on the former HMI */
#define    CAUTION_Y_OFFSET      (12)

/******************************************************************************
 Private global variables and functions
 ******************************************************************************/
/* The bitmaps are 32 bit pixel data in B, G, R, A byte order, i.e. LVGL ARGB8888 */
static const lv_image_dsc_t s_arrow_dsc =
{
    .header.magic  = LV_IMAGE_HEADER_MAGIC,
    .header.cf     = LV_COLOR_FORMAT_ARGB8888,
    .header.flags  = 0,
    .header.w      = ARROW_WIDTH,
    .header.h      = ARROW_HEIGHT,
    .header.stride = ARROW_WIDTH * 4u,
    .data_size     = ARROW_SIZE,
    .data          = g_arrow_data,
};

static const lv_image_dsc_t s_caution_dsc =
{
    .header.magic  = LV_IMAGE_HEADER_MAGIC,
    .header.cf     = LV_COLOR_FORMAT_ARGB8888,
    .header.flags  = 0,
    .header.w      = CAUTION_WINDOW_WIDTH,
    .header.h      = CAUTION_WINDOW_HEIGHT,
    .header.stride = CAUTION_WINDOW_WIDTH * 4u,
    .data_size     = CAUTION_SIZE,
    .data          = g_caution_data,
};

static lv_obj_t    *sp_cursor;
static lv_obj_t    *sp_caution;
static uint32_t     s_last_move_tick;

/******************************************************************************
 * Function Name: usb_gui_create_image
 * Description  : Creates an image on the system layer that does not take part
 *                in the input handling.
 *****************************************************************************/
static lv_obj_t *usb_gui_create_image( const lv_image_dsc_t *p_dsc )
{
    lv_obj_t *p_img;

    p_img = lv_image_create( lv_layer_sys() );
    lv_image_set_src( p_img, p_dsc );
    lv_obj_remove_flag( p_img, (lv_obj_flag_t)( LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE ) );
    lv_obj_add_flag( p_img, (lv_obj_flag_t)( LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN ) );

    return p_img;
}

/******************************************************************************
 * Function Name: usb_gui_init
 *****************************************************************************/
void usb_gui_init( lv_indev_t *p_indev )
{
    /* notice: asks for a USB mouse / touch panel, shown until a device is attached */
    sp_caution = usb_gui_create_image( &s_caution_dsc );
    lv_obj_align( sp_caution, LV_ALIGN_CENTER, 0, CAUTION_Y_OFFSET );
    lv_obj_remove_flag( sp_caution, LV_OBJ_FLAG_HIDDEN );

    /* mouse cursor: created after the notice so that it is drawn above it */
    sp_cursor = usb_gui_create_image( &s_arrow_dsc );
    lv_indev_set_cursor( p_indev, sp_cursor );

    s_last_move_tick = lv_tick_get();
}

/******************************************************************************
 * Function Name: usb_gui_update
 *****************************************************************************/
void usb_gui_update( const tp_usb_pointer_t *p_ptr )
{
    if( ( NULL == sp_cursor ) || ( NULL == sp_caution ) )
    {
        return;
    }

    /* notice */
    if( 0u != p_ptr->attached )
    {
        lv_obj_add_flag( sp_caution, LV_OBJ_FLAG_HIDDEN );
    }
    else
    {
        lv_obj_remove_flag( sp_caution, LV_OBJ_FLAG_HIDDEN );
    }

    /* cursor: only for a mouse, shown on mouse activity */
    if( ( 0u == p_ptr->attached ) || ( 0u == p_ptr->is_mouse ) )
    {
        lv_obj_add_flag( sp_cursor, LV_OBJ_FLAG_HIDDEN );
    }
    else
    {
        if( 0u != p_ptr->activity )
        {
            s_last_move_tick = lv_tick_get();
            lv_obj_remove_flag( sp_cursor, LV_OBJ_FLAG_HIDDEN );
        }
        else if( ( 0u != USB_GUI_CURSOR_HIDE_MS ) && ( lv_tick_elaps( s_last_move_tick ) >= USB_GUI_CURSOR_HIDE_MS ) )
        {
            lv_obj_add_flag( sp_cursor, LV_OBJ_FLAG_HIDDEN );
        }
        else
        {
            /* no change */
        }
    }
}

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */
