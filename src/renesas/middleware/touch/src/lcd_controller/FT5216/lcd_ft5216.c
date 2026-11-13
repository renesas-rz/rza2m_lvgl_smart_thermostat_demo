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
* @file         lcd_ft5216.c
* @brief        LCD Driver functions
******************************************************************************/
/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include "r_typedefs.h"

#include    <stdio.h>
#include    <string.h>
#include    <fcntl.h>

#include    "r_os_abstraction_api.h"
#include    "r_typedefs.h"
#include    "lcd_ft5216.h"
#include    "lcd_ft5216_int.h"
#include    "r_riic_drv_api.h"
#include    "r_devlink_wrapper.h"
#include    "r_riic_drv_sc_cfg.h"


/******************************************************************************
 Macro definitions
******************************************************************************/


/******************************************************************************
 Enumerated Types
******************************************************************************/


/******************************************************************************
 Structures
******************************************************************************/


/******************************************************************************
 Global Vaiables
******************************************************************************/
uint32_t g_slcd_sem_idt_sk;
uint32_t g_slcd_sem_id_acc;

/*******************************************************************************
 Private global variables and functions
*******************************************************************************/
SCOPE_STATIC uint32_t unEvtMsg;
SCOPE_STATIC lcdevt_entry_t LcdEvt_Entry[LCDEVT_ENTRY_MAX];
int_t g_hi2c3 = ( -1);

/**************************************************************************//**
* Function Name: lcd_ft5216_open
* @brief         Open LCD driver.
*
*                Description:<br>
*
* @param         [in]uint32_t unIrqLv   : IRQ interrupt level
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_open( const uint32_t unIrqLv, int16_t nTskPri, uint32_t unTskStk )
{
    UNUSED_PARAM(unTskStk);
    UNUSED_PARAM(nTskPri);

    int_t nret;
    int_t error = -1;


    nret = 0;
    memset( &LcdEvt_Entry, 0, sizeof(LcdEvt_Entry));
    unEvtMsg = 0;


    /** create access semaphore for driver */
    if (R_OS_SemaphoreCreate( &g_slcd_sem_id_acc, 1) != true)
    {
        DBG_PRINTF_ERR
        ("[ERROR] semaphore for internal access is not created\n");
        nret = -1;
    }

    /** create synchronous semaphore for task */
    if (nret >= 0)
    {
        if (R_OS_SemaphoreCreate( &g_slcd_sem_idt_sk, 1) != true)
        {
            DBG_PRINTF_ERR("[ERROR] semaphore for task is not created\n");
            nret = -1;
            R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
        }
        if (R_OS_SemaphoreWait( &g_slcd_sem_idt_sk, 0) != true)
        {
            DBG_PRINTF_ERR("[ERROR] semaphore for task is not created\n");
            nret = -1;
            R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
        }
    }

    /* open RIIC */
    if( nret >= 0 )
    {
        nret = open(DEVICE_INDENTIFIER "riic3", O_RDWR);

        if (nret >= 0)
        {
            g_hi2c3 = nret;
        }
        else
        {
            R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
            R_OS_SemaphoreDelete( &g_slcd_sem_idt_sk);
        }
    }

    /* start LCD interrupt */
    if (nret >= 0)
    {
        nret = lcd_ft5216_int_open(unIrqLv);
        if (nret < 0)
        {
            R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
            R_OS_SemaphoreDelete( &g_slcd_sem_idt_sk);
        }
    }

    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_close
* @brief         Close LCD driver.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_close( void )
{
    int_t nret;

    nret = 0;

    /* stop LCD interrupt */
    nret = lcd_ft5216_int_close();

    /** delete access semaphore */
    if (g_slcd_sem_id_acc != 0)
    {
        R_OS_SemaphoreDelete( &g_slcd_sem_id_acc);
        g_slcd_sem_id_acc = 0;
    }
    else
    {
        DBG_PRINTF_ERR
            ("[ERROR] semaphore for internal access is not deleted\n");
        nret = -1;
    }

    /** delete synchronous semaphore */
    if (g_slcd_sem_idt_sk != 0)
    {
        R_OS_SemaphoreDelete( &g_slcd_sem_idt_sk);
        g_slcd_sem_idt_sk = 0;
    }
    else
    {
        DBG_PRINTF_ERR("[ERROR] semaphore for task is not deleted\n");
        nret = -1;
    }

    return nret;
}

/**************************************************************************//**
* Function Name: lcd_ft5216_write_cmd
* @brief         Write command to LCD device via RIIC.
*
*                Description:<br>
*
* @param         [in]uint16_t unDevAddr : device address
* @param         [in]uint8_t uData      : data
* @param         [in]uint32_t unSize    : size of data
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_write_cmd( const uint16_t unDevAddr, const uint8_t uData, const uint32_t unSize )
{
    /* Do nothing and the return value is always 0 
       because write command isn't executed in this utility sample */
    
    int_t nret = 0;

    return nret;
}

