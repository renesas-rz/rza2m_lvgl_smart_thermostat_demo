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
* @file         tp_if.c
* @brief        TouchPanel Driver API functions
******************************************************************************/

/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include    "r_typedefs.h"

#include    "tp.h"


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


/*******************************************************************************
 Private global variables and functions
*******************************************************************************/


/**************************************************************************//**
* Function Name: touchpanel_init
* @brief         Initialize TouchPanel driver.
*
*                Description:<br>
*
* @param         None.
* @retval        None.
******************************************************************************/
void touchpanel_init( void )
{
    tp_init();
}


/**************************************************************************//**
* Function Name: touchpanel_open
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
int_t touchpanel_open( const int_t nWidth, const int_t nHeight, const uint32_t unIrqLv, const int16_t nTskPri, const uint32_t unTskStk )
{
    int_t       nret;

    nret = tp_open( nWidth, nHeight, unIrqLv, nTskPri, unTskStk );

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_close
* @brief         Close TouchPanel driver.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_close( void )
{
    int_t       nret;

    nret = tp_close();

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_entry
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
int_t touchpanel_event_entry( const tpevt_entry_type_t eMode, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight, const TpCBFunc function )
{
    int_t       nret;

    nret = tp_event_entry( eMode, nPosX, nPosY, nWidth, nHeight, function );

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_erase
* @brief         Erase callback event from event table.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_event_erase( const int_t nId )
{
    int_t       nret;

    nret = tp_event_erase( nId );

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_change_event_entry
* @brief         Change Entered callback event.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @param         [in]int32_t nPosX          : X-coordinate of LCD area
* @param         [in]int32_t nPosY          : Y-coordinate of LCD area
* @param         [in]int32_t nWidth         : width of LCD area
* @param         [in]int32_t nHeight        : height of LCD area
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_change_event_entry( const int_t nId, const int32_t nPosX, const int32_t nPosY, const int32_t nWidth, const int32_t nHeight )
{
    int_t       nret;

    nret = tp_change_event_entry( nId, nPosX, nPosY, nWidth, nHeight );

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_lock_all
* @brief         Lock all callback event.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_event_lock_all( void )
{
    int_t       nret;

    nret = tp_event_lock_all();

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_unlock_all
* @brief         Unlock all callback event.
*
*                Description:<br>
*
* @param         None.
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_event_unlock_all( void )
{
    int_t       nret;

    nret = tp_event_unlockall();

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_lock
* @brief         Lock assigned callback event.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_event_lock( const int_t nId )
{
    int_t       nret;

    nret = tp_event_lock( nId );

    return nret;
}


/**************************************************************************//**
* Function Name: touchpanel_event_unlock
* @brief         Unlock assigned callback event.
*
*                Description:<br>
*
* @param         [in]int_t nId              : event ID
* @retval          0 : Operation successfull.
*                 -1 : Error occured.
******************************************************************************/
int_t touchpanel_event_unlock( const int_t nId )
{
    int_t       nret;

    nret = tp_event_unlock( nId );

    return nret;
}


