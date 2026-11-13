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
* @file         tp_task.c
* @brief        TouchPanel Driver task functions
******************************************************************************/

/*******************************************************************************
Includes <System Includes>, "Project Includes"
*******************************************************************************/
#include    <stdio.h>
#include    <string.h>

#include    "r_typedefs.h"
#include    "iodefine.h"
#include    "tp.h"
#include    "tp_task.h"
#include    "tp_task_usb.h"
#include    "lcd_controller_if.h"
#include    "driver.h"

/******************************************************************************
Macro definitions
******************************************************************************/
    /*! delay time in touch panel task cycle */
#define     TP_TASK_DELAY       (50)

/*! Threshold for determining the "Move" event, pixel^2 */
#define     TPEVT_MOVE_MINTHD   (2UL)
#define     TPEVT_PRESS_THD     (1000)


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
 Private global variables and functions
******************************************************************************/
#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
SCOPE_STATIC void  tp_set_finger_info( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const int_t nFinger );
SCOPE_STATIC void  tp_line_up_pos( const int16_t nXin, const int16_t nYin, int32_t *pnXout, int32_t *pnYout );
SCOPE_STATIC int_t tp_event_handle( tpevt_entry_type_t *eEvent, const int32_t* const nPosX, const int32_t* const nPosY, tpevt_coordinates_t *psLastCoords );
SCOPE_STATIC int_t tp_check_diff_min( const tpevt_coordinates_t* const psPox_1, const tpevt_coordinates_t* const psPox_2, const uint32_t unThreshold );
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
SCOPE_STATIC int_t tp_direct_srch_cb( tp_touch_event_t *pstouchevt );

static tp_taskstat_t  eTpTaskStat;

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
/**************************************************************************//**
* Function Name: tp_task_process_riic
* @brief         Task function.
*
*                Description:<br>
*                
* @param         [in]void const *vpArg  : pointer to argument
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_task_process_riic( void )
{
    int_t               nret;
    int32_t             nevt_val;
    uint8_t             auread_buf[0x20];
    tp_touch_event_t    stouch_evt;
    tpevt_coordinates_t slast_coords[TP_TOUCHNUM_MAX];
    int_t               npen_status[TP_TOUCHNUM_MAX];
    int_t               ni; 
    int_t               nj;
    uint8_t             ntouch_end_flag;
    uint32_t            uicnt;

    eTpTaskStat = TP_TASKSTAT_ACTIVE;
    memset( &stouch_evt, 0, sizeof(tp_touch_event_t) );
    memset( &slast_coords, 0, sizeof(tpevt_coordinates_t) );
    memset( npen_status, 0, sizeof(npen_status) );

    while( 1 )
    {
        nevt_val = tp_wait_evt_msg();
        if( (nevt_val & TP_EVTFLG_EXIT) == TP_EVTFLG_EXIT )
        {
            tp_clear_evt_msg( TP_EVTFLG_EXIT );
            eTpTaskStat = TP_TASKSTAT_IDLE;

            R_OS_TaskDelete(gp_os_task);
            gp_os_task = NULL;

        }
        else if( (nevt_val & TP_EVTFLG_PENIRQ) == TP_EVTFLG_PENIRQ )
        {
            tp_clear_evt_msg( TP_EVTFLG_PENIRQ );
            while( 1 )
            {
                /* get touch information via RIIC */
                nret = r_lcd_read_cmd(LCD_SLAVE_ADDRESS, 0, auread_buf, 9);
                if( nret != DRV_SUCCESS )
                {
                    DBG_PRINTF_ERR( "[ERROR] touch status is not got\n" );
                    r_lcd_restart();
                    break;
                }
                else
                {
                    /* make touch information for output */
                    for( ni = 0; ni < TP_TOUCHNUM_MAX; ni++ )
                    {
                        tp_set_finger_info( &stouch_evt.sfinger[ni], 
                                            &slast_coords[ni], 
                                            &npen_status[ni], 
                                            auread_buf, 
                                            ni );
                    }
                    /* determine unmoved finger's state */
                    for( ni = 0; ni < TP_TOUCHNUM_MAX; ni ++ )
                    {
                        if( stouch_evt.sfinger[ni].estate 
                            == TPEVT_ENTRY_UNKNOWN )
                        {
                            stouch_evt.sfinger[ni].estate = TPEVT_ENTRY_NONE;
                            for( nj = 0; nj < TP_TOUCHNUM_MAX; nj ++ )
                            {
                                if( (stouch_evt.sfinger[nj].estate 
                                        == TPEVT_ENTRY_UP  ) ||
                                    (stouch_evt.sfinger[nj].estate 
                                        == TPEVT_ENTRY_DOWN) ||
                                    (stouch_evt.sfinger[nj].estate 
                                        == TPEVT_ENTRY_MOVE) )
                                {
                                    stouch_evt.sfinger[ni].estate 
                                        = TPEVT_ENTRY_MOVE;
                                    break;
                                }
                            }
                        }
                    }
                }

                /* refer to previous coordinate if state is UP */
                for( ni = 0; ni < TP_TOUCHNUM_MAX; ni ++ )
                {
                    if( stouch_evt.sfinger[ni].estate == TPEVT_ENTRY_UP )
                    {
                        stouch_evt.sfinger[ni].unposx = slast_coords[ni].x;
                        stouch_evt.sfinger[ni].unposy = slast_coords[ni].y;
                    }
                }

                /* execute callback function */
                if( nret >= 0 )
                {
                    if( tp_get_event_lock_inf() == TP_EVT_UNLOCK )
                    {
                        nret = tp_direct_srch_cb( &stouch_evt ) ;
                    }
                }
                
                /* touch flag clear */
                ntouch_end_flag = 0;
                
                /* move to waiting mode when touch is released */
                for(uicnt = 0; uicnt < TP_TOUCHNUM_MAX; uicnt ++)
                {
                    ntouch_end_flag |= npen_status[uicnt];
                }
                
                if( (nret >= 0) && (ntouch_end_flag == 0) )
                {
                    DBG_PRINTF_DBG( "[ESCAPE]\n" );
                    break;
                }
                R_OS_TaskSleep( TP_TASK_DELAY );
            }
            /* enable LCD touch event interrupt */
            nret = r_lcd_start_int( LCDEVT_ENTRY_TP );
            if( nret < 0 )
            {
                DBG_PRINTF_ERR( "[ERROR] TP interrupt is not started\n" );
                r_lcd_restart();
            }
        }
        else
        {
            /* do nothing */
        }
    }
}


