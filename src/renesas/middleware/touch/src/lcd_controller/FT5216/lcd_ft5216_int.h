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
* @file          lcd_ft5216_int.h
* @brief         LCD Driver internal hedder for interrupt
******************************************************************************/

/******************************************************************************
Includes <System Includes>, "Project Includes"
******************************************************************************/
#include     "r_intc_drv_api.h"

#ifndef     LCD_FT5216_INT_H
#define     LCD_FT5216_INT_H

#if defined(__cplusplus)
extern "C" {
#endif

/******************************************************************************
Macro definitions
******************************************************************************/
#define LCD_FT5216_INT_NUM (INTC_ID_IRQ_IRQ3)

/******************************************************************************
 Enumerated Types
******************************************************************************/

/******************************************************************************
 Structures
******************************************************************************/

/******************************************************************************
 Global Vaiables
******************************************************************************/

/******************************************************************************
 Function Prototypes
******************************************************************************/

/******************************************************************************
* Function Name: lcd_ft5216_int_open
* Description  : Open LCD interrupt.
* Arguments    : none.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t lcd_ft5216_int_open( const uint32_t unIrqLv );

/******************************************************************************
* Function Name: lcd_ft5216_int_close
* Description  : Close LCD interrupt.
* Arguments    : unIrqLv - IRQ interrupt level.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t lcd_ft5216_int_close( void );

/******************************************************************************
* Function Name: lcd_ft5216_int_start
* Description  : Enable interrupt of assigned type.
* Arguments    : none.
* Return Value : 0 - Operation successfull, alwayse return
******************************************************************************/
int_t lcd_ft5216_int_start( void );

#if defined(__cplusplus)
}
#endif

#endif      /* LCD_FT5216_INT_H */
