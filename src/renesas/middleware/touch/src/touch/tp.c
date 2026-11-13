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
* @file         tp.c
* @brief        TouchPanel Driver functions
******************************************************************************/

/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include    <stdio.h>
#include    <string.h>

#include    "r_typedefs.h"
#include    "compiler_settings.h"
#include    "r_os_abstraction_api.h"
#include    "FreeRTOS.h"
#include    "task.h"

#include    "lcd_controller_if.h"
#include    "tp.h"
#include    "tp_task.h"

#include    "r_riic_drv_api.h"

/******************************************************************************
 Macro definitions
******************************************************************************/
#define LCD_EEPROM_ADDRESS (0xAA)

/******************************************************************************
 Enumerated Types
******************************************************************************/


/******************************************************************************
 Structures
******************************************************************************/


/******************************************************************************
 Global Vaiables
******************************************************************************/
int32_t   g_stp_sem_id_tsk;
int32_t   g_eep_sem_id_acc;
os_task_t *gp_os_task;

/*******************************************************************************
 Private global variables and functions
*******************************************************************************/
SCOPE_STATIC void tp_callback_lcd_event( void *pvData );

SCOPE_STATIC uint32_t unTpEvtMsg;
/*!< Touch panel all event lock state */
SCOPE_STATIC tpevt_lock_state_t TpEvtLockInf ;
/*! Touch panel event entry data
 @n The length of the array is not too long, 
 so data chain structure is not applied. */
SCOPE_STATIC tpevt_entry_t tpevt_entry[ TPEVT_ENTRY_MAX ] ;

/*! Touch Panel screen width */
SCOPE_STATIC int_t    ScreenWidth ;
/*! Touch Panel screen height */
SCOPE_STATIC int_t    ScreenHeight ;

SCOPE_STATIC int_t    nEvtEntryId;

SCOPE_STATIC int_t    sRskRftAppBoardVersion;

/**************************************************************************//**
* Function Name: tp_init
* @brief         Initialize TouchPanel driver.
*
*                Description:<br>
*                
* @param         None. 
* @retval        None.
******************************************************************************/
void tp_init( void )
{
#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
    int_t       nret = 0;
    int_t       nriic_handle;
    st_r_drv_riic_transfer_t eep_i2c_info;
    uint8_t     untft_ver_info;
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
    
    r_lcd_init();

    memset( &tpevt_entry, 0, sizeof tpevt_entry ) ;
    nEvtEntryId  = -1;
    TpEvtLockInf = TP_EVT_UNLOCK ;          /** Unlocked */

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)

    /** create synchronous semaphore for access to EEPROM */
    if( R_OS_SemaphoreCreate( &g_eep_sem_id_acc, 1 ) != true)
    {
        DBG_PRINTF_ERR
        ( "[ERROR] semaphore for acces to EEPROM is not created\n" );
        nret = -1;
    }
    
    /** Open RIIC channel for access to EEPROM */
    if( nret >= 0 )
    {
        if( R_OS_SemaphoreWait( &g_eep_sem_id_acc, 0 ) != true)
        {
            DBG_PRINTF_ERR( "[ERROR] semaphore for task is not get\n" );
            nret = -1;
        }
        else
        {
            nret = open(DEVICE_INDENTIFIER "riic3", O_RDWR);
            R_OS_SemaphoreRelease( &g_eep_sem_id_acc );
        }
    
        if( nret == (-1) )
        {
            DBG_PRINTF_ERR
            ( "[ERROR] RIIC driver channel is not opened -- ret:%d\n", nret );
        }
        else
        {
            nriic_handle = nret;
        }
    }
    
    if( nret >= 0 )
    {        
        eep_i2c_info.device_address = LCD_EEPROM_ADDRESS;
        eep_i2c_info.sub_address_type = RIIC_SUB_ADDR_WIDTH_8_BITS;
        eep_i2c_info.sub_address = 0x00;
        eep_i2c_info.number_of_bytes = 1;
        eep_i2c_info.p_data_buffer = &untft_ver_info;
        
        nret = control(nriic_handle, CTL_RIIC_READ, &eep_i2c_info);
        if( nret != DRV_ERROR )
        {
            DBG_PRINTF_DBG( "[OK] Touch panel version information read\n" );
            
            if (0x00 == untft_ver_info)
            {
                /* The read byte is 0x00 - version 1 board */
                sRskRftAppBoardVersion = 1;
            }
            else if (0xFF == untft_ver_info)
            {
                /* The read byte is 0xFF - version 2 board */
                sRskRftAppBoardVersion = 2;
            }
            else
            {
                /* The version is unknown, default to version 1 */
                sRskRftAppBoardVersion = 1;
            }
        }
        else
        {
            DBG_PRINTF_DBG
            ( "[ERROR] EEPROM slave address set is failed -- ret:%d\n", nret );
            /* No EEPROM is fitted - so version 1 board */
            sRskRftAppBoardVersion = 1;
        }
    }
    
    if( R_OS_SemaphoreWait( &g_eep_sem_id_acc, 0 ) != true)
    {
        DBG_PRINTF_ERR( "[ERROR] semaphore for task is not get\n" );
        nret = -1;
    }
    else
    {
        nret = close( nriic_handle );
        R_OS_SemaphoreRelease( &g_eep_sem_id_acc );
        
        R_OS_SemaphoreDelete( &g_eep_sem_id_acc );
    }
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
}


