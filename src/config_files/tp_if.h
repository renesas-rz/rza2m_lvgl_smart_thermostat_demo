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
 * and to discontinue the availability of this software. By using this
 * software, you agree to the additional terms and conditions found by
 * accessing the following link:
 * http://www.renesas.com/disclaimer
*******************************************************************************
* Copyright (C) 2018 Renesas Electronics Corporation. All rights reserved.
 *****************************************************************************/
/******************************************************************************
 * @headerfile     tp_if.h
 * @brief          Interface to the touch panel 
 * @version        1.00
 * @date           27.06.2018
 * H/W Platform    RZ/A1LU
 *****************************************************************************/
/*****************************************************************************
 * History      : DD.MM.YYYY Ver. Description
 *              : 21.12.2016 1.00 First Release
 *****************************************************************************/
/* Multiple inclusion prevention macro */
#ifndef     TP_IF_H
#define     TP_IF_H
/**************************************************************************//**
 * @ingroup R_SW_PKG_93_NONOS_MIDDLEWARE
 * @defgroup R_SW_PKG_93_TOUCH_API Touch
 * @brief Touch Panel Interface
 *
 * @anchor R_SW_PKG_93_TP_INTERFACE_API_SUMMARY 
 * @par Summary
 * This is a Touch Panel Interface middleware module, 
 * which allows the user to write to and read from the registers of 
 * the touch panels, detecting touch events and receiving their coordinates. 
 * This  module is specific to the Touch panel, but not the device or board.
 *
 * @anchor R_SW_PKG_93_TP_INTERFACE_API_INSTANCES 
 * @par Known Implementations:
 * This middleware is used in the RZA1LU Software Package.
 * @see RENESAS_APPLICATION_SOFTWARE_PACKAGE
 * @see RENESAS_OS_ABSTRACTION  Renesas OS Abstraction interface
 * @{
 *****************************************************************************/
/******************************************************************************
Includes <System Includes> , "Project Includes"
******************************************************************************/

#include "lcd_controller_if.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/******************************************************************************
Macro definitions
******************************************************************************/
#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
#define TP_TOUCHNUM_MAX (2)
#else  /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
#define TP_TOUCHNUM_MAX (1)
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */

/*************************************************************************
 Enumerated Types
*************************************************************************/
/*! The type of touch panel event entry */
typedef enum {
    TPEVT_ENTRY_NONE    = 0x0000,      /*!< None */
    TPEVT_ENTRY_UP      = 0x0001,      /*!< Up */
    TPEVT_ENTRY_DOWN    = 0x0002,      /*!< Down */
    TPEVT_ENTRY_MOVE    = 0x0004,      /*!< Move */
    TPEVT_ENTRY_ALL     = 0x0007,      /*!< All */
    TPEVT_ENTRY_UNKNOWN = 0x8000       /*!< internal event state */
} tpevt_entry_type_t ;


/*************************************************************************
 Structures
*************************************************************************/
typedef struct {
    tpevt_entry_type_t   estate;
    uint16_t             unposx;
    uint16_t             unposy;
} tp_touch_finger_t;

typedef struct {
    tp_touch_finger_t   sfinger[TP_TOUCHNUM_MAX];
} tp_touch_event_t;

typedef void (*TpCBFunc)(int_t, tp_touch_event_t*);


/******************************************************************************
 Global Vaiables
******************************************************************************/


/******************************************************************************
 Function Prototypes
******************************************************************************/

/******************************************************************************
* Function Name: touchpanel_init
* Description  : Initializes the touch panel driver by calling the tp_init.
* Arguments    : none
* Return Value : none
******************************************************************************/
extern void  touchpanel_init( void );

/******************************************************************************
* Function Name: touchpanel_open
* Description  : Generates and initializes a touch panel task by calling the 
*                tp_open.
*                Do not call this function during touch panel utility has been 
*                opened.
* Arguments    : nWidth  - screen width.
*              : nHeight - screen height.
*              : unIrqLv - IRQ interrupt level.
*              : nTskPri - task priority.
*              : unTskStk - task stack size.
* Return Value : 0 - Operation successfull.
*               -1 - Error occured. 
******************************************************************************/
extern int_t touchpanel_open( const int_t nWidth, const int_t nHeight, const uint32_t unIrqLv, const int16_t nTskPri, const uint32_t unTskStk );

/**
 * @brief      Touch Panel utility close function
 * @retval     NONE
*/
extern int_t touchpanel_close( void );

/******************************************************************************
* Function Name: touchpanel_event_entry
* Description  : Registers a call-back function linked to the LCD area 
*                where a touch panel event occurs in the touch panel event 
*                management structure.
* 
*                Calls the function described in tp_event_entry, to perform the
*                following processing:
*                Searching for a free area in the touch panel event table 
*                (Up to 16 touch panel events can be registered,
*                and error processing occurs if no free area is available.)
*                Making "specified touch action," 
*                       "X coordinate of specified area," 
*                       "Y coordinate of specified area," 
*                       "width of specified area," 
*                       "height of specified area," 
*                       "specified call-back function" 
*                settings for the touch panel event table free area.
*  
*                Note: If events occur simultaneously in multiple registered 
*                      areas that overlap, the associated call-back functions 
*                      are executed in order, starting with the one with the 
*                      lowest event ID.
* Arguments    : nWidth  - screen width.
*              : nHeight - screen height.
*              : unIrqLv - IRQ interrupt level.
*              : nTskPri - task priority.
*              : unTskStk - task stack size.
* Return Value : 0 to (TPEVT_ENTRY_MAX -1) - Success event ID
*               -1 - Fail returns. 
******************************************************************************/
extern int_t touchpanel_event_entry( const tpevt_entry_type_t eMode, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight, const TpCBFunc function );

