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
* @file          tp.h
* Rev: 30 
* Date:: 2016-12-21 12:02:44 +0900#
* @brief         TouchPanel Driver internal hedder
******************************************************************************/
#include "r_os_abstraction_api.h"
#include "tp_if.h"

#ifndef     TP_H
#define     TP_H

/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/

#if defined(__cplusplus)
extern "C" {
#endif


/******************************************************************************
Macro definitions
******************************************************************************/
#define DBG_LEVEL_OT                (-1)                /* onetime debug */
#define DBG_LEVEL_DEF               (0)                 /* default */
#define DBG_LEVEL_ERR               (1)                 /* error */
#define DBG_LEVEL_MSG               (2)                 /* message */
#define DBG_LEVEL_LOG               (3)                 /* log */
#define DBG_LEVEL_DBG               (4)                 /* debug */

#define DBG_LEVEL                   (DBG_LEVEL_ERR)
#if (DBG_LEVEL >= DBG_LEVEL_OT)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_OT(...)              (printf( __VA_ARGS__))
#else
#define DBG_PRINTF_OT(...)
#endif
#if (DBG_LEVEL >= DBG_LEVEL_DEF)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_DEF(...)             (printf(__VA_ARGS__))
#else
#define DBG_PRINTF_DEF(...)
#endif
#if (DBG_LEVEL >= DBG_LEVEL_ERR)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_ERR(...)             (printf(__VA_ARGS__))
#else
#define DBG_PRINTF_ERR(...)
#endif
#if (DBG_LEVEL >= DBG_LEVEL_MSG)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_MSG(...)             (printf(__VA_ARGS__))
#else
#define DBG_PRINTF_MSG(...)
#endif
#if (DBG_LEVEL >= DBG_LEVEL_LOG)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_LOG(...)             (printf(__VA_ARGS__))
#else
#define DBG_PRINTF_LOG(...)
#endif
#if (DBG_LEVEL >= DBG_LEVEL_DBG)
/* Casting a variable-length argument to a printf macro */
#define DBG_PRINTF_DBG(...)             (printf(__VA_ARGS__))
#else
#define DBG_PRINTF_DBG(...)
#endif

#ifdef  __DEBUG
#define SCOPE_STATIC
#else   /* __DEBUG */
#define SCOPE_STATIC    static
#endif  /* __DEBUG */

/*! The max number of event entry */
#define     TPEVT_ENTRY_MAX         (16)

/*! USB: the USB driver number of the reporting device travels in the upper 16 bit of the event message */
#define     TP_DEVADDR_SHIFT        (16)
#define     TP_DEVADDR_MASK         (0xFFFF0000UL)

/*! Pen status */
#define     TP_PENSTATUS_RELEASE    (0)
#define     TP_PENSTATUS_PUSH       (1)

#define        TP_EVTFLG_NONE             (0x00000000)
/*! Touch Panel event flag, pen interrupt */
#define        TP_EVTFLG_PENIRQ           (0x00000001)
/*! Touch Panel event flag, USB only, device attached */
#define        TP_EVTFLG_ATTACH           (0x00000002)
/*! Touch Panel event flag, USB only, device detached */
#define        TP_EVTFLG_DETACH           (0x00000004)
/*! Touch Panel event flag, exit and delete task */
#define        TP_EVTFLG_EXIT             (0x00000080)
#define        TP_EVTFLG_ALL              (TP_EVTFLG_PENIRQ | TP_EVTFLG_ATTACH | TP_EVTFLG_DETACH | TP_EVTFLG_EXIT)


/******************************************************************************
 Enumerated Types
******************************************************************************/
/*! Touch panel event lock state */
typedef enum {
    TP_EVT_UNLOCK = 0,  /*!< Unlocked */
    TP_EVT_LOCK         /*!< Locked */
} tpevt_lock_state_t ;


/******************************************************************************
 Structures
******************************************************************************/
/*! Coordinate structure */
typedef struct {
    int32_t x ;        /*!< x-coordinate [pixel] */
    int32_t y ;        /*!< y-coordinate [pixel] */
} tpevt_coordinates_t ;

/*! Event entry struct */
typedef struct {
    /*!< The type of touch panel event entry */
    tpevt_entry_type_t  mode ;
    /*!< The upper-left coordinates of the rectangular area 
    in which touch event can be received. [pixel] */
    tpevt_coordinates_t st ;
    /*!< The lower-right coordinates of the rectangular area 
    in which touch event can be received. [pixel] */
    tpevt_coordinates_t ed ;
    /*!< Event notification callback function pointer */
    void (*function)( int_t nId, tp_touch_event_t* psTouchEvt) ;
    tpevt_lock_state_t evtlock ;     /*!< Event lock state */
} tpevt_entry_t ;


/******************************************************************************
 Global Vaiables
******************************************************************************/
extern os_task_t*  gp_os_task;


/******************************************************************************
 Function Prototypes
******************************************************************************/

/******************************************************************************
* Function Name: tp_init
* Description  : Initializes internal variables of the touch panel driver.
*                Securing of touch panel event entry area.
*                Setting of internal variable nEvtEntryId  to -1.
*                Setting of internal variable TpEvtLockInf to TP_EVT_UNLOCK.
* Arguments    : none
* Return Value : none 
******************************************************************************/
void            tp_init( void );

/******************************************************************************
* Function Name: tp_open
* Description  : Opens the touch panel driver.
*                Setting the LCD size in the driver's variables ScreenWidth 
*                and ScreenHeight.
*                Generation of touch panel task synchronization semaphore.
*                Generation of touch panel task.
*                Setting of task priority of touch panel task.
*                Opening of communication environment with LCD board.
*                Registration of call-back event when touch panel interrupt 
*                occurs in LCD event
* Arguments    : nWidth - screen width.
*              : nHeight - nHeight screen height.
*              : unIrqLv - IRQ interrupt level.
*              : nTskPri - task priority.
*              : unTskStk - task stack size.
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_open( const int_t nWidth, const int_t nHeight, const uint32_t unIrqLv, const int16_t nTskPri, const uint32_t unTskStk );

/******************************************************************************
* Function Name: tp_close
* Description  : Closes the touch panel driver.
*                Removal of call-back event when touch panel interrupt occurs 
*                in LCD event.
*                Removal of all touch panel event registrations by the user.
*                Removal of touch panel task. 
*                Removal of semaphore for synchronization with the touch panel 
*                task.
* Arguments    : none
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_close( void );

/******************************************************************************
* Function Name: tp_event_entry
* Description  : Registers in the event table a call-back function linked to 
*                a touch panel interrupt.
*                After registration finishes, the event ID is sent as a return
*                value.
*                Searching for a free area in the touch panel event table 
*                (Up to 16 touch panel events can be registered, and error 
*                processing occurs if no free area is available).
*                Making "specified touch action," 
*                       "X coordinate of specified area," 
*                       "Y coordinate of specified area," 
*                       "width of specified area,"
*                       "height of specified area," 
*                       "specified call-back function" 
*                settings for the touch panel event table free area.
*                Note: When "X coordinate of specified area," 
*                           "Y coordinate of specified area," 
*                           "width of specified area,"\n
*                      and "height of specified area" are registered 
*                      in the touch panel event.
*                table, the following processing is performed to register 
*                the result as a rectangular area:
*                st.x (X coordinate of area start position) 
*                       <- "X coordinate of specified area"
*                st.y (Y coordinate of area start position)
*                       <- "Y coordinate of specified area"\n
*                ed.x (X coordinate of area end position) 
*                       <- ("X coordinate of specified area" - 
*                        "width of specified area")
*                ed.y (Y coordinate of area end position) 
*                       <- ("Y coordinate of specified area" - 
*                        "height of specified area")
* Arguments    : eMode - event type.
*              : nPosX - X-coordinate of LCD area.
*              : nPosY - Y-coordinate of LCD area.
*              : nWidth - width of LCD area.
*              : nHeight - height of LCD area.
*              : function - callback function.
* Return Value : 0 to (TPEVT_ENTRY_MAX-1)
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_entry( const tpevt_entry_type_t eMode, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight, const TpCBFunc function );

/******************************************************************************
* Function Name: tp_event_erase
* Description  : Removes an event from the call-back event table of the 
*                touch panel driver.
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX)
*                Disabling of event associated with event ID (TPEVT_ENTRY_NON).
* Arguments    : nid - event ID.
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_erase( const int_t nid );

/******************************************************************************
* Function Name: tp_change_event_entry
* Description  : The rectangular area to which the event ID specified by the 
*                1st argument (nid) is registered is changed to the rectangular 
*                area specified by the 2nd to 5th arguments.
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX). 
*                Event ID checking (unregistered ID or removed ID).
*                Registration of event in area of specified ID in touch panel 
*                event table.
* Arguments    : nid - event ID.
*              : nPosX - X-coordinate of LCD area.
*              : nPosY - Y-coordinate of LCD area.
*              : nWidth - width of LCD area.
*              : nHeight - height of LCD area.
* Return Value : 0 to (TPEVT_ENTRY_MAX-1)
*               -1 - Error occured 
******************************************************************************/
int_t           tp_change_event_entry( const int_t nid, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight );

/******************************************************************************
* Function Name: tp_event_lock_all
* Description  : Locks all registered touch panel call-back events.
*                Calls the function described in tp_event_lock, to set all 
*                events to the locked state (TP_EVT_LOCK).
* Arguments    : none
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_lock_all( void );

/******************************************************************************
* Function Name: tp_event_unlockall
* Description  : Unlocks all registered touch panel call-back events.
*                Calls the function described in tp_event_unlock, to set all 
*                events to the unlocked state (TP_EVT_UNLOCK).
* Arguments    : none
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_unlockall( void );

/******************************************************************************
* Function Name: tp_event_lock
* Description  : Locks the touch panel call-back event specified by the 
*                1st argument (nid).
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX).
*                Setting the event specified by the event ID to the locked 
*                state (TP_EVT_LOCK) in the touch panel event table.
* Arguments    : none
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_lock( const int_t nid );

/******************************************************************************
* Function Name: tp_event_unlock
* Description  : Unlocks the touch panel call-back event specified by the 
*                1st argument (nid).
*                Event ID checking (within range of 0 to TPEVT_ENTRY_MAX).
*                Setting the event specified by the event ID to the unlocked 
*                state (TP_EVT_UNLOCK) in the touch panel event table.
* Arguments    : nid - event ID.
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int_t           tp_event_unlock( const int_t nid );

/******************************************************************************
* Function Name: tp_get_event_table
* Description  : Acquires from the touch panel driver call-back event table 
*                the pointer address at which the event ID event information is 
*                registered.
* Arguments    : nid - event ID.
* Return Value : tpevt_entry_t pointer to event 
******************************************************************************/
tpevt_entry_t     *tp_get_event_table( const int_t nid );

/******************************************************************************
* Function Name: tp_get_event_lock_inf
* Description  : Acquires the lock state of the touch panel call-back event.
* Arguments    : none
* Return Value : TP_EVT_LOCK - In locked state
*                TP_EVT_UNLOCK - In unlocked state
******************************************************************************/
tpevt_lock_state_t tp_get_event_lock_inf( void );

/******************************************************************************
* Function Name: tp_get_screen_size
* Description  : Acquires the screen size of the LCD panel.
* Arguments    : *pnWidth - pointer to width value.
*              : *pnHeight - pointer to height value.
* Return Value : none
******************************************************************************/
void            tp_get_screen_size( int_t *pnWidth, int_t *pnHeight );

/******************************************************************************
* Function Name: tp_send_evt_msg
* Description  : Sends a synchronization event message.
* Arguments    : unEvtFlg - event flag
* Return Value : 0 - Operation successfull
*               -1 - Error occured 
******************************************************************************/
int32_t         tp_send_evt_msg( const uint32_t unEvtFlg );

/******************************************************************************
* Function Name: tp_get_app_board_version
* Description  : Return touch panel version information.
* Arguments    : none
* Return Value : 1 - Touch panel version1
*                2 - Touch panel version2
******************************************************************************/
int_t           tp_get_app_board_version( void );

/******************************************************************************
* Function Name: tp_wait_evt_msg
* Description  : Waits to receive a synchronization event message.
* Arguments    : none
* Return Value : TP_EVTFLG_NONE - No event flags
*                TP_EVTFLG_PENIRQ - Interrupt pending
*                TP_EVTFLG_EXIT - End task
*                TP_EVTFLG_ALL - Both Interrupt pending and exit flag
*                -1 - Error occured
******************************************************************************/
int32_t         tp_wait_evt_msg( void );

/******************************************************************************
* Function Name: tp_clear_evt_msg
* Description  : Clears the specified event flag.
* Arguments    : unEvtFlg - event flag
* Return Value : none
******************************************************************************/
void            tp_clear_evt_msg( const uint32_t unEvtFlg );

#if defined(__cplusplus)
}
#endif


#endif      /* TP_H */