/**************************************************************************//**
* Function Name: tp_open
* @brief         Open TouchPanel driver.
*
*                Description:<br>
*                
* @param         [in]int_t nWidth       : screen width
* @param         [in]int_t nHeight      : screen height
* @param         [in]uint32_t unIrqLv   : IRQ interrupt level
* @param         [in]int16_t nTskPri    : task priority
* @param         [in]uint32_t unTskStk  : task stack size
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_open( const int_t nWidth, const int_t nHeight, const uint32_t unIrqLv, const int16_t nTskPri, const uint32_t unTskStk )
{
    int_t       nret;

    nret            = 0;
    ScreenWidth     = nWidth ;
    ScreenHeight    = nHeight ;
    unTpEvtMsg      = 0;
    uint32_t untsk_stk_tmp = unTskStk;

    /** create synchronous semaphore for task */
    if( nret >= 0 )
    {
        if( R_OS_SemaphoreCreate( &g_stp_sem_id_tsk, 1 ) != true)
        {
            DBG_PRINTF_ERR( "[ERROR] semaphore for task is not created\n" );
            nret = -1;
        }
        if( R_OS_SemaphoreWait( &g_stp_sem_id_tsk, 0 ) != true)
        {
            DBG_PRINTF_ERR( "[ERROR] semaphore for task is not get\n" );
            nret = -1;
        }
    }

    /** create task */
    if( nret >= 0 )
    {
        if( untsk_stk_tmp == R_OS_ABSTRACTION_TINY_STACK_SIZE )
        {
            untsk_stk_tmp = R_OS_ABSTRACTION_SMALL_STACK_SIZE;
        }
        gp_os_task = R_OS_TaskCreate
                    ("Touch", tp_task, NULL, untsk_stk_tmp, nTskPri );
        if(( gp_os_task == NULL )||
            (R_OS_ABSTRACTION_INVALID_HANDLE == gp_os_task))
        {
            DBG_PRINTF_ERR( "[ERROR] task is not created\n" );
            nret = -1;
            R_OS_SemaphoreDelete( &g_stp_sem_id_tsk );
        }
        else
        {
            DBG_PRINTF_LOG( "[PRIORITY] %d\n", nTskPri );
        }
    }

    if( nret >= 0 )
    {
        nret = r_lcd_open( unIrqLv, nTskPri, untsk_stk_tmp );
        if( nret < 0 )
        {
            R_OS_TaskDelete( gp_os_task );
            R_OS_SemaphoreDelete( &g_stp_sem_id_tsk );
        }
    }

    if( nret >= 0 )
    {
        nret = r_lcd_event_entry( LCDEVT_ENTRY_TP, &tp_callback_lcd_event );
        if( nret < 0 )
        {
            DBG_PRINTF_ERR( "[ERROR] LCD event entry is not registered\n" );
            r_lcd_close();
            R_OS_TaskDelete( gp_os_task );
            R_OS_SemaphoreDelete( &g_stp_sem_id_tsk );
        }
        else
        {
            nEvtEntryId = nret;
        }
    }

    return nret;
}