/******************************************************************************
* Function Name: touchpanel_event_erase
* Description  : Removes registration information for the specified event ID 
*                from the touch panel event management structure.
*                Calls the function described in tp_event_erase, to perform 
*                the following processing:
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX)
*                Disabling of event associated with event ID.
* Arguments    : nId  - Event ID.
* Return Value : 0 - normal end.
*               -1 - event removal failure. 
******************************************************************************/
extern int_t touchpanel_event_erase( const int_t nId );

/******************************************************************************
* Function Name: touchpanel_change_event_entry
* Description  : Changes the LCD area of the specified event ID.
*                Calls the function described in tp_change_event_entry, 
*                to perform the following processing:
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX),
*                Event ID checking (unregistered ID or removed ID) 
*                Registration of event in area of specified ID in touch panel 
*                event table.
* Arguments    : nId  - Event ID.
*                nPosX  - X coordinate of area after change.
*                nPosY  - Y coordinate of area after change.
*                nWidth  - Width of area after change.
*                nHeight  - Height of area after change.
* Return Value : 0 - normal end.
*               -1 - LCD area change failure. 
******************************************************************************/
extern int_t touchpanel_change_event_entry( const int_t nId, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight );

/******************************************************************************
* Function Name: touchpanel_event_lock_all
* Description  : Locks processing of all touch panel events.
*                Calls the function described in tp_event_lock_all, 
*                to perform the following processing:
*                Setting all events in the touch panel event table to the 
*                locked state.
* Arguments    : none
* Return Value : 0 - normal end.
*               -1 - touch panel event locking failure. 
******************************************************************************/
extern int_t touchpanel_event_lock_all( void );

/******************************************************************************
* Function Name: touchpanel_event_lock_all
* Description  : Unlocks processing of all touch panel events.
*                Calls the function described in tp_event_unlockall, 
*                to perform the following processing:
*                Setting all events in the touch panel event table to the 
*                unlocked state.
* Arguments    : none
* Return Value : 0 - normal end.
*               -1 - touch panel event unlocking failure. 
******************************************************************************/
extern int_t touchpanel_event_unlock_all( void );

/******************************************************************************
* Function Name: touchpanel_event_lock
* Description  : Locks processing of the touch panel event specified by the 
*                event ID.
*                Calls the function described in tp_event_lock, to perform 
*                the following processing:
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX)
*                Setting the event specified by the event ID to the locked 
*                state in the touch panel event table.
* Arguments    : nId - Event ID
* Return Value : 0 - normal end.
*               -1 - event removal failure. 
******************************************************************************/
extern int_t touchpanel_event_lock( const int_t nId );

/******************************************************************************
* Function Name: touchpanel_event_unlock
* Description  : Unlocks processing of the touch panel event specified by the 
*                event ID.
*                Calls the function described in tp_event_unlock, 
*                to perform the following processing:
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX)
*                Setting the event specified by the event ID to the unlocked 
*                state in the touch panel event table.
* Arguments    : nId - Event ID
* Return Value : 0 - normal end.
*               -1 - event removal failure. 
******************************************************************************/
extern int_t touchpanel_event_unlock( const int_t nId );

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)
/******************************************************************************
 * USB pointing device (mouse / touch monitor) -> LVGL pointer indev
 *****************************************************************************/
/*! Latest state of the USB pointing device, in screen pixels. */
typedef struct {
    int32_t     x;          /*!< x-coordinate [pixel]                                 */
    int32_t     y;          /*!< y-coordinate [pixel]                                 */
    uint8_t     pressed;    /*!< 1: left button / finger is down                      */
    uint8_t     attached;   /*!< 1: a USB pointing device is connected                */
    uint8_t     is_mouse;   /*!< 1: device is a mouse (cursor required), 0: touch     */
    uint8_t     activity;   /*!< 1: new report since the previous call                */
} tp_usb_pointer_t;

/******************************************************************************
* Function Name: touchpanel_usb_get_pointer
* Description  : Copies the latest USB pointer state. A press that is released
*                before the caller polls again is still reported once as
*                pressed (at the position where it happened), so that a fast
*                click is not lost by a slow poller such as the LVGL indev.
*                Call from the LVGL task.
* Arguments    : p_ptr - pointer to the output structure.
* Return Value : none
******************************************************************************/
extern void touchpanel_usb_get_pointer( tp_usb_pointer_t *p_ptr );
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif      /* TP_IF_H */
/**************************************************************************//**
 * @} (end addtogroup)
 *****************************************************************************/
