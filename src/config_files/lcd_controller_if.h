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
 * @headerfile     lcd_controller_if.h
 * @brief          LCD Controller API file for communicating with panel 
 *                 registers. 
 * @version        1.00
 * @date           27.06.2018
 * H/W Platform    RZ/A1LU
 *****************************************************************************/
/*****************************************************************************
 * History      : DD.MM.YYYY Ver. Description
 *              : 21.12.2016 1.00 First Release
 *****************************************************************************/
/* Multiple inclusion prevention macro */
#ifndef     LCD_CONTROLLER_IF_H
#define     LCD_CONTROLLER_IF_H
/**************************************************************************//**
 * @ingroup R_SW_PKG_93_TOUCH_API Touch
 * @defgroup R_SW_PKG_93_TOUCH_LCD LCD Controller
 * @brief Touch Panel LCD Controller 
 * 
 * @anchor R_SW_PKG_93_LCD_CONTROLLER_API_SUMMARY
 * @par Summary
 *
 * This is a LCD Controller middleware module, which allows the user to write 
 * to and read from the registers of the LCD Display. 
 *
 * This  module is specific to the LCD panel, but not the device or board.
 *
 * @anchor R_SW_PKG_93_LCD_CONTROLLER_API_INSTANCES
 * @par Known Implementations:
 * This middleware is used in the RZA1LU Software Package.
 * @see RENESAS_APPLICATION_SOFTWARE_PACKAGE
 * @see RENESAS_OS_ABSTRACTION  Renesas OS Abstraction interface
 * @{
 *****************************************************************************/