/**************************************************************************//**
* Function Name: tp_close
* @brief         Close TouchPanel driver.
*
*                Description:<br>
*                
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_close( void )
{
    int_t           nret;
    int_t           ni;
    tp_taskstat_t   etask_stat;

    nret = r_lcd_close();

    nret |= r_lcd_event_erase( nEvtEntryId );
    if( nret >= 0 )
    {
        nEvtEntryId = -1;
    }
    else
    {
        DBG_PRINTF_ERR( "[ERROR] LCD event entry is not erased\n" );
    }

    /* erase all events */
    for( ni = 0; ni < TPEVT_ENTRY_MAX; ni ++ )
    {
        nret = tp_event_erase( ni );
        if( nret < 0)
        {
            DBG_PRINTF_ERR( "[ERROR] erase event is not complete(ni=%d)\n",ni );
            break;
        }
    }

    /* stop TP task */
    nret |= tp_send_evt_msg( TP_EVTFLG_EXIT );
    if( nret < 0 )
    {
        DBG_PRINTF_ERR( "[ERROR] exit event message is not sent\n" );
    }
    else
    {
        while( 1 )
        {
            etask_stat = tp_get_task_status();
            if( etask_stat == TP_TASKSTAT_IDLE )
            {
                break;
            }
            R_OS_TaskSleep( 10 );
        }
    }

    R_OS_SemaphoreDelete( &g_stp_sem_id_tsk );

    return nret;
}


/**************************************************************************//**
* Function Name: tp_event_entry
* @brief         Enter callback event to event table.
*
*                Description:<br>
*                
* @param         [in]tpevt_entry_type_t eMode  : event type
* @param         [in]int32_t nPosX             : X-coordinate of LCD area
* @param         [in]int32_t nPosY             : Y-coordinate of LCD area
* @param         [in]int32_t nWidth            : width of LCD area
* @param         [in]int32_t nHeight           : height of LCD area
* @param         [in]void *function            : callback function
* @retval          0..(TPEVT_ENTRY_MAX-1) : event ID.
*                 -1                      : Error occured.
******************************************************************************/
int_t tp_event_entry( const tpevt_entry_type_t eMode, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight, const TpCBFunc function )
{
    int_t ni ;
    tpevt_entry_t *p_entry ;

    p_entry = &tpevt_entry[ 0 ] ;
    for( ni = 0 ; ni < TPEVT_ENTRY_MAX ; ni++)
    {
        if( p_entry->mode == TPEVT_ENTRY_NONE )      /*Serch a no entry space */
        {
            p_entry->mode = eMode ;
            p_entry->st.x = nPosX ;
            p_entry->st.y = nPosY ;
            p_entry->ed.x = nPosX + nWidth ;
            p_entry->ed.y = nPosY + nHeight ;
            p_entry->function = function ;
            break ;
        }

        p_entry++;
    }
    /** Return ERROR ( -1 ), when the event entry fails. */
    return(( ni < TPEVT_ENTRY_MAX ) ? ni : ( -1 )) ;
}


