/*
 * r_lvgl_touch_port.c
 *
 *  Created on: 23 Oct 2026
 *      Author: b3800117
 *
 *  LVGL pointer input device of the HMI. The device that is behind it follows the selected
 *  panel (LCD_PANEL in lcd_panel.h, LCD_CONTROLL_DEV in lcd_controller_if.h):
 *    LCD_CONTROLL_RIIC : FT5216 touch controller of the RSK LCD (LCD_PANEL_RSK)
 *    LCD_CONTROLL_USB  : USB mouse / USB touch monitor on HDMI (LCD_PANEL_DVI)
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "r_typedefs.h"
#include "r_cache_lld_rza2m.h"
#include "r_intc_lld_rza2m.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

#include "lvgl.h"
#include "rm_lvgl_port.h"

#include "lcd_panel.h"
#include "src/display/lv_display_private.h"

#include "tp.h"
#include "tp_if.h"
#include "lcd_controller_if.h"

#include "r_os_abstraction_api.h"
#include "r_devlink_wrapper.h"
#include <fcntl.h>
#include <unistd.h>


#define TP_INT_PRI              (30uL)
#define TP_TSK_PRI              (R_OS_TASK_MAIN_TASK_PRI+1)

#define GRAPHICS_LAYER_WIDTH    (LCD_CH0_DISP_HW)
#define GRAPHICS_LAYER_HEIGHT   (LCD_CH0_DISP_VW)

#define LCD_FT5216_INT_NUM (INTC_ID_IRQ_IRQ3)

#define TOUCH_POINTS_DETECT_REG_CFG     (2u)

/*! Threshold for determining the "Move" event, pixel^2 */
#define     TPEVT_MOVE_MINTHD   (2UL)
#define     TPEVT_PRESS_THD     (1000)

#define TOUCH_POINT_0_CFG (0)

#ifdef  __DEBUG
#define SCOPE_STATIC
#else   /* __DEBUG */
#define SCOPE_STATIC    static
#endif  /* __DEBUG */

#if (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC)

/*! Coordinate structure */
typedef struct {
    int32_t x ;        /*!< x-coordinate [pixel] */
    int32_t y ;        /*!< y-coordinate [pixel] */
} TPEVT_COORDINATES ;


extern int_t lcd_ft5216_lvgl_int_open( const uint32_t unIrqLv );
extern int_t lcd_ft5216_int_start( void );
extern void  tp_set_screen_size( int_t nWidth, int_t nHeight );

extern int_t g_hi2c3;

extern uint32_t  g_irq_touch_semaphore;

void touchpad_read (lv_indev_t * indev_drv, lv_indev_data_t * data);

uint8_t             auread_buf[0x20];
tp_touch_event_t    stouch_evt;
tpevt_coordinates_t slast_coords[TP_TOUCHNUM_MAX];
int_t               npen_status[TP_TOUCHNUM_MAX];

static bool  touch_pressed = false;

SCOPE_STATIC void  tp_set_finger_info( tp_touch_finger_t *psTouch, tpevt_coordinates_t *psLastCoords, int_t *pnPenStatus, const uint8_t* const puReadBuf, const int_t nFinger );
SCOPE_STATIC void  tp_line_up_pos( const int16_t nXin, const int16_t nYin, int32_t *pnXout, int32_t *pnYout );
SCOPE_STATIC int_t tp_event_handle( tpevt_entry_type_t *eEvent, const int32_t* const nPosX, const int32_t* const nPosY, tpevt_coordinates_t *psLastCoords );
SCOPE_STATIC int_t tp_check_diff_min( const tpevt_coordinates_t* const psPox_1, const tpevt_coordinates_t* const psPox_2, const uint32_t unThreshold );
SCOPE_STATIC int_t tp_direct_srch_cb( tp_touch_event_t *pstouchevt );

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed (void)
{

    touch_pressed = R_OS_SemaphoreWait(&g_irq_touch_semaphore, 1);

    return touch_pressed;
}