/******************************************************************************
Includes <System Includes>, "Project Includes"
******************************************************************************/
#if defined(__cplusplus)
extern "C" {
#endif

#include "r_typedefs.h"
#include "lcd_panel.h"

typedef void (*LcdCBFunc) (void*);
/******************************************************************************
Macro definitions
******************************************************************************/
/*! LCD slave address */
#define LCD_SLAVE_ADDRESS    (0x70 << 0)

/*! Pointing device behind the LCD controller interface.
 *  The device is selected by the panel selected in lcd_panel.h (LCD_PANEL):
 *    LCD_PANEL_DVI (HDMI/DVI monitor)  -> USB HID (USB mouse / USB touch monitor)
 *    LCD_PANEL_RSK (and LVDS)          -> RIIC touch controller on the LCD board (FT5216) */
#define LCD_CONTROLL_RIIC    (0)
#define LCD_CONTROLL_USB     (1)

#if (LCD_PANEL == LCD_PANEL_DVI)
#define LCD_CONTROLL_DEV     (LCD_CONTROLL_USB)
#else
#define LCD_CONTROLL_DEV     (LCD_CONTROLL_RIIC)
#endif

/******************************************************************************
 Enumerated Types
******************************************************************************/
/*! The type of touch panel event entry */
typedef enum {
    LCDEVT_ENTRY_NONE = 0x0000,      /*!< None */
    LCDEVT_ENTRY_TP   = 0x0001,      /*!< None */

    LCDEVT_ENTRY_ALL  = 0x0002       /*!< All */
} lcdevt_entry_type_t ;


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
* Function Name: r_lcd_init
* Description  : Sets the LCD board initialization counter (nLcdInitCnt) to 0.
* Arguments    : none
* Return Value : none 
******************************************************************************/
extern void  r_lcd_init( void );

/******************************************************************************
* Function Name: r_lcd_open
* Description  : Opens a communication environment with the LCD board.
*                This function enables the user to perform multiple open 
*                operations.
* Arguments    : unIrqLv - IRQ interrupt priority (0 to 31).
*                          Sets the GIC interrupt priority.
*              : nTskPri - Task Priority.
*                          Sets the value of osPriority type.
*              : unIrqLv - Not Used.
* Return Value : 0 - Normal end
*               -1 - Open error 
******************************************************************************/
extern int_t r_lcd_open( const uint32_t unIrqLv, const int16_t nTskPri, const uint32_t unTskStk );

/******************************************************************************
* Function Name: r_lcd_close
* Description  : Closes a communication environment with the LCD board.
*                When LCD_Open is used to perform multiple open operations, 
*                this function must be called the same number of times.
* Arguments    : none
* Return Value : 0 - Operation successfull.
*                -1 - Error occured. 
******************************************************************************/
extern int_t r_lcd_close( void );

/******************************************************************************
* Function Name: r_lcd_write_cmd
* Description  : Sends data to the LCD board via the RIIC.
* Arguments    : unDevAddr - LCD Device Address.
*              : uCmd - Not Used.
*              : uData - Send Data.
*              : unSize - Send Data Length.
* Return Value : 0 - normal end
*                -1 - data send processing error
******************************************************************************/
extern uint8_t r_lcd_write_cmd( const uint16_t unDevAddr, const uint8_t uCmd, const uint8_t uData, const uint32_t unSize );

/******************************************************************************
* Function Name: r_lcd_read_cmd
* Description  : Receives data from the LCD board via the RIIC.
* Arguments    : unDevAddr - LCD Device Address.
*              : uCmd - Not Used.
*              : *puData - Receive data buffer pointer.
*              : unSize - Receive Data Length.
* Return Value : 0 - normal end
*              -1 - data send processing error
******************************************************************************/
extern uint8_t r_lcd_read_cmd( const uint16_t unDevAddr, const uint8_t uCmd, uint8_t *puData, const uint32_t unSize );

/******************************************************************************
* Function Name: r_lcd_event_entry
* Description  : Registers an LCD board event.
* Arguments    : eType - Specified Interrupt type.
*              : function - Call-back function.
* Return Value : 0 - (LCDEVT_ENTRY_MAX-1) registration value
*                -1 - Event registration failure
******************************************************************************/
extern int_t r_lcd_event_entry( const lcdevt_entry_type_t eType, const LcdCBFunc function );

/******************************************************************************
* Function Name: r_lcd_event_erase
* Description  : Removes an LCD board event.
* Arguments    : nId - Event ID.
* Return Value : 0 - Operation successfull
*                -1 - Error occured
******************************************************************************/
extern int_t r_lcd_event_erase( const int_t nId );

/******************************************************************************
* Function Name: r_lcd_start_int
* Description  : Removes masking of specified interrupt type.
* Arguments    : eType - Not Used.
* Return Value : 0 - event successfully removed
*                -1 - event removal failure
******************************************************************************/
extern int_t r_lcd_start_int( const lcdevt_entry_type_t eType );

/******************************************************************************
* Function Name: r_lcd_restart
* Description  : Reset LCD board.
*                Function not currently supported
* Arguments    : none.
* Return Value : 0 - Always Returned
******************************************************************************/
extern int_t r_lcd_restart( void );

/******************************************************************************
* Function Name: r_lcd_read_version
* Description  : Read LCD FW version.
*                Function not currently supported
* Arguments    : *puData - pointer to receive buffer.
* Return Value : 0 - Always Returned
******************************************************************************/
extern void  r_lcd_read_version( uint8_t *puData );

/******************************************************************************
* Function Name: r_lcd_set_black_light
* Description  : Set bright level of backlight.
*                Function not currently supported
* Arguments    : uLevel - bright level.
* Return Value : none
******************************************************************************/
extern void  r_lcd_set_black_light( const uint8_t uLevel );

/******************************************************************************
* Function Name: r_lcd_set_buzzer
* Description  : Set scale of buzzer.
*                Function not currently supported
* Arguments    : uScale - scale.
* Return Value : none
******************************************************************************/
extern void  r_lcd_set_buzzer( const uint8_t uScale );

#if defined(__cplusplus)
}
#endif


#endif      /* LCD_CONTROLLER_IF_H */
/**************************************************************************//**
 * @} (end addtogroup)
 *****************************************************************************/