/**************************************************************************//**
* Function Name: tp_event_erase
* @brief         Erase callback event from event table.
*
*                Description:<br>
*                
* @param         [in]int_t nId              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_event_erase( const int_t nid )
{
    int_t ercd = 0;

    if(( nid < 0 ) || ( nid >= TPEVT_ENTRY_MAX ))
    {
        ercd = -1;         /* id error */
    }
    else
    {
        tpevt_entry[ nid ].mode = TPEVT_ENTRY_NONE ;
    }

    return ercd;
}


/**************************************************************************//**
* Function Name: tp_change_event_entry
* @brief         Change Entered callback event.
*
*                Description:<br>
*                
* @param         [in]int_t nid              : event ID
* @param         [in]int32_t nPosX          : X-coordinate of LCD area
* @param         [in]int32_t nPosY          : Y-coordinate of LCD area
* @param         [in]int32_t nWidth         : width of LCD area
* @param         [in]int32_t nHeight        : height of LCD area
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_change_event_entry( const int_t nid, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight )
{
    tpevt_entry_t *p_entry ;
    int_t ercd = 0;

    if(( nid < 0 ) || ( nid >= TPEVT_ENTRY_MAX ))
    {
        ercd = -1;         /* id error */
    }
    else
    {

        p_entry = &tpevt_entry[ nid ] ;

        if( p_entry->mode == TPEVT_ENTRY_NONE )
        {
            ercd = -1;     /* no entry */
        }
        else
        {
            p_entry->st.x = nPosX ;
            p_entry->st.y = nPosY ;
            p_entry->ed.x = nPosX + nWidth ;
            p_entry->ed.y = nPosY + nHeight;
        }
    }

    return ercd;
}


/**************************************************************************//**
* Function Name: tp_event_lock_all
* @brief         Lock all callback event.
*
*                Description:<br>
*                
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_event_lock_all( void )
{
    int_t   nret;
    int_t   ni;

    for( ni = 0; ni < TPEVT_ENTRY_MAX; ni ++ )
    {
        nret = tp_event_lock( ni );
        if( nret < 0 )
        {
            break;
        }
    }

    return nret;
}


/**************************************************************************//**
* Function Name: tp_event_unlockall
* @brief         Unlock all callback event.
*
*                Description:<br>
*                
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_event_unlockall( void )
{
    int_t   nret;
    int_t   ni;

    for( ni = 0; ni < TPEVT_ENTRY_MAX; ni ++ )
    {
        nret = tp_event_unlock( ni );
        if( nret < 0 )
        {
            break;
        }
    }

    return nret;
}


/**************************************************************************//**
* Function Name: tp_event_lock
* @brief         Lock assigned callback event.
*
*                Description:<br>
*                
* @param         [in]int_t nid              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_event_lock( const int_t nid )
{
    int_t ercd = 0;

    if(( nid < 0 ) || ( nid >= TPEVT_ENTRY_MAX ))
    {
        ercd = -1;         /* id error */
    }
    else
    {
        tpevt_entry[ nid ].evtlock = TP_EVT_LOCK ;
    }

    return ercd;
}


/**************************************************************************//**
* Function Name: tp_event_unlock
* @brief         Unlock assigned callback event.
*
*                Description:<br>
*                
* @param         [in]int_t nid              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t tp_event_unlock( const int_t nid )
{
    int_t ercd = 0;

    if(( nid < 0 ) || ( nid >= TPEVT_ENTRY_MAX ))
    {
        ercd = -1;         /* id error */
    }
    else
    {
        tpevt_entry[ nid ].evtlock = TP_EVT_UNLOCK ;
    }

    return ercd;
}


/**************************************************************************//**
* Function Name: tp_get_event_table
* @brief         Get assigned callback event.
*
*                Description:<br>
*                
* @param         [in]int_t nid              : event ID
* @retval        pointer to event.
******************************************************************************/
tpevt_entry_t *tp_get_event_table( const int_t nid )
{
    return &tpevt_entry[ nid ];
}


/**************************************************************************//**
* Function Name: tp_get_event_lock_inf
* @brief         Get state of total callback event.
*
*                Description:<br>
*                
* @param         None.
* @retval        state of total callback event.
******************************************************************************/
tpevt_lock_state_t tp_get_event_lock_inf( void )
{
    return TpEvtLockInf;
}


