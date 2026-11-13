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
******************************************************************************/
/**************************************************************************//**
* @file          tp_task.h
* @brief         TouchPanel driver internal hedder for task
******************************************************************************/
#ifndef     TP_TASK_H
#define     TP_TASK_H

/******************************************************************************
Includes <System Includes>, "Project Includes"
******************************************************************************/
#include "r_typedefs.h"
#include "tp_if.h"

#if defined(__cplusplus)
extern "C" {
#endif


/******************************************************************************
Macro definitions
******************************************************************************/


/******************************************************************************
 Enumerated Types
******************************************************************************/
typedef enum {
    TP_TASKSTAT_IDLE   = 0,
    TP_TASKSTAT_ACTIVE
} tp_taskstat_t;

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
* Function Name: tp_task
* Description  : Task function.
* Arguments    : *vpArg - pointer to argument.
* Return Value : none 
******************************************************************************/
void            tp_task(void *params);

/******************************************************************************
* Function Name: tp_get_task_status
* Description  : Get task status.
* Arguments    : none.
* Return Value : Task status 
******************************************************************************/
tp_taskstat_t   tp_get_task_status( void );

/******************************************************************************
* Function Name: tp_set_task_status
* Description  : Set task status (used by the USB touch panel process).
* Arguments    : estat - task status.
* Return Value : none
******************************************************************************/
void            tp_set_task_status( const tp_taskstat_t estat );

/******************************************************************************
* Function Name: tp_notify_event
* Description  : Calls the callbacks of the registered touch panel event
*                entries that match the touch information.
* Arguments    : *pstouchevt - pointer to touch information.
* Return Value : Number of notified entries
******************************************************************************/
int_t           tp_notify_event( tp_touch_event_t *pstouchevt );

#if defined(__cplusplus)
}
#endif


#endif      /* TP_TASK_H */
