/*
This software is subject to the license described in the license.txt file included with this software distribution. 
You may not use this file except in compliance with this license. 
Copyright � Dynastream Innovations Inc. 2012
All rights reserved.
*/

/**@file
 * @brief ANT HRM TX profile device simulator configuration, setup and main processing loop.
 * This file is based on implementation originally made by Dynastream Innovations Inc. - June 2012
 *
 * @defgroup ant_hrm_tx_user_controlled_computed_heart_rate ANT HRM TX example - user controlled computed heart rate
 * @{
 * @ingroup nrf_ant_hrm
 *
 */

#include "main_hrm_tx.h"
#include <stdint.h>
#include <stdio.h>
#include "hrm_tx.h"
#include "nrf_soc.h"
#include "ant_interface.h"
#include "ant_parameters.h"
#include "app_error.h"
#include "app_timer.h"
#include "app_button.h"
#include "app_gpiote.h"
#include "boards.h"
#include "nordic_common.h"

#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32u                                       /**< Minimum size of ANT event message buffer. */

#define HRMTX_DEVICE_TYPE             0x78                                      /**< Channel ID. */
#define HRMTX_MSG_PERIOD              0x1F86u                                   /**< Decimal 8070 (4.06Hz). */
#define HRMTX_NETWORK_KEY             {0, 0, 0, 0, 0, 0, 0, 0}                  /**< The default network key used. */

#define APP_TIMER_PRESCALER           0                                         /**< Value of the RTC1 PRESCALER register. */
#define APP_TIMER_MAX_TIMERS          1u                                        /**< Maximum number of simultaneously created timers. */
#define APP_TIMER_OP_QUEUE_SIZE       2u                                        /**< Size of timer operation queues. */

#define APP_GPIOTE_MAX_USERS          1u                                        /**< Maximum number of users of the GPIOTE handler. */

#define BUTTON_DETECTION_DELAY        APP_TIMER_TICKS(50u, APP_TIMER_PRESCALER) /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

static const uint8_t  m_network_key[] = HRMTX_NETWORK_KEY;                      /**< ANT PLUS network key. */
static volatile bool  m_is_ant_event_received;                                  /**< Flag value to determine is ANT stack interrupt received. */

 
/**@brief Function for setting up the ANT module for TX broadcast.
 */
static __INLINE void ant_channel_tx_broadcast_setup(void)
{
    printf("+ant_channel_tx_broadcast_setup\n");   

    uint32_t err_code = sd_ant_network_address_set(ANTPLUS_NETWORK_NUMBER, (uint8_t*)m_network_key);
    APP_ERROR_CHECK(err_code);
  
    err_code = hrm_tx_open();
    APP_ERROR_CHECK(err_code);    
  
    err_code = sd_ant_channel_id_set(HRMTX_ANT_CHANNEL, 
                                     HRM_DEVICE_NUMBER, 
                                     HRMTX_DEVICE_TYPE, 
                                     HRM_TRANSMISSION_TYPE);
    APP_ERROR_CHECK(err_code);
  
    err_code = sd_ant_channel_radio_freq_set(HRMTX_ANT_CHANNEL, ANTPLUS_RF_FREQ);
    APP_ERROR_CHECK(err_code);
  
    err_code = sd_ant_channel_period_set(HRMTX_ANT_CHANNEL, HRMTX_MSG_PERIOD);
    APP_ERROR_CHECK(err_code);
  
    err_code = sd_ant_channel_open(HRMTX_ANT_CHANNEL);
    APP_ERROR_CHECK(err_code);

    printf("-ant_channel_tx_broadcast_setup\n");   
}


/**@brief Function for processing application specific events.
 */
static __INLINE void application_event_process(void)
{
    uint8_t  event;
    uint8_t  ant_channel;  
    uint8_t  event_message_buffer[ANT_EVENT_MSG_BUFFER_MIN_SIZE];     
    uint32_t err_code;        
    
    // Extract and process all pending ANT events.
    do
    {
        err_code = sd_ant_event_get(&ant_channel, &event, event_message_buffer);        
        if (err_code == NRF_SUCCESS)
        {
            err_code = hrm_tx_channel_event_handle(event);         
            APP_ERROR_CHECK(err_code);
        }
    } 
    while (err_code == NRF_SUCCESS);
}


/**@brief Function for handling button events.
 *
 * @param[in] pin_no The pin number of the button pressed.
 */
void button_event_handler(uint8_t pin_no, uint8_t button_action)
{  
    uint32_t err_code;
    
    if (button_action == APP_BUTTON_PUSH)
    {
        switch (pin_no)
        {
            case BUTTON_0:
                // Increase heart rate measurement.
                err_code = hrm_tx_heart_rate_increment();
                APP_ERROR_CHECK(err_code);            
                break;
                
            case BUTTON_1:
                // Decrease heart rate measurement.
                err_code = hrm_tx_heart_rate_decrement();
                APP_ERROR_CHECK(err_code);
                break;
                
            default:
                APP_ERROR_HANDLER(pin_no);
                break;
        }
    }
}


/**@brief Function for handling protocol stack IRQ.
 *
 * Interrupt is generated by the ANT stack upon sending an event to the application. 
 */
void SD_EVT_IRQHandler(void)
{
    m_is_ant_event_received = true;
}


void main_hrm_tx_run(void)
{
    printf("+main_hrmtx\n");
  
    ant_channel_tx_broadcast_setup();   
  
    // Initialize timer module.
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);
  
    // Initialize GPIOTE module.  
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);
  
    // Initialize and enable button handler module. 
    static app_button_cfg_t buttons[] =
    {
        {BUTTON_0, false, BUTTON_PULL, button_event_handler},
        {BUTTON_1, false, BUTTON_PULL, button_event_handler}  
    };    
    APP_BUTTON_INIT(buttons, sizeof(buttons) / sizeof(buttons[0]), BUTTON_DETECTION_DELAY, false);    
    uint32_t err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
      
    // Main event processing loop. The application sleeps until an interrupt is received. Upon 
    // wake-up it processes all application events that require processing and then returns to sleep 
    // mode. 
    m_is_ant_event_received = false;    
    for (;;)
    {
        err_code = sd_app_evt_wait();  
        APP_ERROR_CHECK(err_code);
    
        if (m_is_ant_event_received)  
        {
            m_is_ant_event_received = false;
            application_event_process();      
        }
    }
} 

/**
 *@}
 **/