/**************************************************************************//**
* Function Name: tp_get_screen_size
* @brief         Get screen size.
*
*                Description:<br>
*                
* @param         [out]int_t *pnWidth        : pointer to width value
* @param         [out]int_t *pnHeight       : pointer to height value
* @retval        None.
******************************************************************************/
void tp_get_screen_size( int_t *pnWidth, int_t *pnHeight )
{
    *pnWidth  = ScreenWidth;
    *pnHeight = ScreenHeight;
}

void tp_set_screen_size( int_t nWidth, int_t nHeight )
{
    ScreenWidth = nWidth;
    ScreenHeight = nHeight;
}


/**************************************************************************//**
* Function Name: tp_get_app_board_version
* @brief         Get the version number of the connected TFT App. board.
*
*                Description:<br>
*
* @retval        The version number of the board as an integer.
******************************************************************************/
int_t tp_get_app_board_version()
{
    return sRskRftAppBoardVersion;
}

/**************************************************************************//**
* Function Name: tp_send_evt_msg
* @brief         Send event message to synchronism.
*
*                Description:<br>
*                
* @param         [in]uint32_t unEvtFlg      : event flag
* @retval        = 0 : Operation successfull.
*                < 0 : Error occured.
******************************************************************************/
int32_t tp_send_evt_msg( const uint32_t unEvtFlg )
{
    int32_t     nret;

    nret = 0;

    taskENTER_CRITICAL();
    unTpEvtMsg |= unEvtFlg;
    taskEXIT_CRITICAL();
    R_OS_SemaphoreRelease( &g_stp_sem_id_tsk );

    DBG_PRINTF_DBG( "[SEND EVT] 0x%08X\n", unTpEvtMsg );

    return nret;
}


/**************************************************************************//**
* Function Name: tp_wait_evt_msg
* @brief         Wait event message to synchronism.
*
*                Description:<br>
*                
* @param         None.
* @retval        >= 0 : event flag list.
*                <  0 : Error occured.
******************************************************************************/
int32_t tp_wait_evt_msg( void )
{
    int32_t nret;
    bool_t nnum;

    nnum = R_OS_SemaphoreWait
            ( &g_stp_sem_id_tsk, R_OS_ABSTRACTION_EV_WAIT_INFINITE ); 
    if( nnum == true )
    {
        DBG_PRINTF_DBG( "[WAIT EVT] num=%d\n", nnum );
        nret = unTpEvtMsg;
    }
    else
    {
        DBG_PRINTF_ERR( "[ERROR] message is not got -- ret = %d\n", nnum );
        nret = -1;
    }

    return nret;
}


/**************************************************************************//**
* Function Name: tp_clear_evt_msg
* @brief         Clear assigned event flag.
*
*                Description:<br>
*                
* @param         [in]uint32_t unEvtFlg      : event flag
* @retval        None.
******************************************************************************/
void tp_clear_evt_msg( const uint32_t unEvtFlg )
{
    /* the USB driver number held in the upper bits is cleared together with the flag */
    taskENTER_CRITICAL();
    unTpEvtMsg &= (~(unEvtFlg | TP_DEVADDR_MASK));
    taskEXIT_CRITICAL();
}


/**************************************************************************//**
* Function Name: tp_callback_lcd_event
* @brief         Callback function.
*
*                Description:<br>
*                
* @param         [in]void *pvData           : RIIC: not used (NULL)
*                                               USB : USB driver number of the reporting device
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_callback_lcd_event( void *pvData )
{
    uint32_t    undrvnum;

    undrvnum = (((uint32_t)(uintptr_t)pvData) << TP_DEVADDR_SHIFT) & TP_DEVADDR_MASK;
    tp_send_evt_msg( TP_EVTFLG_PENIRQ | undrvnum );
}


