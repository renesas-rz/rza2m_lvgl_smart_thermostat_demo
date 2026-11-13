/*******************************************************************************
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
* Renesas reserves the right, without notice, to make changes to this software
* and to discontinue the availability of this software. By using this software,
* you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer*
* Copyright (C) 2013-2017 Renesas Electronics Corporation. All rights reserved.
*******************************************************************************/

/**************************************************************************//**
* @file         tp_task_usb.c
* @brief        TouchPanel Driver task functions with USB
*
*               The USB mouse / touch panel reports are decoded to screen
*               coordinates and are handed to the LVGL pointer indev through
*               touchpanel_usb_get_pointer(). The former HMI drew the mouse
*               cursor and the "connect USB device" window with the VDC sprite
*               engine from this task, that is done with LVGL objects now
*               (see r_usb_gui_control.c).
******************************************************************************/
#include    "lcd_controller_if.h"

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)

/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include    <stdio.h>
#include    <string.h>

#include    "r_typedefs.h"
#include    "iodefine.h"
#include    "FreeRTOS.h"
#include    "task.h"
#include    "driver.h"
#include    "tp.h"
#include    "tp_task.h"
#include    "tp_task_usb.h"
#include    "../lcd_controller/usb/inc/r_usb_hhid_dev.h"
#include    "../lcd_controller/usb/inc/r_usb_hhid_apl.h"

/******************************************************************************
Macro definitions
******************************************************************************/
/*! Size of the report buffer. */
#define     TP_USB_REPORT_BUF_LEN      (USB_BUF_LEN)

/******************************************************************************
 Enumerated Types
******************************************************************************/


/******************************************************************************
 Structures
******************************************************************************/


/***********************************************************************************
 Global Vaiables
***********************************************************************************/
bool_t g_task_start_flag = false;

/*******************************************************************************
 Private global variables and functions
*******************************************************************************/
/*! Pointer state shared with the LVGL task, protected with a critical section */
static tp_usb_pointer_t     s_ptr;
static uint8_t              s_press_latch;
static int32_t              s_latch_x;
static int32_t              s_latch_y;

SCOPE_STATIC void  tp_set_finger_info_usb( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum );
SCOPE_STATIC void  tp_set_position_usb_mouse( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum );
SCOPE_STATIC void  tp_set_position_usb_touch( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum );
SCOPE_STATIC void  tp_get_position_data( const uint8_t* const puReadBuf, int16_t *pnPosX, int16_t *pnPosY, const uint32_t undatalen, const uint32_t unposxoffset, const uint32_t unposyoffset );
SCOPE_STATIC int32_t tp_usb_clamp( const int32_t nval, const int32_t nmax );
SCOPE_STATIC void  tp_usb_update_pointer( const tp_touch_finger_t *psTouch, const int_t npenstatus, const uint8_t uismouse );