#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */


/**************************************************************************//**
* Function Name: tp_task
* @brief         Task function.
*                Runs the touch panel process of the pointing device that is
*                selected with LCD_CONTROLL_DEV (see lcd_controller_if.h):
*                RIIC touch controller (RSK LCD) or USB mouse / touch (HDMI).
*
*                Description:<br>
*                
* @param         [in]void const *vpArg  : pointer to argument
* @retval        None.
******************************************************************************/
void tp_task(void *params)
{
    /* dummt access for argument */
    UNUSED_PARAM(params);

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
    tp_task_process_riic();
#else  /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
    tp_task_process_usb();
#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
}


/**************************************************************************//**
* Function Name: tp_set_task_status
* @brief         Set task status.
*
*                Description:<br>
*                
* @param         [in]tp_taskstat_t estat    : task status
* @retval        None.
******************************************************************************/
void tp_set_task_status( const tp_taskstat_t estat )
{
    eTpTaskStat = estat;
}


/**************************************************************************//**
* Function Name: tp_get_task_status
* @brief         Get task status.
*
*                Description:<br>
*                
* @param         None.
* @retval        Task status.
******************************************************************************/
tp_taskstat_t  tp_get_task_status( void )
{
    return eTpTaskStat;
}


#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)
/**************************************************************************//**
* Function Name: tp_set_finger_info
* @brief         Set touch information to output table.
*
*                Description:<br>
*                
* @param    [out]tp_touch_finger_t *psTouch        : pointer to output table
* @param    [out]tpevt_coordinates_t *psLastCoords : pointer to last coordinate
* @param    [out]int_t *pnPenStatus                : pointer to pending status
* @param    [in]uint8_t *puReadBuf                 : pointer to receive buffer
* @param    [in]int_t nFinger                      : number of finger index
* @retval   None.
******************************************************************************/
SCOPE_STATIC void tp_set_finger_info( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const int_t nFinger )
{
    int16_t                  nxin;
    int16_t                  nyin;
    int32_t                  nxout;
    int32_t                  nyout;
    uint8_t                  unstate;
    int32_t                  npressure;
    npressure = TPEVT_PRESS_THD;

    /* decide touch coordinate */
    nxin = ((puReadBuf[1 + (nFinger * 6)] & 0x7) << 8) 
            + puReadBuf[2 + (nFinger * 6)];
    nyin = ((puReadBuf[3 + (nFinger * 6)] & 0x7) << 8) 
            + puReadBuf[4 + (nFinger * 6)];

    tp_line_up_pos( nxin, nyin, &nxout, &nyout );
    psTouch->unposx = nxout;
    psTouch->unposy = nyout;

    if(puReadBuf[0] > nFinger)
    {
        unstate = 1;
    }
    else{
        unstate = 0;
    }
    
    if( unstate == 0 )
    {
        if( (*pnPenStatus) == 1 )
        {
            /* EVENT : UP */
            *pnPenStatus    = 0;        /* Touch Panel pen status, 0 : up */
            psTouch->estate = TPEVT_ENTRY_UP;
        }
        else
        {
            /* EVENT : NONE */
            psTouch->estate = TPEVT_ENTRY_NONE;
        }
    }
    else
    {
        if( npressure < TPEVT_PRESS_THD )
        {
            /* EVENT : NONE */
            psTouch->estate = TPEVT_ENTRY_NONE;
        }
        else if( (*pnPenStatus) == 0 )
        {
            /* EVENT : DOWN */
            *pnPenStatus    = 1;        /* Touch Panel pen status, 1 : down */
            psTouch->estate = TPEVT_ENTRY_DOWN;
        }
        else
        {
            /* EVENT : MOVE */
            psTouch->estate = TPEVT_ENTRY_MOVE;
        }
        tp_event_handle( &psTouch->estate, &nxout, &nyout, psLastCoords );
    }
}


