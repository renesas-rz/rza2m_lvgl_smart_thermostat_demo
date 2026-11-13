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
 * Copyright (C) 2015 Renesas Electronics Corporation. All rights reserved.
 *****************************************************************************/
/******************************************************************************
* File Name    : r_usb_hhid_apl.c
* Description  : USB Host HID application code
******************************************************************************/
/******************************************************************************
* History : DD.MM.YYYY Version Description
*         : 31.08.2015 1.00    First Release
******************************************************************************/
#include "lcd_controller_if.h"

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_USB)

/******************************************************************************
Includes   <System Includes> , "Project Includes"
******************************************************************************/
#include "inc/r_usb_hhid_apl.h"
#include "inc/r_usb_hhid_control.h"
#include "inc/r_usb_hhid_dev.h"
#include "r_usbh0_hhid_config.h"
#include "r_usbh1_hhid_config.h"

#include "r_usbh0_basic_if.h"
#include "r_usbh0_basic_local.h"
#include "r_usbh0_rtos.h"

#include "r_usbh1_basic_if.h"
#include "r_usbh1_basic_local.h"
#include "r_usbh1_rtos.h"

#include "r_os_abstraction_api.h"
#include "driver.h"
#include "iodefine.h"
#include "tp_if.h"
#include "tp.h"

/******************************************************************************
Global Vaiables
******************************************************************************/
uint8_t                 g_usb_data[USB_BUF_LEN];

r_usb_dev_info_t        gs_usb_dev_info[USBH1_MAXDEVADDR];

/******************************************************************************
Private global variables and functions
******************************************************************************/
static uint32_t         s_lcd_sem_id_data;
static usb_evt_entry_t   s_lcd_evt_entry[LCDEVT_ENTRY_MAX];

static void usb_main_task( void *params );

/******************************************************************************
Function Name   : usb_open
Description     : Open USB
Arguments       : nTskPri : USB task priority
Return value    : 0  : Operation successfull
                : -1 : Error occurred
******************************************************************************/
int_t usb_open( const int16_t nTskPri )
{
    int_t       nret = 0;
    os_task_t   *p_task;

    memset( &s_lcd_evt_entry, 0, sizeof(s_lcd_evt_entry) );

    /** create access semaphore for driver */
    if( R_OS_SemaphoreCreate( &g_slcd_sem_id_acc, 1 ) != true )
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore for internal access is not created\n" );
        nret = -1;
    }

    /* create data copy semaphore */
    if( nret == 0 )
    {
        if( R_OS_SemaphoreCreate( &s_lcd_sem_id_data, 1 ) != true )
        {
            DBG_PRINTF_ERR( "[ERROR] semaphore for data copy is not created\n" );
            nret = -1;
        }
    }

    if( nret == 0 )
    {
        usbh0_cstd_PortInit();      /* Initialize USB Port0 */
        usbh1_cstd_PortInit();      /* Initialize USB Port1 */

        R_USBH0_Init();
        R_USBH1_Init();

        usbh0_hid_init();
        usbh1_hid_init();

        /* Create a task to run the usb */
        p_task = R_OS_TaskCreate( "USB_Mouse", usb_main_task, NULL, R_OS_ABSTRACTION_DEFAULT_STACK_SIZE, (uint32_t)nTskPri );
        if( ( NULL == p_task ) || ( (os_task_t *)R_OS_ABSTRACTION_INVALID_HANDLE == p_task ) )
        {
            DBG_PRINTF_ERR( "[ERROR] USB task is not created\n" );
            nret = -1;
        }
    }

    if( nret != 0 )
    {
        R_OS_SemaphoreDelete( &g_slcd_sem_id_acc );
        R_OS_SemaphoreDelete( &s_lcd_sem_id_data );
    }

    return nret;
}

/******************************************************************************
 * End of function usb_open
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_close
Description     : Close USB
Arguments       : none
Return value    : 0  : Operation successfull
                : -1 : Error occurred
******************************************************************************/
int_t usb_close( void )
{
    int_t nret = 0;

#if (BSP_CFG_RTOS_USED == 1)
    R_USBH0_HstdMgrClose();      /* USB0 MGR Close */
    R_USBH1_HstdMgrClose();      /* USB1 MGR Close */
#endif

    /** delete access semaphore */
    if( g_slcd_sem_id_acc != 0 )
    {
        R_OS_SemaphoreDelete( &g_slcd_sem_id_acc );
        g_slcd_sem_id_acc = 0;
    }
    else
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore for internal access is not deleted\n" );
        nret = -1;
    }

    /** delete synchronous semaphore */
    if( s_lcd_sem_id_data != 0 )
    {
        R_OS_SemaphoreDelete( &s_lcd_sem_id_data );
        s_lcd_sem_id_data = 0;
    }
    else
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore for data copy is not deleted\n" );
        nret = -1;
    }

    return nret;
}

