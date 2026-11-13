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
 * File Name :   r_usb_hhid_control.h
 * @file         r_usb_hhid_control.h
 * @version      0.02
 * @brief        USB HID (mouse / touch) control of the LCD controller interface.
 *               Debug macros (DBG_PRINTF_xxx) and SCOPE_STATIC are taken from tp.h.
 ******************************************************************************/
#ifndef R_USB_HHID_CONTROL_H
#define R_USB_HHID_CONTROL_H

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r_typedefs.h"
#include "lcd_controller_if.h"

/******************************************************************************
 Macro definitions
 ******************************************************************************/
/* LCD event entry number */
#define LCDEVT_ENTRY_MAX                    (1)

/* USB task delay time */
#define USB_TASK_DELAY                      (1)

/* Time that USB event did not occur */
#define USB_NO_EVENT_TIME                   (20)

/******************************************************************************
 Enumerated Types
 ******************************************************************************/
/*! USB event lock state */
typedef enum {
    USB_EVT_UNLOCK = 0,  /*!< Unlocked */
    USB_EVT_LOCK         /*!< Locked */
} usb_evt_lock_state_t;

/*! USB event entry struct */
typedef struct {
    lcdevt_entry_type_t  mode;       /*!< The type of touch panel event entry */
    LcdCBFunc            function;   /*!< Callback, argument: USB driver number (cast to void *) */
    usb_evt_lock_state_t evtlock;    /*!< Event lock state */
} usb_evt_entry_t;

/******************************************************************************
 Exported global variables and functions (to be accessed by other files)
 ******************************************************************************/
extern uint32_t g_slcd_sem_id_acc;

/* r_usb_hhid_apl.c */
int_t           usb_open( const int16_t nTskPri );
int_t           usb_close( void );
int_t           usb_read_cmd( uint8_t *puData, const uint32_t unSize );
int_t           usb_event_entry( const lcdevt_entry_type_t eType, const LcdCBFunc function );
int_t           usb_event_erase( const int_t nId );
usb_evt_entry_t *usb_get_event_table( const int_t nId );
int_t           usb_wait_sync_buffer( void );
void            usb_release_sync_buffer( void );

#endif      /* R_USB_HHID_CONTROL_H */

/******************************************************************************
 End  Of File
 ******************************************************************************/