/**************************************************************************//**
* Function Name: tp_task_process_usb
* @brief         Task function process of USB.
*
*                Description:<br>
*
* @param         None.
* @retval        None.
******************************************************************************/
void tp_task_process_usb( void )
{
    int_t               nret;
    int32_t             nevt_val;
    uint32_t            unevt_flg;
    uint8_t             auread_buf[TP_USB_REPORT_BUF_LEN];
    tp_touch_event_t    stouch_evt;
    tpevt_coordinates_t slast_coords[TP_TOUCHNUM_MAX];
    int_t               npen_status[TP_TOUCHNUM_MAX];
    int_t               ni;
    int_t               nscr_width;
    int_t               nscr_height;
    uint32_t            drvnum;

    tp_get_screen_size( &nscr_width, &nscr_height );

    tp_set_task_status( TP_TASKSTAT_ACTIVE );
    memset( &stouch_evt, 0, sizeof(tp_touch_event_t) );
    memset( slast_coords, 0, sizeof(slast_coords) );
    memset( npen_status, 0, sizeof(npen_status) );
    memset( auread_buf, 0, sizeof(auread_buf) );

    /* the mouse cursor starts at the centre of the screen */
    for( ni = 0; ni < TP_TOUCHNUM_MAX; ni++ )
    {
        slast_coords[ni].x = nscr_width / 2;
        slast_coords[ni].y = nscr_height / 2;
        stouch_evt.sfinger[ni].unposx = (uint16_t)slast_coords[ni].x;
        stouch_evt.sfinger[ni].unposy = (uint16_t)slast_coords[ni].y;
    }

    taskENTER_CRITICAL();
    memset( &s_ptr, 0, sizeof(s_ptr) );
    s_ptr.x       = slast_coords[0].x;
    s_ptr.y       = slast_coords[0].y;
    s_press_latch = 0;
    taskEXIT_CRITICAL();

    g_task_start_flag = true;

    while( 1 )
    {
        nevt_val = tp_wait_evt_msg();
        if( nevt_val < 0 )
        {
            continue;
        }

        /* all pending flags are taken over here, events that occur from now on set the flags again */
        unevt_flg = (uint32_t)nevt_val & TP_EVTFLG_ALL;
        drvnum    = ((uint32_t)nevt_val & TP_DEVADDR_MASK) >> TP_DEVADDR_SHIFT;
        tp_clear_evt_msg( unevt_flg );
        if( drvnum >= USBH1_MAXDEVADDR )
        {
            drvnum = 0;
        }

        if( (unevt_flg & TP_EVTFLG_EXIT) == TP_EVTFLG_EXIT )
        {
            g_task_start_flag = false;
            tp_set_task_status( TP_TASKSTAT_IDLE );

            gp_os_task = NULL;
            R_OS_TaskDelete( NULL );     /* delete this task */
        }

        /* USB device attached / detached */
        if( (unevt_flg & TP_EVTFLG_ATTACH) == TP_EVTFLG_ATTACH )
        {
            taskENTER_CRITICAL();
            s_ptr.attached = 1;
            taskEXIT_CRITICAL();
        }

        if( (unevt_flg & TP_EVTFLG_DETACH) == TP_EVTFLG_DETACH )
        {
            taskENTER_CRITICAL();
            s_ptr.attached = 0;
            s_ptr.pressed  = 0;
            s_ptr.is_mouse = 0;
            s_press_latch  = 0;
            taskEXIT_CRITICAL();

            for( ni = 0; ni < TP_TOUCHNUM_MAX; ni++ )
            {
                npen_status[ni] = TP_PENSTATUS_RELEASE;
            }
        }

        /* new report of the mouse / touch panel */
        if( (unevt_flg & TP_EVTFLG_PENIRQ) == TP_EVTFLG_PENIRQ )
        {
            if( USB_IF_NODEVICE == gs_usb_dev_info[drvnum].interface )
            {
                /* the device was detached after the report */
                continue;
            }

            /* get touch information via USB */
            nret = (int_t)r_lcd_read_cmd( 0, 0, auread_buf, gs_usb_dev_info[drvnum].copy_data_len );
            if( nret != DRV_SUCCESS )
            {
                DBG_PRINTF_ERR( "[ERROR] touch status is not got\n" );
            }
            else
            {
                /* make touch information for output */
                for( ni = 0; ni < TP_TOUCHNUM_MAX; ni++ )
                {
                    tp_set_finger_info_usb( &stouch_evt.sfinger[ni], &slast_coords[ni], &npen_status[ni], auread_buf, drvnum );
                }

                /* pointer state for LVGL */
                tp_usb_update_pointer( &stouch_evt.sfinger[0], npen_status[0],
                                       (uint8_t)((USB_IF_MOUSE == gs_usb_dev_info[drvnum].interface) ? 1u : 0u) );

                /* execute callback function */
                if( tp_get_event_lock_inf() == TP_EVT_UNLOCK )
                {
                    (void)tp_notify_event( &stouch_evt );
                }
            }
        }
    }
}