/******************************************************************************
 * End of function usb_close
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_read_cmd
Description     : read position data of USB mouse / touch
Arguments       : *puData   : pointer to receive buffer
                : unSize    : size of the data to copy
Return value    : 0  : Operation successfull
                : -1 : Error occurred
******************************************************************************/
int_t usb_read_cmd( uint8_t *puData, const uint32_t unSize )
{
    int_t       nret = 0;
    uint32_t    data_num;

    if( R_OS_SemaphoreWait( &s_lcd_sem_id_data, R_OS_ABSTRACTION_EV_WAIT_INFINITE ) != true )
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore error for data\n" );
        nret = -1;
    }
    else
    {
        for( data_num = 0; ( data_num < unSize ) && ( data_num < USB_BUF_LEN ); data_num++ )
        {
            *( puData + data_num ) = g_usb_data[data_num];
        }
        R_OS_SemaphoreRelease( &s_lcd_sem_id_data );
    }

    return nret;
}

/******************************************************************************
 * End of function usb_read_cmd
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_event_entry
Description     : Enter callback event to event table.
Arguments       : eType      : event type
                : *function  : callback function
Return value    : 0..(LCDEVT_ENTRY_MAX-1) : event ID
                : -1 : Error occurred
******************************************************************************/
int_t usb_event_entry( const lcdevt_entry_type_t eType, const LcdCBFunc function )
{
    int_t           ni;
    usb_evt_entry_t *p_entry;

    ni = -1;

    if( ( eType > LCDEVT_ENTRY_NONE ) && ( (int_t)eType <= LCDEVT_ENTRY_MAX ) )
    {
        p_entry = &s_lcd_evt_entry[ (int_t)eType - 1 ];
        if( p_entry->mode == LCDEVT_ENTRY_NONE )
        {
            p_entry->mode     = eType;
            p_entry->function = function;
            ni                = (int_t)eType - 1;
        }
    }
    /** Return ERROR ( -1 ), when the event entry fails. */
    return ni;
}

/******************************************************************************
 * End of function usb_event_entry
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_event_erase
Description     : Erase callback event from event table.
Arguments       : nId  : event ID
Return value    : 0  : Operation successfull
                : -1 : Error occurred
******************************************************************************/
int_t usb_event_erase( const int_t nId )
{
    int_t nret = 0;

    if( ( nId < 0 ) || ( nId >= LCDEVT_ENTRY_MAX ) )
    {
        nret = -1;         /* id error */
    }
    else
    {
        s_lcd_evt_entry[ nId ].mode = LCDEVT_ENTRY_NONE;
    }

    return nret;
}

/******************************************************************************
 * End of function usb_event_erase
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_get_event_table
Description     : Get assigned callback event.
Arguments       : nId  : event ID
Return value    : pointer to event table,
******************************************************************************/
usb_evt_entry_t *usb_get_event_table( const int_t nId )
{
    return &s_lcd_evt_entry[ nId ];
}

/******************************************************************************
 * End of function usb_get_event_table
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_wait_sync_buffer
Description     : Synchronize access to buffer.
Arguments       : none
Return value    : 0  : sync success
                  -1 : semaphore wait error
******************************************************************************/
int_t usb_wait_sync_buffer( void )
{
    int_t nret = 0;

    if( R_OS_SemaphoreWait( &s_lcd_sem_id_data, R_OS_ABSTRACTION_EV_WAIT_INFINITE ) != true )
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore error for data\n" );
        nret = -1;
    }

    return nret;
}

/******************************************************************************
 * End of function usb_wait_sync_buffer
 *****************************************************************************/

/******************************************************************************
Function Name   : usb_release_sync_buffer
Description     : Synchronize access to buffer.
Arguments       : none
Return value    : none
******************************************************************************/
void usb_release_sync_buffer( void )
{
    R_OS_SemaphoreRelease( &s_lcd_sem_id_data );
}

/******************************************************************************
 * End of function usb_release_sync_buffer
 *****************************************************************************/


/******************************************************************************
Function Name   : usb_main_task
Description     : USB task
Arguments       : params : not used
Return value    : none
******************************************************************************/
static void usb_main_task( void *params )
{
    UNUSED_PARAM(params);

    while( 1 )
    {
        /* Scheduler */
        R_USBH0_CstdScheduler();
#if (BSP_CFG_RTOS_USED == 0)

        if( USBH0_FLGSET == R_USBH0_CstdCheckSchedule() )
        {
            R_USBH0_HstdMgrTask();          /* MGR task */
            R_USBH0_HhubTask();             /* HUB task */
            R_USBH0_HhidTask();             /* HHID Task */
        }
#endif /* (BSP_CFG_RTOS_USED == 0)*/
        usbh0_hid_main();

        /* Scheduler */
        R_USBH1_CstdScheduler();
#if (BSP_CFG_RTOS_USED == 0)

        if( USBH1_FLGSET == R_USBH1_CstdCheckSchedule() )
        {
            R_USBH1_HstdMgrTask();          /* MGR task */
            R_USBH1_HhubTask();             /* HUB task */
            R_USBH1_HhidTask();             /* HHID Task */
        }
#endif /* (BSP_CFG_RTOS_USED == 0)*/
        usbh1_hid_main( g_usb_data );
        R_OS_TaskSleep( USB_TASK_DELAY );
    }
}
/******************************************************************************
 * End of function usb_main_task
 *****************************************************************************/

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_USB) */