/**************************************************************************//**
* Function Name: tp_line_up_pos
* @brief         Line up touch position.
*
*                Description:<br>
*                
* @param         [in]int16_t nXin       : before X-coordinate
* @param         [in]int16_t nYin       : before Y-coordinate
* @param         [out]int32_t *pnXout   : pointer to after X-coordinate
* @param         [out]int32_t *pnYout   : pointer to after Y-coordinate
* @retval        None.
******************************************************************************/
SCOPE_STATIC void tp_line_up_pos( const int16_t nXin, const int16_t nYin, int32_t *pnXout, int32_t *pnYout )
{
    int_t   nscr_width;
    int_t   nscr_height;

    tp_get_screen_size( &nscr_width, &nscr_height );

    if (1 == tp_get_app_board_version())
    {
        *pnXout = nXin;
        *pnYout = nYin;
    }
    else if (2 == tp_get_app_board_version())
    {
        *pnXout = nscr_width - nXin;
        *pnYout = nscr_height - nYin;
    }
    else
    {
        *pnXout = nXin;
        *pnYout = nYin;
    }

    DBG_PRINTF_DBG( "[TPDBG] S(%d:%d), I(%d:%d), O(%d:%d)\n", 
                    nscr_width, nscr_height, nXin, nYin, *pnXout, *pnYout );
    if((*pnXout) < 0)
    {
        *pnXout = 0 ;
    }
    else if((*pnXout) >= nscr_width)
    {
        *pnXout = nscr_width - 1 ;
    }
    else
    {
        /* do nothing */
    }

    if((*pnYout) < 0)
    {
        *pnYout = 0 ;
    }
    else if ((*pnYout) >= nscr_height)
    {
        *pnYout = nscr_height - 1 ;
    }
    else
    {
        /* do nothing */
    }
}