/**************************************************************************//**
* Function Name: touchpanel_usb_get_pointer
* @brief         Copy the state of the USB pointer for the LVGL indev.
*
*                Description:<br>
*                A press released before this function is called again is
*                reported once as pressed at the position where it happened.
*
* @param         [out]tp_usb_pointer_t *p_ptr   : pointer to output
* @retval        None.
******************************************************************************/
void touchpanel_usb_get_pointer( tp_usb_pointer_t *p_ptr )
{
    taskENTER_CRITICAL();
    *p_ptr          = s_ptr;
    s_ptr.activity  = 0;
    if( 0u != s_press_latch )
    {
        p_ptr->pressed = 1;
        p_ptr->x       = s_latch_x;
        p_ptr->y       = s_latch_y;
        s_press_latch  = 0;
    }
    taskEXIT_CRITICAL();
}

/**************************************************************************//**
* Function Name: tp_usb_update_pointer
* @brief         Update the pointer state that is shared with the LVGL task.
*
*                Description:<br>
*
* @param         [in]tp_touch_finger_t *psTouch : decoded finger information
* @param         [in]int_t npenstatus           : pen status
* @param         [in]uint8_t uismouse           : 1: mouse, 0: touch panel
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_usb_update_pointer( const tp_touch_finger_t *psTouch, const int_t npenstatus, const uint8_t uismouse )
{
    taskENTER_CRITICAL();
    s_ptr.x        = psTouch->unposx;
    s_ptr.y        = psTouch->unposy;
    s_ptr.pressed  = (uint8_t)((TP_PENSTATUS_PUSH == npenstatus) ? 1u : 0u);
    s_ptr.is_mouse = uismouse;
    s_ptr.attached = 1;     /* a report means that the device is there */
    s_ptr.activity = 1;
    if( TPEVT_ENTRY_DOWN == psTouch->estate )
    {
        s_press_latch = 1;
        s_latch_x     = psTouch->unposx;
        s_latch_y     = psTouch->unposy;
    }
    taskEXIT_CRITICAL();
}

/**************************************************************************//**
* Function Name: tp_set_finger_info_usb
* @brief         Branch process with device to set touch information to output table.
*
*                Description:<br>
*
* @param         [out]tp_touch_finger_t *psTouch        : pointer to output table
* @param         [out]tpevt_coordinates_t *psLastCoords : pointer to last coordinate
* @param         [out]int_t *pnPenStatus                : pointer to pending status
* @param         [in]uint8_t *puReadBuf                 : pointer to receive buffer
* @param         [in]uint32_t drvnum                    : number of USB driver
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_set_finger_info_usb( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum )
{
    if( USB_IF_MOUSE == gs_usb_dev_info[drvnum].interface )
    {
        tp_set_position_usb_mouse( psTouch, psLastCoords, pnPenStatus, puReadBuf, drvnum );
    }
    else if( USB_IF_TOUCH == gs_usb_dev_info[drvnum].interface )
    {
        tp_set_position_usb_touch( psTouch, psLastCoords, pnPenStatus, puReadBuf, drvnum );
    }
    else
    {
        DBG_PRINTF_ERR( "[ERROR] unsupported device \n" );
    }
}

/**************************************************************************//**
* Function Name: tp_usb_clamp
* @brief         Limit a coordinate to 0 ... nmax - 1.
*
*                Description:<br>
*
* @param         [in]int32_t nval   : coordinate
* @param         [in]int32_t nmax   : size of the axis [pixel]
* @retval        Limited coordinate.
******************************************************************************/
SCOPE_STATIC int32_t tp_usb_clamp( const int32_t nval, const int32_t nmax )
{
    int32_t nret = nval;

    if( nret >= nmax )
    {
        nret = nmax - 1;
    }
    if( nret < 0 )
    {
        nret = 0;
    }

    return nret;
}

