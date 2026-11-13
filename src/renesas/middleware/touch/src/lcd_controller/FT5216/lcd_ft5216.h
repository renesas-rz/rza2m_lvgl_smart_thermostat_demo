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
* @file          lcd_ft5216.h
* @brief         LCD Driver internal hedder
******************************************************************************/
#include    "r_os_abstraction_api.h"
#include    "lcd_controller_if.h"

#ifndef     LCD_FT5216_H
#define     LCD_FT5216_H

/******************************************************************************
Includes <System Includes>, "Project Includes"
******************************************************************************/

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
#define DBG_PRINTF_OT(...)              (printf(__VA_ARGS__))
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
#define DBG_PRINTF_LOG(str, ...)
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
#define     LCDEVT_ENTRY_MAX            (1)


/******************************************************************************
 Enumerated Types
******************************************************************************/
/*! Touch panel event lock state */
typedef enum {
    LCD_EVT_UNLOCK = 0,  /*!< Unlocked */
    LCD_EVT_LOCK         /*!< Locked */
} lcdevt_lock_state_t ;


/******************************************************************************
 Structures
******************************************************************************/
/*! Event entry struct */
typedef struct {
    lcdevt_entry_type_t mode ;      /*!< The type of touch panel event entry */
    LcdCBFunc function;
    lcdevt_lock_state_t evtlock ;   /*!< Event lock state */
} lcdevt_entry_t ;


/******************************************************************************
 Global Vaiables
******************************************************************************/
extern uint32_t   g_slcd_sem_id_acc;


/******************************************************************************
 Function Prototypes
******************************************************************************/

/******************************************************************************
* Function Name: lcd_ft5216_open
* Description  : Open LCD driver.
* Arguments    : unIrqLv - IRQ interrupt level.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_open( const uint32_t unIrqLv, int16_t nTskPri, uint32_t unTskStk );

/******************************************************************************
* Function Name: lcd_ft5216_close
* Description  : Close LCD driver.
* Arguments    : none.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_close( void );

/******************************************************************************
* Function Name: lcd_ft5216_write_cmd
* Description  : Write command to LCD device via RIIC.
* Arguments    : unDevAddr - device address.
*                uData - data
*                unSize - size of data
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_write_cmd( const uint16_t unDevAddr, const uint8_t uData, const uint32_t unSize );

/******************************************************************************
* Function Name: lcd_ft5216_read_cmd
* Description  : Write command to LCD device and read data via RIIC.
* Arguments    : unDevAddr - device address.
*                *puData - data
*                unSize - size of data
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_read_cmd( const uint16_t unDevAddr, uint8_t *puData, const uint32_t unSize );

/******************************************************************************
* Function Name: lcd_ft5216_event_entry
* Description  : Enter callback event to event table.
* Arguments    : eType - event type.
*                *function - callback function
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_event_entry( const lcdevt_entry_type_t eType, const LcdCBFunc function );

/******************************************************************************
* Function Name: lcd_ft5216_event_erase
* Description  : Erase callback event from event table.
* Arguments    : nId - event ID.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int_t        lcd_ft5216_event_erase( const int_t nId );

/******************************************************************************
* Function Name: lcd_ft5216_start_int
* Description  : Get assigned callback event.
* Arguments    : nId - event ID.
* Return Value : pointer - pointer to event
******************************************************************************/
int_t        lcd_ft5216_start_int( const lcdevt_entry_type_t eType );

/******************************************************************************
* Function Name: lcd_ft5216_get_event_table
* Description  : Enable interrupt of assigned type.
* Arguments    : eType - interrupt type.
* Return Value : 0 - Operation successfull, always return
******************************************************************************/
lcdevt_entry_t *lcd_ft5216_get_event_table( const int_t nId );

/******************************************************************************
* Function Name: lcd_ft5216_send_evt_msg
* Description  : Send event message to synchronism.
* Arguments    : unEvtFlg - event flag.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
int32_t      lcd_ft5216_send_evt_msg( const uint32_t unEvtFlg );

/******************************************************************************
* Function Name: lcd_ft5216_wait_evt_msg
* Description  : Wait event message to synchronism.
* Arguments    : none.
* Return Value : 0 - event flag list
*                -1 - Error occured
******************************************************************************/
int32_t      lcd_ft5216_wait_evt_msg( void );

/******************************************************************************
* Function Name: lcd_ft5216_clear_evt_msg
* Description  : Clear assigned event flag.
* Arguments    : unEvtFlg - event flag.
* Return Value : none
******************************************************************************/
void         lcd_ft5216_clear_evt_msg( const uint32_t unEvtFlg );

#if defined(__cplusplus)
}
#endif

#endif      /* LCD_FT5216_H */