void touchpad_read (lv_indev_t * indev_drv, lv_indev_data_t * data)
{
	UNUSED_PARAM(indev_drv);
    UNUSED_PARAM(data);

    static int ni = 0;
    static int ntpd = 0;

    memset( &stouch_evt, 0, sizeof(tp_touch_event_t) );
    memset( &slast_coords, 0, sizeof(tpevt_coordinates_t) * TP_TOUCHNUM_MAX);
    memset( npen_status, 0, sizeof(TP_TOUCHNUM_MAX) );


    if (touchpad_is_pressed())
    {
        int_t nret = DRV_ERROR;

        /* get touch information via RIIC */
        nret = r_lcd_read_cmd(LCD_SLAVE_ADDRESS, 0, auread_buf, 9);

        if(DRV_SUCCESS != nret)
        {
//            DBG_PRINTF_ERR( "[ERROR] touch status is not got\n" );
            r_lcd_restart();
        }
        else
        {
            ntpd = auread_buf[TOUCH_POINTS_DETECT_REG_CFG];

             /* make touch information for output */
             for( ni = 0; ni < TP_TOUCHNUM_MAX; ni++ )
             {
                 tp_set_finger_info( &stouch_evt.sfinger[ni],
                                     &slast_coords[ni],
                                     &npen_status[ni],
                                     auread_buf,
                                     ni );
             }

             if (ntpd > 0)
             {
                 if(1 == npen_status[0])
                 {
                     data->state = LV_INDEV_STATE_PRESSED;

                     /*Set the last pressed coordinates*/
#if 0
					 data->point.x = (int32_t) (GRAPHICS_LAYER_WIDTH  - slast_coords[TOUCH_POINT_0_CFG].x);
					 data->point.y = (int32_t) (GRAPHICS_LAYER_HEIGHT - slast_coords[TOUCH_POINT_0_CFG].y);
#else
					 data->point.x = (slast_coords[TOUCH_POINT_0_CFG].x);
					 data->point.y = (slast_coords[TOUCH_POINT_0_CFG].y);
#endif
                 }
                 else
                 {
                     data->state = LV_INDEV_STATE_RELEASED;
                 }
             }
             else
             {
                 data->state = LV_INDEV_STATE_RELEASED;
             }

         }

        /* enable LCD interrupt */
        R_INTC_Enable( LCD_FT5216_INT_NUM );

    }

}



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
* Function Name: rm_lvgl_touch_port_open
* @brief         Open the FT5216 touch controller of the RSK LCD and register
*                it as LVGL pointer input device.
*
* @param         None.
* @retval        None.
******************************************************************************/
void rm_lvgl_touch_port_open (void)
{
    int_t nret;
    lv_indev_t * p_indev_touchpad;

    /* open RIIC */
    if( g_hi2c3 < 0 )
    {
        nret = open(DEVICE_INDENTIFIER "riic3", O_RDWR);

        if (nret >= 0)
        {
            g_hi2c3 = nret;
        }
    }

    lcd_ft5216_lvgl_int_open(TP_INT_PRI);

    lcd_ft5216_int_start();

    /* Register touch controller */
    p_indev_touchpad = lv_indev_create();
    lv_indev_set_type(p_indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(p_indev_touchpad, touchpad_read);

    tp_set_screen_size(GRAPHICS_LAYER_WIDTH, GRAPHICS_LAYER_HEIGHT);
}

#else  /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */

/* USB mouse / touch monitor, middleware: touch/src/lcd_controller/usb/r_usb_gui_control.c */
extern void usb_gui_init( lv_indev_t *p_indev );
extern void usb_gui_update( const tp_usb_pointer_t *p_ptr );

/**************************************************************************//**
* Function Name: usb_pointer_read
* @brief         LVGL read callback of the USB mouse / touch monitor.
*
*                The USB reports are decoded by the touch panel task of the
*                touch middleware (tp_task_usb.c), this callback takes over
*                the latest state. It also shows / hides the mouse cursor and
*                the "connect USB device" notice.
*
* @param         [in]lv_indev_t *indev          : LVGL input device
* @param         [out]lv_indev_data_t *data     : LVGL input data
* @retval        None.
******************************************************************************/
static void usb_pointer_read (lv_indev_t * indev, lv_indev_data_t * data)
{
    tp_usb_pointer_t    ptr;

    UNUSED_PARAM(indev);

    touchpanel_usb_get_pointer(&ptr);

    usb_gui_update(&ptr);

    data->point.x = ptr.x;
    data->point.y = ptr.y;
    data->state   = (1u == ptr.pressed) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/**************************************************************************//**
* Function Name: rm_lvgl_touch_port_open
* @brief         Start the USB host and the touch panel task of the touch
*                middleware and register the USB mouse / touch monitor as
*                LVGL pointer input device.
*
* @param         None.
* @retval        None.
******************************************************************************/
void rm_lvgl_touch_port_open (void)
{
    lv_indev_t * p_indev_pointer;
    int_t        nret;

    /* LVGL pointer input device with mouse cursor and the USB notice */
    p_indev_pointer = lv_indev_create();
    lv_indev_set_type(p_indev_pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(p_indev_pointer, usb_pointer_read);
    usb_gui_init(p_indev_pointer);

    /* USB host, USB HID driver and touch panel task */
    touchpanel_init();
    nret = touchpanel_open(GRAPHICS_LAYER_WIDTH, GRAPHICS_LAYER_HEIGHT, TP_INT_PRI, TP_TSK_PRI, R_OS_ABSTRACTION_DEFAULT_STACK_SIZE);
    if (nret < 0)
    {
        printf("[ERROR] USB touch panel is not opened\r\n");
    }
}

#endif /* (LCD_CONTROLL_DEV == LCD_CONTROLL_RIIC) */