/**************************************************************************//**
* Function Name: lcd_ft5216_read_cmd
* @brief         Write command to LCD device and read data via RIIC.
*
*                Description:<br>
*
* @param         [in]uint16_t unDevAddr : device address
* @param         [out]uint8_t *puData   : pointer to receive buffer
* @param         [in]uint32_t unSize    : size of data
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_read_cmd( const uint16_t unDevAddr, uint8_t *puData, const uint32_t unSize )
{
    int_t nret;
    st_r_drv_riic_transfer_t i2c_read;
    i2c_read.device_address = unDevAddr;
    i2c_read.sub_address_type = RIIC_SUB_ADDR_WIDTH_8_BITS;
    i2c_read.sub_address = 2;
    i2c_read.number_of_bytes = unSize;
    i2c_read.p_data_buffer = puData;

    nret = control(g_hi2c3, CTL_RIIC_READ, &i2c_read);

    return nret;
}

/**************************************************************************//**
* Function Name: lcd_ft5216_event_entry
* @brief         Enter callback event to event table.
*
*                Description:<br>
*
* @param         [in]lcdevt_entry_type_t eType : event type
* @param         [in]void *function         : callback function
* @retval         0..(LCDEVT_ENTRY_MAX-1) : event ID.
*                -1                       : Error occured.
******************************************************************************/
int_t lcd_ft5216_event_entry( const lcdevt_entry_type_t eType, const LcdCBFunc function )
{
    int_t ni ;
    lcdevt_entry_t *p_entry ;

    ni = -1;

    if( (eType > LCDEVT_ENTRY_NONE) && (eType <= LCDEVT_ENTRY_MAX) )
    {
        p_entry = &LcdEvt_Entry[ eType-1 ] ;
        if( p_entry->mode == LCDEVT_ENTRY_NONE )
        {
            p_entry->mode     = eType;
            p_entry->function = function ;
            ni                = eType-1;

            /* enable LCD interrupt */
            lcd_ft5216_int_start();
        }
    }
    /** Return ERROR ( -1 ), when the event entry fails. */
    return ni;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_event_erase
* @brief         Erase callback event from event table.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_event_erase( const int_t nId )
{
    int_t ercd = 0;

    if(( nId < 0 ) || ( nId >= LCDEVT_ENTRY_MAX ))
    {
        ercd = -1;         /* id error */
    }
    else
    {
        LcdEvt_Entry[ nId ].mode = LCDEVT_ENTRY_NONE;
    }

    return ercd;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_start_int
* @brief         Enable interrupt of assigned type.
*
*                Description:<br>
*
* @param         [in]lcdevt_entry_type_t eType : interrupt type
* @retval          0 : Operation successfull.
******************************************************************************/
int_t lcd_ft5216_start_int( const lcdevt_entry_type_t eType )
{
    UNUSED_PARAM(eType);
    int_t nret;

    nret = lcd_ft5216_int_start();

    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_get_event_table
* @brief         Get assigned callback event.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @retval        pointer to event.
******************************************************************************/
lcdevt_entry_t *lcd_ft5216_get_event_table( const int_t nId )
{
    return &LcdEvt_Entry[ nId ];
}


/**************************************************************************//**
* Function Name: lcd_ft5216_send_evt_msg
* @brief         Send event message to synchronism.
*
*                Description:<br>
*
* @param         [in]uint32_t unEvtFlg      : event flag
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int32_t lcd_ft5216_send_evt_msg( const uint32_t unEvtFlg )
{
    int32_t nret;

    nret = 0;

    unEvtMsg |= unEvtFlg;
    if (g_slcd_sem_idt_sk == 0)
    {
        DBG_PRINTF_ERR("[ERROR] message is not put -- msg = 0x%0X\n", unEvtFlg);
        unEvtMsg &= (~unEvtFlg);
        nret = -1;
    }
    else
    {
        R_OS_ReleaseSemaphore( &g_slcd_sem_idt_sk);
    }
    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_wait_evt_msg
* @brief         Wait event message to synchronism.
*
*                Description:<br>
*
* @param         None.
* @retval        >= 0 : event flag list.
*                  -1 : Error occured.
******************************************************************************/
int32_t lcd_ft5216_wait_evt_msg( void )
{
    int32_t nret;
    bool_t nnum;

    nnum = 
        R_OS_SemaphoreWait( &g_slcd_sem_idt_sk, 
                            R_OS_ABSTRACTION_EV_WAIT_INFINITE);
    if (nnum == true)
    {
        nret = unEvtMsg;
    }
    else
    {
        DBG_PRINTF_ERR("[ERROR] message is not got -- ret = %d\n", nnum);
        nret = -1;
    }

    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_clear_evt_msg
* @brief         Clear assigned event flag.
*
*                Description:<br>
*
* @param         [in]uint32_t unEvtFlg      : event flag
* @retval        None.
******************************************************************************/
void lcd_ft5216_clear_evt_msg( const uint32_t unEvtFlg )
{
    unEvtMsg &= (~unEvtFlg);
}