/**************************************************************************//**
* Function Name: tp_set_position_usb_mouse
* @brief         Set position information of USB mouse.
*
*                Description:<br>
*                The pointer is moved by the relative movement of the mouse
*                (relative mouse) and limited to the screen. The left button
*                is the touch: pushed -> DOWN, held -> MOVE, released -> UP.
*
* @param         [out]tp_touch_finger_t *psTouch        : pointer to output table
* @param         [out]tpevt_coordinates_t *psLastCoords : pointer to last coordinate
* @param         [out]int_t *pnPenStatus                : pointer to pending status
* @param         [in]uint8_t *puReadBuf                 : pointer to receive buffer
* @param         [in]uint32_t drvnum                    : number of USB driver number
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_set_position_usb_mouse( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum )
{
    const r_usb_mouse_info_t *p_mouse = gs_usb_dev_info[drvnum].mouse;
    int16_t                  nmovex;
    int16_t                  nmovey;
    int32_t                  nxout;
    int32_t                  nyout;
    int_t                    nscr_width;
    int_t                    nscr_height;
    bool_t                   bleft;

    tp_get_screen_size( &nscr_width, &nscr_height );

    tp_get_position_data( puReadBuf, &nmovex, &nmovey, p_mouse->data_len_pos,
                          p_mouse->data_offset_posx1, p_mouse->data_offset_posy1 );

    if( USB_POS_TYPE_RELATIVE == p_mouse->pos_data_type )
    {
        /* move the pointer by the mouse movement */
        nxout = psLastCoords->x + ((int32_t)nmovex * (int32_t)p_mouse->cursor_speed);
        nyout = psLastCoords->y + ((int32_t)nmovey * (int32_t)p_mouse->cursor_speed);
    }
    else
    {
        /* absolute position of the device scaled to the screen */
        nxout = ((int32_t)nmovex * nscr_width)  / (int32_t)p_mouse->dev_max_posx;
        nyout = ((int32_t)nmovey * nscr_height) / (int32_t)p_mouse->dev_max_posy;
    }

    nxout = tp_usb_clamp( nxout, nscr_width );
    nyout = tp_usb_clamp( nyout, nscr_height );

    psLastCoords->x = nxout;
    psLastCoords->y = nyout;
    psTouch->unposx = (uint16_t)nxout;
    psTouch->unposy = (uint16_t)nyout;

    /* state initialize, a movement without a button is no event */
    psTouch->estate = TPEVT_ENTRY_NONE;

    bleft = (0x00 != (puReadBuf[p_mouse->data_offset_click] & p_mouse->left_click_bit)) ? true : false;

    if( *pnPenStatus == TP_PENSTATUS_PUSH )
    {
        if( false == bleft )
        {
            /* release left click, EVENT : UP */
            *pnPenStatus    = TP_PENSTATUS_RELEASE;        /* Touch Panel pen status, 0 : up */
            psTouch->estate = TPEVT_ENTRY_UP;
        }
        else
        {
            /* hold left click, EVENT : MOVE */
            psTouch->estate = TPEVT_ENTRY_MOVE;
        }
    }
    else
    {
        if( true == bleft )
        {
            /* push left click, EVENT : DOWN */
            *pnPenStatus    = TP_PENSTATUS_PUSH;           /* Touch Panel pen status, 1 : down */
            psTouch->estate = TPEVT_ENTRY_DOWN;
        }
    }
}