/**************************************************************************//**
* Function Name: tp_event_handle
* @brief         Handle event type.
*
*                Description:<br>
*                
* @param    [out]tpevt_entry_type_t *eEvent        : pointer to event variable
* @param    [in]int32_t nPosX                      : X-coordinate
* @param    [in]int32_t nPosY                      : Y-coordinate
* @param    [out]tpevt_coordinates_t *psLastCoords : pointer to last coordinate
* @retval    0 : Operation successfull.
*            -1 : Error occured.
******************************************************************************/
SCOPE_STATIC int_t tp_event_handle( tpevt_entry_type_t *eEvent, const int32_t* const nPosX, const int32_t* const nPosY, tpevt_coordinates_t *psLastCoords )
{
    int_t                    nret ;
    int_t                    nover ;
    tpevt_coordinates_t      coords ;

    nret    = 0;

    coords.x = *nPosX ;
    coords.y = *nPosY ;
    switch( *eEvent )
    {
        case TPEVT_ENTRY_DOWN :
            psLastCoords->x = *nPosX ;
            psLastCoords->y = *nPosY ;
            break ;
        case TPEVT_ENTRY_MOVE :
            nover = tp_check_diff_min( psLastCoords, 
                                        &coords, 
                                        TPEVT_MOVE_MINTHD ) ;
            if( nover == 0 )
            {
                *eEvent = TPEVT_ENTRY_UNKNOWN;
            }
            else
            {
                psLastCoords->x = *nPosX ;
                psLastCoords->y = *nPosY ;
            }
            break ;
        case TPEVT_ENTRY_UP :
            break ;
        default :
            nret = -1;
            break;
    }

    return nret ;
}


/**************************************************************************//**
* Function Name: tp_check_diff_min
* @brief         Check amount of slide.
*
*                Description:<br>
*                
* @param       [in]tpevt_coordinates_t *psPox_1 : pointer to last coordinate
* @param       [in]tpevt_coordinates_t *psPox_2 : pointer to current coordinate
* @param       [in]int32_t unThreshold          : threshold value
* @retval        1 : Move.
*                0 : Not move.
******************************************************************************/
SCOPE_STATIC int_t tp_check_diff_min( const tpevt_coordinates_t* const psPox_1, const tpevt_coordinates_t* const psPox_2, const uint32_t unThreshold )
{
    int32_t     nx;
    int32_t     ny;
    uint32_t    undata;

    nx = psPox_1->x - psPox_2->x;
    ny = psPox_1->y - psPox_2->y;
    undata = ( nx * nx ) + ( ny * ny ) ;

    return(( undata > unThreshold ) ? 1 : 0 ) ;
}


#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */


/**************************************************************************//**
* Function Name: tp_direct_srch_cb
* @brief         Search and call callback function.
*
*                Description:<br>
*                
* @param         [in]tp_touch_event_t   : pointer to touch information
* @retval        Found event number
******************************************************************************/
SCOPE_STATIC int_t tp_direct_srch_cb( tp_touch_event_t *pstouchevt )
{
    int_t           nret;
    int_t           ni; 
    int_t           nj;
    tpevt_entry_t   *p_sentry;

    nret    = 0;

    DBG_PRINTF_LOG( "[TOUCH],%d,%d,%d,%d,%d,%d\n",
                    pstouchevt->sfinger[0].estate, 
                    pstouchevt->sfinger[0].unposx, 
                    pstouchevt->sfinger[0].unposy,
                    pstouchevt->sfinger[1].estate, 
                    pstouchevt->sfinger[1].unposx, 
                    pstouchevt->sfinger[1].unposy );

    p_sentry = tp_get_event_table( 0 );
    for( ni = 0 ; ni < TPEVT_ENTRY_MAX ; ni++ )
    {
        if( p_sentry->evtlock != TP_EVT_UNLOCK )
        {
            continue ;                  /* Locked */
        }
        for( nj = 0; nj < TP_TOUCHNUM_MAX; nj ++ )
        {
            if( (( pstouchevt->sfinger[nj].estate & p_sentry->mode ) != TPEVT_ENTRY_NONE) &&
                (( pstouchevt->sfinger[nj].unposx >= p_sentry->st.x ) && ( pstouchevt->sfinger[nj].unposx < p_sentry->ed.x )) &&
                (( pstouchevt->sfinger[nj].unposy >= p_sentry->st.y ) && ( pstouchevt->sfinger[nj].unposy < p_sentry->ed.y )) )
            {
                /** Event notification callback function */
                p_sentry->function( ni, pstouchevt );
                nret ++;                       /* Exist */
                break;
            }
        }
        p_sentry++;
    }

    return nret ;
}


/**************************************************************************//**
* Function Name: tp_notify_event
* @brief         Notify the registered touch panel event entries.
*
*                Description:<br>
*                Public entry of tp_direct_srch_cb for the USB touch panel process.
*
* @param         [in]tp_touch_event_t   : pointer to touch information
* @retval        Found event number
******************************************************************************/
int_t tp_notify_event( tp_touch_event_t *pstouchevt )
{
    return tp_direct_srch_cb( pstouchevt );
}


