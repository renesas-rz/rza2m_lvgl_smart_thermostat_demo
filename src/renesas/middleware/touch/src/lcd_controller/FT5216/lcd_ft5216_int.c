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
* @file         lcd_ft5216_int.c
* @brief        LCD Driver interrupt functions
******************************************************************************/
/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include    <stdio.h>
#include    <string.h>

#include    "r_os_abstraction_api.h"
#include    "r_typedefs.h"
#include    "iodefine.h"

#include    "lcd_ft5216.h"
#include    "lcd_ft5216_int.h"


/******************************************************************************
 Macro definitions
******************************************************************************/


/******************************************************************************
 Enumerated Types
******************************************************************************/
/* Interrupt level */
typedef enum
{
    LCD_INT_LEVEL = 0,   /* Level interrupt */
    LCD_INT_EDGE  = 1    /* Edge interrupt */
} lcd_int_lv_t;


/******************************************************************************
 Structures
******************************************************************************/
typedef struct {
    uint32_t    untotal;
    uint32_t    untouch;
    uint32_t    unkey1;
    uint32_t    unkey2;
    uint32_t    unkey3;
    uint32_t    unerr;
} lcd_int_cnt_t;


/******************************************************************************
 Global Vaiables
******************************************************************************/


/*******************************************************************************
 Private global variables and functions
*******************************************************************************/
SCOPE_STATIC void lvgl_ft5216_int_hdl(const uint32_t int_sense);

SCOPE_STATIC void lcd_ft5216_int_hdl( void );

SCOPE_STATIC void LCD_Ft5216_Int_SetPort( void );

SCOPE_STATIC lcd_int_cnt_t g_int_cnt;

uint32_t  g_irq_touch_semaphore = 0;

/**************************************************************************//**
* Function Name: lcd_ft5216_int_open
* @brief         Open LCD interrupt.
*
*                Description:<br>
*
* @param         [in]uint32_t unIrqLv   : IRQ interrupt level
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_int_open( const uint32_t unIrqLv )
{
    int_t      nret;
    int32_t    unint_ret;

    DBG_PRINTF_DBG( "[FUNC] LCD_Ft5x06_Int_Open()\n" );

    nret = 0;

    memset( &g_int_cnt, 0, sizeof(lcd_int_cnt_t) );

    /* disable LCD interrupt */
    R_INTC_Disable( LCD_FT5216_INT_NUM );

    /** define interrupt handler */
    unint_ret = R_INTC_RegistIntFunc( LCD_FT5216_INT_NUM, &lcd_ft5216_int_hdl );
    if( unint_ret != 0 )
    {
        DBG_PRINTF_ERR( "[ERROR] interrupt handler is not registed\n" );
        nret = -1;
    }
    else
    {
        /* set interrupt level */
        R_IRQ_SetSense( INTC_IRQ3, IRQ_SENSE_LEVEL );

        /* set interrupt priority */
        R_INTC_SetPriority( LCD_FT5216_INT_NUM, (uint8_t)unIrqLv );
    }

    return nret;
}

int_t lcd_ft5216_lvgl_int_open( const uint32_t unIrqLv )
{
    int_t      nret;
    int32_t    unint_ret;

    R_OS_SemaphoreCreate(&g_irq_touch_semaphore,1);

    /* Ensure that touchpad_read has semaphore un-available
     * until LVDL_Ft5216_Int_Hdl isr is called  */
    R_OS_SemaphoreWait(&g_irq_touch_semaphore, 0);


    /* disable LCD interrupt */
    R_INTC_Disable( LCD_FT5216_INT_NUM );

    /** define interrupt handler */
    unint_ret = R_INTC_RegistIntFunc( LCD_FT5216_INT_NUM, &lvgl_ft5216_int_hdl );

    if( unint_ret != 0 )
    {
        DBG_PRINTF_ERR( "[ERROR] interrupt handler is not registed\n" );
        nret = -1;
    }
    else
    {
        /* set interrupt level */
        R_IRQ_SetSense( INTC_IRQ3, IRQ_SENSE_LEVEL );

        /* set interrupt priority */
        R_INTC_SetPriority( LCD_FT5216_INT_NUM, (uint8_t)unIrqLv );
    }

    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_int_close
* @brief         Close LCD interrupt.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t lcd_ft5216_int_close( void )
{
    int_t       nret;

    nret = 0;

    /* disable LCD interrupt */
    R_INTC_Disable( LCD_FT5216_INT_NUM );


    return nret;
}


/**************************************************************************//**
* Function Name: lcd_ft5216_int_start
* @brief         Enable interrupt of assigned type.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
******************************************************************************/
int_t lcd_ft5216_int_start( void )
{
    /* enable LCD interrupt */
    R_INTC_Enable( LCD_FT5216_INT_NUM );

    return 0;
}


/******************************************************************************
* Function Name: lcd_ft5216_int_hdl
* @brief         Interrupt handler function.
*
*                Description:<br>
*
* @param         None.
* @retval        None.
******************************************************************************/
SCOPE_STATIC void lcd_ft5216_int_hdl( void )
{
    lcdevt_entry_t *p_evt;

    /* disable LCD interrupt */
    R_INTC_Disable( LCD_FT5216_INT_NUM );

    g_int_cnt.untotal ++;

    /* send interrupt event to LCD_Task */
    p_evt = lcd_ft5216_get_event_table( 0 );
    if( p_evt->mode == LCDEVT_ENTRY_TP )
    {
        /* Casting a callback function with no arguments */
        p_evt->function( (void*)NULL );
    }
}


SCOPE_STATIC void lvgl_ft5216_int_hdl(const uint32_t int_sense)
{
    UNUSED_PARAM(int_sense);

    /* disable LCD interrupt */
    R_INTC_Disable( LCD_FT5216_INT_NUM );

    /* Release semaphore allowing touchpad_is_pressed() to read point(s) */
    R_OS_SemaphoreRelease( &g_irq_touch_semaphore );
}