/**************************************************************************//**
* Function Name: tp_set_position_usb_touch
* @brief         Set position information of USB touch.
*
*                Description:<br>
*                The absolute position of the touch panel is scaled from the
*                coordinate range of the device to the screen.
*
* @param         [out]tp_touch_finger_t *psTouch        : pointer to output table
* @param         [out]tpevt_coordinates_t *psLastCoords : pointer to last coordinate
* @param         [out]int_t *pnPenStatus                : pointer to pending status
* @param         [in]uint8_t *puReadBuf                 : pointer to receive buffer
* @param         [in]uint32_t drvnum                    : number of USB driver number
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_set_position_usb_touch( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const uint32_t drvnum )
{
    const r_usb_touch_info_t *p_touch = gs_usb_dev_info[drvnum].touch;
    int32_t                  nxin;
    int32_t                  nyin;
    int32_t                  nxout;
    int32_t                  nyout;
    int16_t                  nposx;
    int16_t                  nposy;
    int_t                    nscr_width;
    int_t                    nscr_height;

    /* check finger data */
    if( 0 != puReadBuf[p_touch->finger_info_pos] )
    {
        tp_get_screen_size( &nscr_width, &nscr_height );

        tp_get_position_data( puReadBuf, &nposx, &nposy, p_touch->data_len_pos,
                              p_touch->data_offset_posx1, p_touch->data_offset_posy1 );

        /* convert position data to coordinates corresponding to screen resolution */
        nxin = ((int32_t)nposx * (int32_t)p_touch->dev_resolutionx) / (int32_t)p_touch->dev_max_posx;
        nyin = ((int32_t)nposy * (int32_t)p_touch->dev_resolutiony) / (int32_t)p_touch->dev_max_posy;

#if (USB_TOUCH_INVERT_XY == 1)
        nxin = nscr_width  - nxin;
        nyin = nscr_height - nyin;
#endif

        nxout = tp_usb_clamp( nxin, nscr_width );
        nyout = tp_usb_clamp( nyin, nscr_height );

        psTouch->unposx = (uint16_t)nxout;
        psTouch->unposy = (uint16_t)nyout;

        if( *pnPenStatus == TP_PENSTATUS_RELEASE )
        {
            /* EVENT : DOWN */
            *pnPenStatus    = TP_PENSTATUS_PUSH;        /* Touch Panel pen status, 1 : down */
            psTouch->estate = TPEVT_ENTRY_DOWN;
        }
        else
        {
            /* EVENT : MOVE */
            psTouch->estate = TPEVT_ENTRY_MOVE;
        }

        psLastCoords->x = nxout;
        psLastCoords->y = nyout;
    }
    else
    {
        psTouch->unposx = (uint16_t)psLastCoords->x;
        psTouch->unposy = (uint16_t)psLastCoords->y;

        if( *pnPenStatus == TP_PENSTATUS_PUSH )
        {
            /* EVENT : UP */
            *pnPenStatus    = TP_PENSTATUS_RELEASE;     /* Touch Panel pen status, 0 : up */
            psTouch->estate = TPEVT_ENTRY_UP;
        }
        else
        {
            /* EVENT : NONE */
            psTouch->estate = TPEVT_ENTRY_NONE;
        }
    }
}

/**************************************************************************//**
* Function Name: tp_get_position_data
* @brief         Get position data from USB Buffer.
*
*                Description:<br>
*
* @param         [in]uint8_t *puReadBuf                 : pointer to receive buffer
* @param         [out]int16_t *pnPosX                   : X coordinate position data
* @param         [out]int16_t *pnPosY                   : Y coordinate position data
* @param         [in]uint32_t undatalen                 : data length of coordinate value in buffer
* @param         [in]uint32_t unposxoffset              : position of X coordinate data in buffer
* @param         [in]uint32_t unposyoffset              : position of Y coordinate data in buffer
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_get_position_data( const uint8_t* const puReadBuf, int16_t *pnPosX, int16_t *pnPosY, const uint32_t undatalen, const uint32_t unposxoffset, const uint32_t unposyoffset )
{
    if( 1u == undatalen )
    {
        /* 8bit signed coordinate data, it's a data such as mouse mainly */
        *pnPosX = (int16_t)(int8_t)puReadBuf[unposxoffset];
        *pnPosY = (int16_t)(int8_t)puReadBuf[unposyoffset];
    }
    else
    {
        /* 16bit little endian coordinate data, it's a data such as touch mainly */
        *pnPosX = (int16_t)(((uint32_t)puReadBuf[unposxoffset + 1u] << 8) | (uint32_t)puReadBuf[unposxoffset]);
        *pnPosY = (int16_t)(((uint32_t)puReadBuf[unposyoffset + 1u] << 8) | (uint32_t)puReadBuf[unposyoffset]);
    }
}

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */
