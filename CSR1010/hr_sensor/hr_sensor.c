/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012
 *  Part of uEnergy SDK 2.0.0
 *  2.0.0.0
 *
 *  FILE
 *      hr_sensor.c
 *
 *  DESCRIPTION
 *      This file defines a simple Heart Rate Sensor application.
 *
 *****************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <main.h>
#include <pio.h>
#include <mem.h>
#include <Sys_events.h>
#include <Sleep.h>
#include <timer.h>
#include <security.h>
#include <gatt.h>
#include <gatt_prim.h>
#include <panic.h>
#include <nvm.h>
#include <buf_utils.h>
#include <debug.h>
#include <ls_app_if.h>      /* Link Supervisor application interface */

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"
#include "heart_rate_service.h"
#include "gap_service.h"
#include "battery_service.h"
#include "hr_sensor.h"
#include "hr_sensor_hw.h"
#include "hr_sensor_gatt.h"
#include "app_gatt_db.h"
#include "nvm_access.h"

/*============================================================================*
 *  Private Definitions
 *============================================================================*/

#define PIO_MAKE_COFFEE (21)  /* PIO connected to the pin to start the process to make coffee short/long */
#define PIO_ONOFF       (23)  /* PIO connected to the pin to turn on/off the coffee machine*/

#define PIO_TRIGGER     (18)  /* PIO connected to the Ultrasonic sensor Trigger */
#define PIO_ECHO        (19)  /* PIO connected to the Ultrasonic sensor Echo */

#define PIO_WATER_LEVEL     (9)   /* PIO connected to the sensor to measure the water level*/
#define PIO_GLASS_POSITION (31)  /* PIO connected to the infrared to validate the glass position*/

#define PIO_DIR_OUTPUT  ( TRUE )    /* PIO direction configured as output */
#define PIO_DIR_INPUT   ( FALSE )   /* PIO direction configured as input */

#define BLE_GATTS_OP_WRITE_REQ   0x01


/* Maximum number of timers */
#define MAX_APP_TIMERS                 (5)

/*Number of IRKs that application can store */
#define MAX_NUMBER_IRK_STORED          (1)

/* Time after which HR Measurements will be transmitted to the connected 
 * host.
 */
#define HR_MEAS_TIME                   (1 * SECOND)

/* Magic value to check the sanity of NVM region used by the application */
#define NVM_SANITY_MAGIC               (0xAB04)

/* NVM offset for NVM sanity word */
#define NVM_OFFSET_SANITY_WORD         (0)

/* NVM offset for bonded flag */
#define NVM_OFFSET_BONDED_FLAG         (NVM_OFFSET_SANITY_WORD + 1)

/* NVM offset for bonded device bluetooth address */
#define NVM_OFFSET_BONDED_ADDR         (NVM_OFFSET_BONDED_FLAG + \
                                        sizeof(g_hr_data.bonded))

/* NVM offset for diversifier */
#define NVM_OFFSET_SM_DIV              (NVM_OFFSET_BONDED_ADDR + \
                                        sizeof(g_hr_data.bonded_bd_addr))

/* NVM offset for IRK */
#define NVM_OFFSET_SM_IRK              (NVM_OFFSET_SM_DIV + \
                                        sizeof(g_hr_data.diversifier))

/* Number of words of NVM used by application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define NVM_MAX_APP_MEMORY_WORDS       (NVM_OFFSET_SM_IRK + \
                                        MAX_WORDS_IRK)


/* Slave device is not allowed to transmit another Connection Parameter 
 * Update request till time TGAP(conn_param_timeout). Refer to section 9.3.9.2,
 * Vol 3, Part C of the Core 4.0 BT spec. The application should retry the 
 * 'connection paramter update' procedure after time TGAP(conn_param_timeout)
 * which is 30 seconds.
 */
#define GAP_CONN_PARAM_TIMEOUT         (30 * SECOND)


#ifdef NO_ACTUAL_MEASUREMENT

/* Heart Rate used as a base for HR Measurements */
#define HEART_RATE_IN_BPM              (78)

/* Dummy RR interval in milliseconds */
#define RR_INTERVAL_IN_MS              (0x1e0)

#else /* NO_ACTUAL_MEASUREMENT */

/* Idle time out period in Connected state. Device will disconnect with the
 * connected host at the expiry of this timer
 */
#define CONNECTED_IDLE_TIMEOUT_VALUE   (10 * SECOND)

/* RR intervals are stored in the circular queue in units of 1/1024 seconds,
 * that is, an RR interval of 1 means that the RR interval is 1/1024 s. When
 * heart rate is calculated in beats per minute, the
 * number of RR intervals in queue/sum of RR intervals will have to be
 * multiplied by 60 * 1024. This macro stores this conversion factor
 */
#define CONVERSION_FACTOR              (60 * 1024UL)

#endif /* ! NO_ACTUAL_MEASUREMENT */

/* Static value for Energy Expended in KJoules used by example application */
#define ENERGY_EXP_PER_HR_MEAS         (2)

/*============================================================================*
 *  Private Data Types
 *============================================================================*/
/* Enumeration to register the known button states */
typedef enum _button_state
{
    button_state_down,       /* Button was pressed */
    button_state_up,         /* Button was released */
    button_state_unknown     /* Button state is unknown */
} BUTTON_STATE_T;

                                        
/*============================================================================*
 *  Public Data
 *============================================================================*/

/* Current state of button */
static BUTTON_STATE_T g_cur_button_state_water = button_state_unknown;   
static BUTTON_STATE_T g_cur_button_state_glass = button_state_unknown;
static BUTTON_STATE_T g_cur_button_state = button_state_unknown;
static uint16 cont = 0; 
static uint16 result = 0;
                                        
/* HR Sensor application data instance */
HR_DATA_T g_hr_data;

/*============================================================================*
 *  Private Data
 *============================================================================*/

/* Declare space for application timers. */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_APP_TIMERS];

static uint32 initTime;
static uint32 finalTime;

/*============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void hrSensorDataInit(void);
static void readPersistentStore(void);

#ifdef NO_ACTUAL_MEASUREMENT

static uint8* simulateHRMeasReading(uint8 *p_length);

#else /* NO_ACTUAL_MEASUREMENT */

static void hrSensorIdleTimerHandler(timer_id tid);
static void addRRToQueue(uint32 raw_RR);
static uint16 getRRValuesInQueue(uint8 *p_rr_val_arr);
static uint8* receivedHRMeasReading(uint8 *p_length);

#endif /* ! NO_ACTUAL_MEASUREMENT */

/* RR value generator */
static void hrMeasTimerHandler(timer_id tid);

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      hrSensorDataInit
 *
 *  DESCRIPTION
 *      This function is called to initialise heart rate sensor application 
 *      data structure.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void hrSensorDataInit(void)
{
    TimerDelete(g_hr_data.app_tid);
    g_hr_data.app_tid = TIMER_INVALID;

    TimerDelete(g_hr_data.hr_meas_tid);
    g_hr_data.hr_meas_tid = TIMER_INVALID;

    g_hr_data.pairing_button_pressed = FALSE;

    TimerDelete(g_hr_data.con_param_update_tid);
    g_hr_data.con_param_update_tid = TIMER_INVALID;

    g_hr_data.st_ucid = GATT_INVALID_UCID;

    g_hr_data.enable_white_list = FALSE;

    g_hr_data.advert_timer_value = 0;

#ifndef NO_ACTUAL_MEASUREMENT
    /* Initialise circular queue buffer */
    g_hr_data.pending_rr_values.start_idx = 0;
    g_hr_data.pending_rr_values.num = 0;

#endif /* ! NO_ACTUAL_MEASUREMENT */

    /* HR sensor hardware data initialisation */
    HrHwDataInit();

    /* Initialise GAP Data structure */
    GapDataInit();

    /* Battery Service data initialisation */
    BatteryDataInit();

    /* Heart Rate Service data initialisation */
    HRDataInit();

}

/****************************************************************************
NAME
    UartDataRxCallback

DESCRIPTION
    This callback is issued when data is received over UART. Application
    may ignore the data, if not required. For more information refer to
    the API documentation for the type "uart_data_out_fn"

RETURNS
    The number of words processed, return data_count if all of the received
    data had been processed (or if application don't care about the data)
*/
static uint16 UartDataRxCallback ( void* p_data, uint16 data_count,
        uint16* p_num_additional_words )
{
    *p_num_additional_words = 0; /* Application do not need any additional
                                       data to be received */
    return data_count;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      readPersistentStore
 *
 *  DESCRIPTION
 *      This function is used to initialise and read NVM data
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void readPersistentStore(void)
{
    /* NVM offset for supported services */
    uint16 nvm_offset = NVM_MAX_APP_MEMORY_WORDS;
    uint16 nvm_sanity = 0xffff;
    bool nvm_start_fresh = FALSE;

    /* Read persistent storage to know if the device was last bonded 
     * to another device 
     */

    /* If the device was bonded, trigger fast undirected advertisements by 
     * setting the white list for bonded host. If the device was not bonded, 
     * trigger undirected advertisements for any host to connect.
     */

    Nvm_Read(&nvm_sanity, 
             sizeof(nvm_sanity), 
             NVM_OFFSET_SANITY_WORD);

    if(nvm_sanity == NVM_SANITY_MAGIC)
    {

        /* Read Bonded Flag from NVM */
        Nvm_Read((uint16*)&g_hr_data.bonded,
                  sizeof(g_hr_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);

        if(g_hr_data.bonded)
        {

            /* Bonded Host Typed BD Address will only be stored if bonded flag
             * is set to TRUE. Read last bonded device address.
             */
            Nvm_Read((uint16*)&g_hr_data.bonded_bd_addr, 
                       sizeof(TYPED_BD_ADDR_T),
                       NVM_OFFSET_BONDED_ADDR);

            /* If device is bonded and bonded address is resolvable then read 
             * the bonded device's IRK
             */
            if(GattIsAddressResolvableRandom(&g_hr_data.bonded_bd_addr))
            {
                Nvm_Read(g_hr_data.central_device_irk.irk, 
                         MAX_WORDS_IRK,
                         NVM_OFFSET_SM_IRK);
            }

        }
        else /* Case when we have only written NVM_SANITY_MAGIC to NVM but 
              * didn't get bonded to any host in the last powered session
              */
        {
            g_hr_data.bonded = FALSE;
        }

        /* Read the diversifier associated with the presently bonded/last 
         * bonded device.
         */
        Nvm_Read(&g_hr_data.diversifier, 
                 sizeof(g_hr_data.diversifier),
                 NVM_OFFSET_SM_DIV);

        /* If NVM in use, read device name and length from NVM */
        GapReadDataFromNVM(&nvm_offset);

    }
    else /* NVM Sanity check failed means either the device is being brought up 
          * for the first time or memory has got corrupted in which case 
          * discard the data and start fresh.
          */
    {

        nvm_start_fresh = TRUE;

        nvm_sanity = NVM_SANITY_MAGIC;

        /* Write NVM Sanity word to the NVM */
        Nvm_Write(&nvm_sanity, 
                  sizeof(nvm_sanity), 
                  NVM_OFFSET_SANITY_WORD);

        /* The device will not be bonded as it is coming up for the first 
         * time 
         */
        g_hr_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_hr_data.bonded, 
                  sizeof(g_hr_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);

        /* When the application is coming up for the first time after flashing 
         * the image to it, it will not have bonded to any device. So, no LTK 
         * will be associated with it. Hence, set the diversifier to 0.
         */
        g_hr_data.diversifier = 0;

        /* Write the same to NVM. */
        Nvm_Write(&g_hr_data.diversifier, 
                  sizeof(g_hr_data.diversifier),
                  NVM_OFFSET_SM_DIV);

        /* If fresh NVM, write device name and length to NVM for the 
         * first time.
         */
        GapInitWriteDataToNVM(&nvm_offset);

    }

    /* Read Heart Rate service data from NVM if the devices are bonded and  
     * update the offset with the number of words of NVM required by 
     * this service
     */
    HeartRateReadDataFromNVM(nvm_start_fresh, &nvm_offset);

    /* Read Battery service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    BatteryReadDataFromNVM(&nvm_offset);

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      hrMeasTimerHandler
 *
 *  DESCRIPTION
 *      Called repeatedly via a timer to transmit HR measurements
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void hrMeasTimerHandler(timer_id tid)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            if(tid == g_hr_data.hr_meas_tid)
            {
                uint8 *p_hr_meas_data = NULL;
                uint8   hr_meas_len = 0;

                g_hr_data.hr_meas_tid = TIMER_INVALID;

                if(IsHeartRateNotifyEnabled())
                {

#ifdef NO_ACTUAL_MEASUREMENT

                    /* Read simulated HR measurement and send dummy data */
                    p_hr_meas_data = simulateHRMeasReading(&hr_meas_len);

#else /* NO_ACTUAL_MEASUREMENT */

                    /* Received HR measurement reading */
                    p_hr_meas_data = receivedHRMeasReading(&hr_meas_len);

#endif /* !NO_ACTUAL_MEASUREMENT */

                    if(hr_meas_len)
                    {
                       /* HeartRateSendMeasValue(g_hr_data.st_ucid, hr_meas_len, 
                                                         p_hr_meas_data);*/

#ifndef NO_ACTUAL_MEASUREMENT

                        /* Reset Idle timer only if we have received some 
                         * measurements to transmit
                         */
                        ResetIdleTimer();

#endif /* !NO_ACTUAL_MEASUREMENT */

                    }

                }

                /* Restart HR Measurement timer */
                g_hr_data.hr_meas_tid = TimerCreate(HR_MEAS_TIME, 
                                                    TRUE, hrMeasTimerHandler);

            }
        }
        break;

        case app_state_disconnecting:
        {
            /* Do nothing in this state as the device has triggered 
             * disconnect 
             */
            g_hr_data.hr_meas_tid = TIMER_INVALID;
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }

}

#ifdef NO_ACTUAL_MEASUREMENT

/*----------------------------------------------------------------------------*
 *  NAME
 *      simulateHRMeasReading
 *
 *  DESCRIPTION
 *      This function formulates HR Measurement data in a format
 *      given by HR service specification. It simulates static 
 *      HR measurement reading. Energy Expended is added to every 
 *      10th measurement.
 *
 *  RETURNS
 *      uint8 Pointer - HR Measurement data 
 *
 *---------------------------------------------------------------------------*/

static uint8* simulateHRMeasReading(uint8 *p_length)
{

    uint8 *p_hr_meas = g_hr_data.hr_meas_data;
    uint16 energy_exp;
    static uint8 meas_count = 0;
    uint8  *p_hr_meas_flags = NULL;

    p_hr_meas_flags = &p_hr_meas[(*p_length)++];

    *p_hr_meas_flags = SENSOR_MEASUREVAL_FORMAT_UINT8 | 
                       SENSOR_IN_CONTACT | 
                       RR_INTERVAL_PRESENT;

    /* 78 +/- 32 bpm */
    p_hr_meas[(*p_length)++] = HEART_RATE_IN_BPM + 
                               (32 - (int32)(TimeGet16() % 16));

    /* Note: Vendors should use their own proprietary algorithms to 
     * compute Energy expended from Heart Rate measurements. 
     * For this example application we are just adding static value.
     */
    HeartRateIncEnergyExpended(ENERGY_EXP_PER_HR_MEAS);

    ++meas_count;

    /* Energy Expended is sent at regular interval  as defined in  
     * Heart Rate Service module based on HR Profile spec ver 1.0
     * recommendations
     */
    if(meas_count / HR_MEAS_ENERGY_EXP_PERIOD)
    {
        energy_exp = HeartRateGetEnergyExpended();

        p_hr_meas[(*p_length)++] = LE8_L(energy_exp);
        p_hr_meas[(*p_length)++] = LE8_H(energy_exp);

        *p_hr_meas_flags |= ENERGY_EXP_AVAILABLE;

        meas_count = 0;
    }

    /* 480 +/- 32 */
    p_hr_meas[(*p_length)++] = LE8_L(RR_INTERVAL_IN_MS) + 
                               (32 - (int32)(TimeGet16() % 16));
    p_hr_meas[(*p_length)++] = LE8_H(RR_INTERVAL_IN_MS);

    return p_hr_meas;
}


#else /* NO_ACTUAL_MEASUREMENT */

/*----------------------------------------------------------------------------*
 *  NAME
 *      addRRToQueue
 *
 *  DESCRIPTION
 *      This function is used to add RR values to circular queue maintained 
 *      by application. The RR values will get notified to the collector once
 *      every second(RR_TIMER_TIME).
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void addRRToQueue(uint32 raw_RR)
{
    uint8 add_idx;
    uint16 RR;

    /* Calculate RR value from the number of ticks after the last edge 
     * seen on the PIO reserved to capture heart beats.
     */

    /* To convert the number of ticks to milliseconds, divide the RR 
     * value by the frequency of the crystal.
     */
    RR = raw_RR >> 5;  /* divide by 32 to get 1/1024secs */

    /* Add new RR value to the end of circular queue. If Max circular 
     * queue length has reached the oldest RR value will get 
     * overwritten 
     */
    add_idx = (g_hr_data.pending_rr_values.start_idx + 
                    g_hr_data.pending_rr_values.num)% MAX_RR_VALUES;

    g_hr_data.pending_rr_values.rr_value[add_idx] = RR;
    
    if(g_hr_data.pending_rr_values.num < MAX_RR_VALUES)
    {
        ++ g_hr_data.pending_rr_values.num;
    }
    else /* Oldest RR value overwalritten, move the index */
    {
        g_hr_data.pending_rr_values.start_idx = (add_idx + 1) % 
                    MAX_RR_VALUES;
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      getRRValuesInQueue
 *
 *  DESCRIPTION
 *      This function is used to get all the RR values in the queue.
 *
 *  RETURNS
 *      Sum of all the RR intervals in the circular queue.
 *
 *---------------------------------------------------------------------------*/

static uint16 getRRValuesInQueue(uint8 *p_rr_val_arr)
{
    uint8 start_idx = g_hr_data.pending_rr_values.start_idx;
    uint16 sum_rr_intervals = 0;
    uint16 rr_val;

    /* Go through the number of elements in the queue */
    while(g_hr_data.pending_rr_values.num)
    {

        rr_val = g_hr_data.pending_rr_values.rr_value[start_idx];

        /* Add available RR interval value */
        sum_rr_intervals += rr_val;

        /* Fill the value to output array */
        BufWriteUint16(&p_rr_val_arr, rr_val);

        /* Move start index to next element in circular queue */
        start_idx = (start_idx + 1) % MAX_RR_VALUES;

        /* Decrement the number of elements in circular queue */
        -- g_hr_data.pending_rr_values.num;
    }

    /* Assign the new start index */
    g_hr_data.pending_rr_values.start_idx = start_idx;

    return sum_rr_intervals;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      receivedHRMeasReading
 *
 *  DESCRIPTION
 *      This function formulates HR Measurement data in a format
 *      given by HR service specification. It collates received RR Intervals 
 *      and derives HR measurement value from it. Energy expended is added 
 *      to every 10th measurement.
 *
 *  RETURNS
 *      uint8 Pointer - HR Measurement data 
 *
 *---------------------------------------------------------------------------*/

static uint8* receivedHRMeasReading(uint8 *p_length)
{

    uint8 *p_hr_meas = g_hr_data.hr_meas_data;
    uint16 energy_exp;
    static uint8 meas_count = 0;
    uint8  *p_hr_meas_flags = NULL;
    uint8 num_of_rr_val = g_hr_data.pending_rr_values.num;

    if(num_of_rr_val)
    {
        uint16 sum_of_rr_int;
        uint8   *p_hr_value = NULL;

        p_hr_meas_flags = &p_hr_meas[(*p_length)++];

        *p_hr_meas_flags = SENSOR_MEASUREVAL_FORMAT_UINT8 | 
                           SENSOR_IN_CONTACT | 
                           RR_INTERVAL_PRESENT;

        /* Store reference of HR measurement value location as it will get 
         * filled later
         */
        p_hr_value = &p_hr_meas[(*p_length)++];

        ++ meas_count;

        /* Note: Vendors should use their own proprietary algorithms to 
         * compute energy expended from heart rate measurements. 
         * For this example application we are just adding static value.
         */
        HeartRateIncEnergyExpended(ENERGY_EXP_PER_HR_MEAS);

        /* Energy expended is sent at regular interval  as defined in  
         * heart rate service module based on HR Profile spec ver 1.0
         * recommendations
         */
        if(meas_count / HR_MEAS_ENERGY_EXP_PERIOD)
        {

            energy_exp = HeartRateGetEnergyExpended();

            p_hr_meas[(*p_length)++] = LE8_L(energy_exp);
            p_hr_meas[(*p_length)++] = LE8_H(energy_exp);

            *p_hr_meas_flags |= ENERGY_EXP_AVAILABLE;

            meas_count = 0;
        }

        sum_of_rr_int = getRRValuesInQueue(&p_hr_meas[*p_length]);

        /* Increment the length by the number of RR intervals added */
        *p_length += num_of_rr_val * 2;

        /* If num_of_rr_val are accumulated in sum_of_rr_int time units(the time
         * unit used is 1/1024 th of a second), then, how many RR values will be
         * accumulated in 60 seconds will give the heart rate in bpm.
         * heart_rate_in_bpm = (60 * num_of_rr_val)/ (sum_of_rr_int * (1/1024))
         */
        *p_hr_value = ( CONVERSION_FACTOR * num_of_rr_val) / sum_of_rr_int;

    }

    return p_hr_meas;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      hrSensorIdleTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to handle IDLE timer.expiry in connected states.
 *      At the expiry of this timer, application shall disconnect with the 
 *      host and shall move to 'app_state_disconnecting' state.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void hrSensorIdleTimerHandler(timer_id tid)
{

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            if(tid == g_hr_data.app_tid)
            {
                /* Trigger Disconnect and move to app_state_disconnecting 
                 * state 
                 */

                g_hr_data.app_tid = TIMER_INVALID;

                /* Delete HR Measurement timer */
                TimerDelete(g_hr_data.hr_meas_tid);
                g_hr_data.hr_meas_tid = TIMER_INVALID;

                AppSetState(app_state_disconnecting);

            } /* Else ignore the timer */
        }
        break;

        default:
            /* Ignore timer in any other state */
        break;
    }

}

#endif /* ! NO_ACTUAL_MEASUREMENT */


/*----------------------------------------------------------------------------*
 *  NAME
 *      requestConnParamUpdate
 *
 *  DESCRIPTION
 *      This function is used to send L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST 
 *      to the remote device when an earlier sent request had failed.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void requestConnParamUpdate(timer_id tid)
{
    /* Application specific preferred paramters */
    ble_con_params app_pref_conn_param = 
            {
                PREFERRED_MIN_CON_INTERVAL,
                PREFERRED_MAX_CON_INTERVAL,
                PREFERRED_SLAVE_LATENCY,
                PREFERRED_SUPERVISION_TIMEOUT
            };

    if(g_hr_data.con_param_update_tid == tid)
    {

        g_hr_data.con_param_update_tid = TIMER_INVALID;

        /*Handling signal as per current state */
        switch(g_hr_data.state)
        {

            case app_state_connected:
            {
                /* Send Connection Parameter Update request using application 
                 * specific preferred connection paramters
                 */

                if(LsConnectionParamUpdateReq(&g_hr_data.con_bd_addr, 
                                 &app_pref_conn_param) != ls_err_none)
                {
                    ReportPanic(app_panic_con_param_update);
                }

                /* Increment the count for Connection Parameter Update 
                 * requests 
                 */
                ++ g_hr_data.num_conn_update_req;

            }
            break;

            default:
                /* Ignore in other states */
            break;
        }

    } /* Else ignore the timer */

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      appInitExit
 *
 *  DESCRIPTION
 *      This function is called upon exiting from app_state_init state. The 
 *      application starts advertising after exiting this state.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void appInitExit(void)
{
        if(g_hr_data.bonded && 
            (!GattIsAddressResolvableRandom(&g_hr_data.bonded_bd_addr)))
        {
            /* If the device is bonded and bonded device address is not
             * resolvable random, configure White list with the Bonded 
             * host address 
             */
            if(LsAddWhiteListDevice(&g_hr_data.bonded_bd_addr)!=
                ls_err_none)
            {
                ReportPanic(app_panic_add_whitelist);
            }
        }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      appAdvertisingExit
 *
 *  DESCRIPTION
 *      This function is called while exiting app_state_fast_advertising and
 *      app_state_slow_advertising states.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void appAdvertisingExit(void)
{
        /* Cancel advertisement timer */
        TimerDelete(g_hr_data.app_tid);
        g_hr_data.app_tid = TIMER_INVALID;
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAddDBCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ADD_DB_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattAddDBCfm(GATT_ADD_DB_CFM_T *p_event_data)
{
    switch(g_hr_data.state)
    {
        case app_state_init:
        {
            if(p_event_data->result == sys_status_success)
            {
                AppSetState(app_state_fast_advertising);
            }
            else
            {
                /* Don't expect this to happen */
                ReportPanic(app_panic_db_registration);
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCancelConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CANCEL_CONNECT_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattCancelConnectCfm(void)
{

    if(g_hr_data.pairing_button_pressed)
    {
        g_hr_data.pairing_button_pressed = FALSE;

        g_hr_data.enable_white_list = FALSE;

        /* Reset and clear the whitelist */
        LsResetWhiteList();

        /* Trigger fast advertisements */
        if(g_hr_data.state == app_state_fast_advertising)
        {
            GattTriggerFastAdverts();
        }
        else
        {
            AppSetState(app_state_fast_advertising);
        }
    }
    else
    {

        /*Handling signal as per current state */
        switch(g_hr_data.state)
        {
            case app_state_fast_advertising:
            {
                bool fastConns = FALSE;

                if(g_hr_data.enable_white_list == TRUE)
                {

                    /* Bonded device advertisements stopped. Reset white 
                     * list 
                     */
                    if(LsDeleteWhiteListDevice(&g_hr_data.bonded_bd_addr)
                        != ls_err_none)
                    {
                        ReportPanic(app_panic_delete_whitelist);
                    }

                    /* Case of stopping advertisements for the bonded device 
                     * at the expiry of 10 seconds timer
                     */
                    g_hr_data.enable_white_list = FALSE;

                    fastConns = TRUE;
                }

                if(fastConns)
                {
                    GattStartAdverts(TRUE);

                    /* Remain in same state */
                }
                else
                {
                    AppSetState(app_state_slow_advertising);
                }
            }
            break;

            case app_state_slow_advertising:
                AppSetState(app_state_idle);
            break;
            
            default:
                /* Control should never come here */
                ReportPanic(app_panic_invalid_state);
            break;
        }

    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CONNECT_CFM
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* p_event_data)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_fast_advertising:
        case app_state_slow_advertising:
        {
            if(p_event_data->result == sys_status_success)
            {
                /* Store received UCID */
                g_hr_data.st_ucid = p_event_data->cid;

                /* Store connected BD Address */
                g_hr_data.con_bd_addr = p_event_data->bd_addr;

                if(g_hr_data.bonded && 
                   GattIsAddressResolvableRandom(&g_hr_data.bonded_bd_addr) &&
                   (SMPrivacyMatchAddress(&p_event_data->bd_addr,
                                            g_hr_data.central_device_irk.irk,
                                            MAX_NUMBER_IRK_STORED, 
                                            MAX_WORDS_IRK) < 0))
                {
                    /* Application was bonded to a remote device using 
                     * resolvable random address and application has failed to 
                     * resolve the remote device address to which we just 
                     * connected So disconnect and start advertising again
                     */

                    /* Disconnect if we are connected */
                    AppSetState(app_state_disconnecting);
                }
                else
                {
                    /* Enter connected state 
                     * - If the device is not bonded OR
                     * - If the device is bonded and the connected host doesn't 
                     *   support Resolvable Random address OR
                     * - If the device is bonded and connected host supports 
                     *   Resolvable Random address and the address gets resolved
                     *   using the store IRK key
                     */
                    AppSetState(app_state_connected);
                }                
            }
            else
            {
                /* Move to app_state_idle state and wait for some user event 
                 * to trigger advertisements
                 */
                AppSetState(app_state_idle);
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmKeysInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_KEYS_IND and copies IRK from it
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalSmKeysInd(SM_KEYS_IND_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {

            /* Store the diversifier which will be used for accepting/rejecting
             * the encryption requests.
             */
            g_hr_data.diversifier = (p_event_data->keys)->div;

            /* Write the new diversifier to NVM */
            Nvm_Write(&g_hr_data.diversifier,
                      sizeof(g_hr_data.diversifier), 
                      NVM_OFFSET_SM_DIV);

            /* Store IRK if the connected host is using random resolvable 
             * address. IRK is used afterwards to validate the identity of 
             * connected host 
             */
            if(GattIsAddressResolvableRandom(&g_hr_data.con_bd_addr)) 
            {
                MemCopy(g_hr_data.central_device_irk.irk, 
                        (p_event_data->keys)->irk,
                        MAX_WORDS_IRK);

                /* If bonded device address is resolvable random
                 * then store IRK to NVM 
                 */
                Nvm_Write(g_hr_data.central_device_irk.irk, 
                          MAX_WORDS_IRK, 
                          NVM_OFFSET_SM_IRK);
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmSimplePairingCompleteInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalSmSimplePairingCompleteInd(
                                 SM_SIMPLE_PAIRING_COMPLETE_IND_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {            
            if(p_event_data->status == sys_status_success)
            {
                /* Store bonded host information to NVM. This includes
                 * application and services specific information
                 */
                g_hr_data.bonded = TRUE;
                g_hr_data.bonded_bd_addr = p_event_data->bd_addr;

                /* Store bonded host typed bd address to NVM */

                /* Write one word bonded flag */
                Nvm_Write((uint16*)&g_hr_data.bonded, 
                          sizeof(g_hr_data.bonded), 
                          NVM_OFFSET_BONDED_FLAG);

                /* Write typed bd address of bonded host */
                Nvm_Write((uint16*)&g_hr_data.bonded_bd_addr, 
                          sizeof(TYPED_BD_ADDR_T), 
                          NVM_OFFSET_BONDED_ADDR);

                /* Configure white list with the Bonded host address only 
                 * if the connected host doesn't support random resolvable
                 * address
                 */
                if(!GattIsAddressResolvableRandom(&g_hr_data.bonded_bd_addr))
                {
                    /* It is important to note that this application 
                     * doesn't support reconnection address. In future, if 
                     * the application is enhanced to support Reconnection 
                     * Address, make sure that we don't add reconnection 
                     * address to white list
                     */
                    if(LsAddWhiteListDevice(&g_hr_data.bonded_bd_addr) !=
                        ls_err_none)
                    {
                        ReportPanic(app_panic_add_whitelist);
                    }
                }

                /* If the devices are bonded then send notification to all 
                 * registered services for the same so that they can store
                 * required data to NVM.
                 */

                HeartRateBondingNotify();

                BatteryBondingNotify();
            }
            else
            {

                /* If application is already bonded to this host and pairing 
                 * fails, remove device from the white list.
                 */
                if(AppIsDeviceBonded())
                {
                    if(LsDeleteWhiteListDevice(&g_hr_data.bonded_bd_addr) != 
                                        ls_err_none)
                    {
                        ReportPanic(app_panic_delete_whitelist);
                    }

                    g_hr_data.bonded = FALSE;
                }

                /* The case when pairing has failed. The connection may still be
                 * there if the remote device hasn't disconnected. The
                 * remote device may retry pairing after a time defined by its
                 * own application. So reset all other variables except the
                 * connection specific ones.
                 */

                /* Update bonded status to NVM */
                Nvm_Write((uint16*)&g_hr_data.bonded, 
                          sizeof(g_hr_data.bonded), 
                          NVM_OFFSET_BONDED_FLAG);

#ifndef NO_ACTUAL_MEASUREMENT
                /* Initialise circular queue buffer as old heart rate data 
                 * is not valid any more
                 */
                g_hr_data.pending_rr_values.start_idx = 0;
                g_hr_data.pending_rr_values.num = 0;

#endif /* ! NO_ACTUAL_MEASUREMENT */

                /* Initialise the data of used services as the device is no 
                 * more bonded to the remote host
                 */
                GapDataInit();
                BatteryDataInit();
                HRDataInit();

            }
        }
        break;
        
        default:
            /* Firmware may send this signal after disconnection. So don't 
             * panic but ignore this signal.
             */
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLMEncryptionChange
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_ENCRYPTION_CHANGE
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLMEncryptionChange(
            HCI_EV_DATA_ENCRYPTION_CHANGE_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {

            if ((p_event_data->status == sys_status_success) &&
                (p_event_data->enc_enable == TRUE))
            {
                /* Check if the timer is not running, start timer to 
                 * trigger Connection Parameter Update procedure
                 */
                if(g_hr_data.con_param_update_tid  == TIMER_INVALID)
                {
                    /* Set the num of conn update attempts to zero */
                    g_hr_data.num_conn_update_req = 0;

                    /* Start timer to trigger Connection Paramter Update 
                     * procedure 
                     */
                    g_hr_data.con_param_update_tid = TimerCreate(
                                                 GAP_CONN_PARAM_TIMEOUT,
                                                 TRUE, requestConnParamUpdate);
                } /* Else at the expiry of timer Connection parameter 
                   * update procedure will get triggered
                   */

            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmDivApproveInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_DIV_APPROVE_IND.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_hr_data.state)
    {
        
        /* Request for approval from application comes only when pairing is not
         * in progress
         */
        case app_state_connected:
        {
            sm_div_verdict approve_div = SM_DIV_REVOKED;
            
            /* Check whether the application is still bonded (bonded flag gets
             * reset upon 'connect' button press by the user). Then check 
             * whether the diversifier is the same as the one stored by the 
             * application
             */
            if(AppIsDeviceBonded())
            {
                if(g_hr_data.diversifier == p_event_data->div)
                {
                    approve_div = SM_DIV_APPROVED;
                }
            }

            SMDivApproval(p_event_data->cid, approve_div);
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;

    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateCfm
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_CFM.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLsConnParamUpdateCfm(
                            LS_CONNECTION_PARAM_UPDATE_CFM_T *p_event_data)
{
    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            /* Received in response to the L2CAP_CONNECTION_PARAMETER_UPDATE 
              * request sent from the slave after encryption is enabled. If 
              * the request has failed, the device should again send the same 
              * request only after Tgap(conn_param_timeout). Refer 
              * Bluetooth 4.0 spec Vol 3 Part C, Section 9.3.9 and profile spec.
              */
            if ((p_event_data->status != ls_err_none) &&
                    (g_hr_data.num_conn_update_req < 
                    MAX_NUM_CONN_PARAM_UPDATE_REQS))
            {
                /* Delete timer if running */
                TimerDelete(g_hr_data.con_param_update_tid);

                g_hr_data.con_param_update_tid = TimerCreate(
                                             GAP_CONN_PARAM_TIMEOUT,
                                             TRUE, requestConnParamUpdate);
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLsConnParamUpdateInd(
                                 LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data)
{

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {

        case app_state_connected:
        {
            /* Delete timer if running */
            TimerDelete(g_hr_data.con_param_update_tid);

            /* Connection parameters have been updated. Check if new parameters 
             * comply with application preffered parameters. If not, application 
             * shall trigger Connection parameter update procedure 
             */
             if(p_event_data->conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                p_event_data->conn_interval > PREFERRED_MAX_CON_INTERVAL ||
                p_event_data->conn_latency < PREFERRED_SLAVE_LATENCY)
            {
                /* Set the num of conn update attempts to zero */
                g_hr_data.num_conn_update_req = 0;

                /* Start timer to trigger Connection Paramter Update 
                 * procedure 
                 */
                g_hr_data.con_param_update_tid = TimerCreate(
                                         GAP_CONN_PARAM_TIMEOUT,
                                         TRUE, requestConnParamUpdate);

            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAccessInd
 *
 *  DESCRIPTION
 *      This function handles GATT_ACCESS_IND message for attributes 
 *      maintained by the application.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *p_event_data)
{

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            /* Received GATT ACCESS IND with write access */
            if(p_event_data->flags == 
                (ATT_ACCESS_WRITE | 
                 ATT_ACCESS_PERMISSION | 
                 ATT_ACCESS_WRITE_COMPLETE))
            {
                DebugWriteString("HandleAccessWrite\n");
                HandleAccessWrite(p_event_data);

            }
            /* Received GATT ACCESS IND with read access */
            else if(p_event_data->flags == 
                (ATT_ACCESS_READ | 
                ATT_ACCESS_PERMISSION))
            {
                DebugWriteString("HandleAccessRead\n");
                HandleAccessRead(p_event_data);
            }
            else
            {
                GattAccessRsp(p_event_data->cid, p_event_data->handle, 
                              gatt_status_request_not_supported,
                              0, NULL);
                 DebugWriteString("HandleAccess not supported\n");
            }
        }
        break;

        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmDisconnectComplete
 *
 *  DESCRIPTION
 *      This function handles LM Disconnect Complete event which is received
 *      at the completion of disconnect procedure triggered either by the 
 *      device or remote host or because of link loss 
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void handleSignalLmDisconnectComplete(
                HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data)
{

    /* LM_EV_DISCONNECT_COMPLETE event can have following disconnect 
     * reasons:
     *
     * HCI_ERROR_CONN_TIMEOUT - Link Loss case
     * HCI_ERROR_CONN_TERM_LOCAL_HOST - Disconnect triggered by device
     * HCI_ERROR_OETC_* - Other end (i.e., remote host) terminated connection
     */

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
            /* Initialise heart rate sensor data instance */
            hrSensorDataInit();

            /* FALLTHROUGH */

        case app_state_disconnecting:
        {

            /* Link Loss Case */
            if(p_event_data->reason == HCI_ERROR_CONN_TIMEOUT)
            {
                /* Start undirected advertisements by moving to 
                 * app_state_fast_advertising state
                 */
                AppSetState(app_state_fast_advertising);
            }
            else if(p_event_data->reason == HCI_ERROR_CONN_TERM_LOCAL_HOST)
            {

                if(g_hr_data.state == app_state_connected)
                {
                    /* It is possible to receive LM_EV_DISCONNECT_COMPLETE 
                     * event in app_state_connected state at the expiry of 
                     * lower layers ATT/SMP timer leading to disconnect
                     */

                    /* Start undirected advertisements by moving to 
                     * app_state_fast_advertising state
                     */
                    AppSetState(app_state_fast_advertising);
                }
                else
                {
                    /* Case when application has triggered disconnect */

                    if(g_hr_data.bonded)
                    {
                        /* If the device is bonded and host uses resolvable 
                         * random address, the device initiates disconnect  
                         * procedure if it gets reconnected to a different host,
                         * in which case device should trigger fast 
                         * advertisements after disconnecting from the last 
                         * connected host.
                         */
                        if(GattIsAddressResolvableRandom(
                                               &g_hr_data.bonded_bd_addr) &&
                           (SMPrivacyMatchAddress(&g_hr_data.con_bd_addr,
                                               g_hr_data.central_device_irk.irk,
                                               MAX_NUMBER_IRK_STORED, 
                                               MAX_WORDS_IRK) < 0))
                        {
                            AppSetState(app_state_fast_advertising);
                        }
                        else
                        {
                            /* Else move to app_state_idle state because of 
                             * inactivity
                             */
                            AppSetState(app_state_idle);
                        }
                    }
                    else /* Case of Bonding/Pairing removal */
                    {
                        /* Start undirected advertisements by moving to 
                         * app_state_fast_advertising state
                         */
                        AppSetState(app_state_fast_advertising);
                    }
                }

            }
            else /* Remote user terminated connection case */
            {
                /* If the device has not bonded but disconnected, it may just 
                 * have discovered the services supported by the application or 
                 * read some un-protected characteristic value like device name 
                 * and disconnected. The application should be connectable 
                 * because the same remote device may want to reconnect and 
                 * bond. If not the application should be discoverable by other 
                 * devices.
                 */
                if(!g_hr_data.bonded)
                {
                    AppSetState(app_state_fast_advertising);
                }
                else /* Case when disconnect is triggered by a bonded Host */
                {
                    AppSetState(app_state_idle);
                }
            }

        }
        break;
        
        default:
            /* Control should never come here */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

#ifndef NO_ACTUAL_MEASUREMENT

/*----------------------------------------------------------------------------*
 *  NAME
 *      ResetIdleTimer
 *
 *  DESCRIPTION
 *      This function is used to reset idle timer run by application in
 *      app_state_connected state
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void ResetIdleTimer(void)
{
    /* Reset Idle timer */
    TimerDelete(g_hr_data.app_tid);

    g_hr_data.app_tid = TimerCreate(CONNECTED_IDLE_TIMEOUT_VALUE, 
                                    TRUE, hrSensorIdleTimerHandler);
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleHRInputEvent
 *
 *  DESCRIPTION
 *      This function is used to handle HR Input PIO event.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandleHRInputEvent(void)
{
    uint32 RR;

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            RR = GetRRValue();
            addRRToQueue(RR);
        }
        break;

        case app_state_idle:
            /* Trigger fast advertisements */
            AppSetState(app_state_fast_advertising);
        break;

         default:
            /* Ignore in remaining states */
        break;

    }

}

#endif /* !NO_ACTUAL_MEASUREMENT */


/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleShortButtonPress
 *
 *  DESCRIPTION
 *      This function contains handling of short button press event.
 *
 *      If NO_ACTUAL_MEASUREMENT defined, handling will be as per current state
 *
 *      - Connected: Device will disconnect from the connected host
 *      - Idle: Trigger advertisements
 *      - Advertising: Do nothing
 *
 *      If NO_ACTUAL_MEASUREMENT is not defined, short button press will get 
 *      ignored as the application is being driven by HR_INPUT_PIO events
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandleShortButtonPress(void)
{

#ifdef NO_ACTUAL_MEASUREMENT

    /* Indicate short button press using short beep */
    SoundBuzzer(buzzer_beep_short);

    /*Handling signal as per current state */
    switch(g_hr_data.state)
    {
        case app_state_connected:
        {
            /* Delete HR Measurement timer */
            TimerDelete(g_hr_data.hr_meas_tid);
            g_hr_data.hr_meas_tid = TIMER_INVALID;

            /* Initiate Disconnect with the remote host */
            AppSetState(app_state_disconnecting);
        }
        break;

        case app_state_idle:
            /* Trigger fast advertisements */
            AppSetState(app_state_fast_advertising);
        break;

         default:
            /* Ignore in remaining states */
        break;

    }
#endif /* NO_ACTUAL_MEASUREMENT */

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      HandleExtraLongButtonPress
 *
 *  DESCRIPTION
 *      This function contains handling of extra long button press, which
 *      triggers pairing / bonding removal
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandleExtraLongButtonPress(timer_id tid)
{
    if(tid == g_app_hw_data.button_press_tid)
    {
        /* Re-initialise timer id */
        g_app_hw_data.button_press_tid = TIMER_INVALID;

        /* Sound three beeps to indicate pairing removal to user */
        SoundBuzzer(buzzer_beep_thrice);

        /* Remove bonding information*/

        /* The device will no more be bonded */
        g_hr_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_hr_data.bonded, 
                  sizeof(g_hr_data.bonded), 
                  NVM_OFFSET_BONDED_FLAG);


        switch(g_hr_data.state)
        {

            case app_state_connected:
            {
                /* Disconnect with the connected host before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                AppSetState(app_state_disconnecting);

                /* Reset and clear the whitelist */
                LsResetWhiteList();
            }
            break;

            case app_state_fast_advertising:
            case app_state_slow_advertising:
            {
                /* Initialise application and services data related to 
                 * for bonding status
                 */
                hrSensorDataInit();

                /* Set flag for pairing / bonding removal */
                g_hr_data.pairing_button_pressed = TRUE;

                /* Stop advertisements first as it may be making use of white 
                 * list. Once advertisements are stopped, reset the whitelist
                 * and trigger advertisements again for any host to connect
                 */
                GattStopAdverts();
            }
            break;

            case app_state_disconnecting:
            {
                /* Disconnect procedure on-going, so just reset the whitelist 
                 * and wait for procedure to get completed before triggering 
                 * advertisements again for any host to connect. Application
                 * and services data related to bonding status will get 
                 * updated while exiting disconnecting state
                 */
                LsResetWhiteList();
            }
            break;

            default: /* app_state_init / app_state_idle handling */
            {
                /* Initialise application and services data related to 
                 * for bonding status
                 */
                hrSensorDataInit();

                /* Reset and clear the whitelist */
                LsResetWhiteList();

                /* Start fast undirected advertisements */
                AppSetState(app_state_fast_advertising);
            }
            break;

        }

    } /* Else ignore timer */

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      ReportPanic
 *
 *  DESCRIPTION
 *      This function calls firmware panic routine and gives a single point 
 *      of debugging any application level panics
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
extern void ReportPanic(app_panic_code panic_code)
{
    /* Raise panic */
    Panic(panic_code);
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      sendCoffeeLevel
 *
 *  DESCRIPTION
 *      This function start the trigger pin to measure the echo signal.
 *   
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
extern void sendCoffeeLevel(void)
{
       TimeDelayUSec(60000);
       PioSet(PIO_TRIGGER,0x0); 
       TimeDelayUSec(1500);
       PioSet(PIO_TRIGGER,0x1); 
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppSetState
 *
 *  DESCRIPTION
 *      This function is used to set the state of the application.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
 
extern void AppSetState(app_state new_state)
{
    /* Check if the new state to be set is not the same as the present state
     * of the application. */
    app_state old_state = g_hr_data.state;
    
    if (old_state != new_state)
    {
        /* Handle exiting old state */
        switch (old_state)
        {
            case app_state_init:
                appInitExit();
            break;

            case app_state_disconnecting:
                /* Common things to do whenever application exits
                 * app_state_disconnecting state.
                 */

                /* Initialise HR Sensor and used services data structure 
                 * while exiting Disconnecting state
                 */
                hrSensorDataInit();
            break;

            case app_state_fast_advertising:
            case app_state_slow_advertising:
                /* Common things to do whenever application exits
                 * APP_*_ADVERTISING state.
                 */
                appAdvertisingExit();
            break;

            case app_state_connected:
                /* Nothing to do */
            break;

            case app_state_idle:
                /* Nothing to do */
            break;

            default:
                /* Nothing to do */
            break;
        }

        /* Set new state */
        g_hr_data.state = new_state;

        /* Handle entering new state */
        switch (new_state)
        {
            case app_state_fast_advertising:
            {
                GattTriggerFastAdverts();

                /* Indicate advertising mode by sounding two short beeps */
                SoundBuzzer(buzzer_beep_twice);
            }
            break;

            case app_state_slow_advertising:
                GattStartAdverts(FALSE);
            break;

            case app_state_idle:
                /* Sound long beep to indicate non connectable mode*/
                SoundBuzzer(buzzer_beep_long);
            break;

            case app_state_connected:
            {
                /* Common things to do whenever application enters
                 * app_state_connected state.
                 */

                /* Update battery status at every connection instance. It may 
                 * not be worth updating timer more often, but again it will 
                 * primarily depend upon application requirements 
                 */
                BatteryUpdateLevel(g_hr_data.st_ucid);

                /* Start HR Measurement transmission timer */
                hrMeasTimerHandler(TIMER_INVALID);

#ifndef NO_ACTUAL_MEASUREMENT

                ResetIdleTimer();

#endif /* ! NO_ACTUAL_MEASUREMENT */

            }
            break;

            case app_state_disconnecting:
                GattDisconnectReq(g_hr_data.st_ucid);
            break;

            default:
            break;
        }
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppIsDeviceBonded
 *
 *  DESCRIPTION
 *      This function returns the status whether the connected device is 
 *      bonded or not.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern bool AppIsDeviceBonded(void)
{
    bool bonded = FALSE;

    if(g_hr_data.bonded)
    {
        /* For connected HR sensor application, to address Gymnasium use case,
         * it is possible that the sensor is bonded to a host (say HR monitor 
         * watch) but is connected to different host (Gym treadmill).
         */
        if(g_hr_data.state == app_state_connected)
        {
            if(!MemCmp(&g_hr_data.bonded_bd_addr, &g_hr_data.con_bd_addr, 
               sizeof(TYPED_BD_ADDR_T)))
            {
                bonded = TRUE;
            }
        }
        else
        {
            bonded = TRUE;
        }
    }

    return bonded;
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      InitStateButtons
 *
 *  DESCRIPTION
 *      This function gets the initial state of the inputs PIO_GLASS_POSITION
 *      and PIO_WATER_LEVEL.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void InitStateButtons(void){
    if(PioGet(PIO_GLASS_POSITION)){
        g_cur_button_state_glass = button_state_up;
    }else{
        g_cur_button_state_glass = button_state_down;
    }
    
    if(PioGet(PIO_WATER_LEVEL)){
         g_cur_button_state_water = button_state_up;
    }else{
        g_cur_button_state_water = button_state_down;
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      getCoffeeLevels
 *
 *  DESCRIPTION
 *      This function gets the state of the water level, glass position and 
 *      coffee level.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

static void getCoffeeLevels(void *data){
    uint8 cf_length;    
    const pio_changed_data *pPioData;    
    /*HandlePIOChangedEvent(((pio_changed_data*)data)->pio_cause);*/
    /* The PIO data is defined by struct pio_changed_data */
    pPioData = (const pio_changed_data *)data;            
    
    uint8* p_cf_data = g_hr_data.hr_meas_data;             
          
            
    /* If the PIO event comes from the button */
    if (pPioData->pio_cause & (1UL << PIO_WATER_LEVEL))
    {                
       /* If PIO was HIGH when this event was generated (that means
        * button was released, generating a rising edge causing PIO
        * to go from LOW to HIGH)
        */
        cf_length = 0x02;
        p_cf_data[0] = cf_control_point_level_water;                
           
        if (pPioData->pio_state & (1UL << PIO_WATER_LEVEL))
        {   
            p_cf_data[1] = app_status_water_level_full; 
            HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                             p_cf_data);
            DebugWriteString("Full\n");
            /* At this point the button is released */

            g_cur_button_state_water = button_state_up;
        }
        else
        {
            /* If PIO was LOW when this event was generated (that means
             * button was pressed, generating a falling edge causing
             * PIO to go from HIGH to LOW)
             */
             
              /* If last recorded state of button was up */
              if (g_cur_button_state_water == button_state_up)
              {         
                    p_cf_data[1] = app_status_water_level_empty;     
                    HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                             p_cf_data);
                    DebugWriteString("Empty\n");
                       
                    /* At this point the button has been pressed */
                    g_cur_button_state_water = button_state_down;
               }
         }
    
    }
    
    if(pPioData->pio_cause & (1UL << PIO_GLASS_POSITION)){
        /* If PIO was HIGH when this event was generated (that means
         * button was released, generating a rising edge causing PIO
         * to go from LOW to HIGH)
         */
        cf_length = 0x02;
         p_cf_data[0] = cf_control_point_glass_position;
             
         if (pPioData->pio_state & (1UL << PIO_GLASS_POSITION))
         {
             p_cf_data[1] = app_status_glass_not_positioned; 
             HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                         p_cf_data);
             DebugWriteString("Glass not positioned\n");
             /* At this point the button is released */
             g_cur_button_state_glass = button_state_up;
         }
         else
         {
            /* If PIO was LOW when this event was generated (that means
             * button was pressed, generating a falling edge causing
             * PIO to go from HIGH to LOW)
             */
                
            /* If last recorded state of button was up */
            if (g_cur_button_state_glass == button_state_up)
            {
                   p_cf_data[1] = app_status_glass_positioned;     
                   HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                         p_cf_data);
                   DebugWriteString("Glass positioned\n");
                   /* At this point the button has been pressed */
                   g_cur_button_state_glass = button_state_down;
             }
          }
       }
    
       if (pPioData->pio_cause & (1UL << PIO_ECHO))
    	{				
    		/* If PIO was HIGH when this event was generated (that means
    		 * button was released, generating a rising edge causing PIO
			 * to go from LOW to HIGH)
			 */
            cf_length = 0x03;
 			if (pPioData->pio_state & (1UL << PIO_ECHO))
			{ 
                    initTime = TimeGet32();
					g_cur_button_state = button_state_up;
                        
			}
			else
            {
				/* If PIO was LOW when this event was generated (that means
				 * button was pressed, generating a falling edge causing
				 * PIO to go from HIGH to LOW)
				 */
				  
                    
				/* If last recorded state of button was up */
				if (g_cur_button_state == button_state_up)
				{
					   /* At this point the button has been pressed */
						g_cur_button_state = button_state_down;
                        
                        finalTime = TimeGet32();
                        result = result + (finalTime - initTime);
                        cont = cont + 1;
                        if(cont > 5){
                            result = result/6;
                            uint8 b = 0x0F & result;
                            uint8 c = 0x0F & (result >> 8);
                            
                            p_cf_data[2] = c;                        
                            p_cf_data[1] = b;
                            p_cf_data[0] = cf_control_point_level_coffee;
                                
                            HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                             p_cf_data);
                            result = 0;
                            cont = 0;
                        }else{
                            sendCoffeeLevel();                              
                        }
                        
                                     					   
                        
				}
                
			}
		}    
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This function is called just after a power-on reset (including after
 *      a firmware panic).
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the reset() function.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
extern void AppPowerOnReset(void)
{
    /* Configure the application constants */
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This function is called after a power-on reset (including after a
 *      firmware panic) or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after app_power_on_reset.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppInit(sleep_state last_sleep_state)
{ 
    uint16 gatt_db_length = 0;
    uint16 *p_gatt_db_pointer = NULL;  
    
    /* Set LED0 and LED1 to be controlled directly via PioSet */
    PioSetModes(( 1UL << PIO_MAKE_COFFEE ) | ( 1UL << PIO_ONOFF ), pio_mode_user );

    /* Configure button to be controlled directly */
    PioSetMode(PIO_WATER_LEVEL, pio_mode_user);
     /* Configure button to be controlled directly */
    PioSetMode(PIO_GLASS_POSITION, pio_mode_user);
    /* Set TRIGGER to be controlled directly via PioSet */
    PioSetModes((1UL << PIO_TRIGGER), pio_mode_user);
        /* Configure ECHO to be controlled directly */
    PioSetMode(PIO_ECHO, pio_mode_user);   
    
    /* Configure LED0 and LED1 to be outputs */
    PioSetDir( PIO_MAKE_COFFEE, PIO_DIR_OUTPUT );
    PioSetDir( PIO_ONOFF, PIO_DIR_OUTPUT );
    /* Configure TRIGGER to be output */  
    PioSetDir(PIO_TRIGGER, PIO_DIR_OUTPUT);
      /* Configure ECHO to be input */  
    PioSetDir(PIO_ECHO, PIO_DIR_INPUT);  
    
       /* Configure button to be input */
    PioSetDir(PIO_WATER_LEVEL, PIO_DIR_INPUT);
           /* Configure button to be input */
    PioSetDir(PIO_GLASS_POSITION, PIO_DIR_INPUT);
    
    /* Set weak pull up on button PIO, in order not to draw too much current
     * while button is pressed
     */
    PioSetPullModes((1UL << PIO_WATER_LEVEL), pio_mode_strong_pull_up);
     /* Set weak pull up on button PIO, in order not to draw too much current
     * while button is pressed
     */
    PioSetPullModes((1UL << PIO_ECHO), pio_mode_strong_pull_down ); 
    
    PioSet( PIO_ONOFF, 0x1 );
    PioSet( PIO_MAKE_COFFEE, 0x1 );
    PioSet(PIO_TRIGGER,0x1);
    
    /* Set the button to generate sys_event_pio_changed when pressed as well
     * as released
     */
    PioSetEventMask((1UL << PIO_WATER_LEVEL), pio_event_mode_both);
    /* Set the button to generate sys_event_pio_changed when pressed as well
     * as released
     */
    PioSetEventMask((1UL << PIO_ECHO), pio_event_mode_both);
      /* Set the button to generate sys_event_pio_changed when pressed as well
     * as released
     */
    PioSetEventMask((1UL << PIO_GLASS_POSITION), pio_event_mode_both);
    
    /* Initialise the application timers */
    TimerInit(MAX_APP_TIMERS, (void*)app_timers);

    /* initialise communications */
    DebugInit(1, UartDataRxCallback, NULL);

    DebugWriteString("AppInit\n");
    
    InitStateButtons();
    
    /* Initialise GATT entity */
    GattInit();
    
    /* Install GATT Server support for the optional Write procedure
     * This is mandatory only if control point characteristic is supported. 
     */
    GattInstallServerWrite();

    /* Don't wakeup on UART RX line */
    SleepWakeOnUartRX(FALSE);

    /* Initialise NVM for I2C EEPROM */
    NvmConfigureI2cEeprom ();
    Nvm_Disable();

    HRInitChipReset();

    /* Battery Initialisation on Chip reset */
    BatteryInitChipReset();

    /* Initialize the gap data. Needs to be done before readPersistentStore */
    GapDataInit();

    /* Read persistent storage */
    readPersistentStore();

    /* Tell Security Manager module about the value it needs to initialize it's
     * diversifier to.
     */
    SMInit(g_hr_data.diversifier);

    /* Initialize the Heart Rate Sensor application data structure */
    hrSensorDataInit();

    /* Initialise Heart Rate Sensor H/W */
    HrInitHardware();

    /* Tell GATT about our database. We will get a GATT_ADD_DB_CFM event when
     * this has completed.
     */
    p_gatt_db_pointer = GattGetDatabase(&gatt_db_length);

    /* Initialise HR Sensor State */
    g_hr_data.state = app_state_init;

    GattAddDatabaseReq(gatt_db_length, p_gatt_db_pointer);
    

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void AppProcessSystemEvent(sys_event_id id, void *data)
{  
    
    switch(id)
    {
        case sys_event_battery_low:
        {
            /* Battery low event received - notify the connected host. If 
             * not connected, the battery level will get notified when 
             * device gets connected again
             */
            if(g_hr_data.state == app_state_connected)
            {
                BatteryUpdateLevel(g_hr_data.st_ucid);
            }
        }
        break;

        case sys_event_pio_changed:
            getCoffeeLevels(data);
        break;

        default:
            /* Ignore anything else */
        break;
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event is
 *      received by the system.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern bool AppProcessLmEvent(lm_event_code event_code, 
                              LM_EVENT_T *p_event_data)
{
    
    
    switch(event_code)
    {

        /* Handle events received from Firmware */

        case GATT_ADD_DB_CFM:
            /* Attribute database registration confirmation */
            handleSignalGattAddDBCfm((GATT_ADD_DB_CFM_T*)p_event_data);
        break;

        case GATT_CANCEL_CONNECT_CFM:
            /* Confirmation for the completion of GattCancelConnectReq()
             * procedure 
             */
            handleSignalGattCancelConnectCfm();
        break;
        
        case GATT_CONNECT_CFM:
            /* Confirmation for the completion of GattConnectReq() 
             * procedure
             */
            handleSignalGattConnectCfm((GATT_CONNECT_CFM_T*)p_event_data);
        break;

        case SM_KEYS_IND:
            /* Indication for the keys and associated security information
             * on a connection that has completed Short Term Key Generation 
             * or Transport Specific Key Distribution
             */
            handleSignalSmKeysInd((SM_KEYS_IND_T*)p_event_data);
        break;

        case SM_SIMPLE_PAIRING_COMPLETE_IND:
            /* Indication for completion of Pairing procedure */
            handleSignalSmSimplePairingCompleteInd(
                (SM_SIMPLE_PAIRING_COMPLETE_IND_T*)p_event_data);
        break;

        case LM_EV_ENCRYPTION_CHANGE:
            /* Indication for encryption change event */
            handleSignalLMEncryptionChange(
            (HCI_EV_DATA_ENCRYPTION_CHANGE_T *)&p_event_data->enc_change.data);
        break;

        case SM_DIV_APPROVE_IND:
            /* Indication for SM Diversifier approval requested by F/W when 
             * the last bonded host exchange keys. Application may or may not
             * approve the diversifier depending upon whether the application 
             * is still bonded to the same host
             */
            handleSignalSmDivApproveInd((SM_DIV_APPROVE_IND_T *)p_event_data);
        break;

        /* Received in response to the LsConnectionParamUpdateReq() 
         * request sent from the slave after encryption is enabled. If 
         * the request has failed, the device should again send the same 
         * request only after Tgap(conn_param_timeout). Refer Bluetooth 4.0 
         * spec Vol 3 Part C, Section 9.3.9 and HID over GATT profile spec 
         * section 5.1.2.
         */
        case LS_CONNECTION_PARAM_UPDATE_CFM:
            handleSignalLsConnParamUpdateCfm((LS_CONNECTION_PARAM_UPDATE_CFM_T*)
                p_event_data);
        break;

        case LS_CONNECTION_PARAM_UPDATE_IND:
            /* Indicates completion of remotely triggered Connection 
             * parameter update procedure
             */
            handleSignalLsConnParamUpdateInd(
                            (LS_CONNECTION_PARAM_UPDATE_IND_T *)p_event_data);
        break;

        case GATT_ACCESS_IND:
            /* Indicates that an attribute controlled directly by the
             * application (ATT_ATTR_IRQ attribute flag is set) is being 
             * read from or written to.                  
             */
                
            /*if(*((GATT_ACCESS_IND_T*)p_event_data)->value == 61)*/
            DebugWriteString("Message received\n");
            handleSignalGattAccessInd((GATT_ACCESS_IND_T*)p_event_data);
            
        break;

        case GATT_DISCONNECT_IND:
            /* Disconnect procedure triggered by remote host or due to 
             * link loss is considered complete on reception of 
             * LM_EV_DISCONNECT_COMPLETE event. So, it gets handled on 
             * reception of LM_EV_DISCONNECT_COMPLETE event.
             */
         break;

        case GATT_DISCONNECT_CFM:
            /* Confirmation for the completion of GattDisconnectReq()
             * procedure is ignored as the procedure is considered complete 
             * on reception of LM_EV_DISCONNECT_COMPLETE event. So, it gets 
             * handled on reception of LM_EV_DISCONNECT_COMPLETE event.
             */
        break;

        case LM_EV_DISCONNECT_COMPLETE:
        {
            /* Disconnect procedures either triggered by application or remote
             * host or link loss case are considered completed on reception 
             * of LM_EV_DISCONNECT_COMPLETE event
             */
             handleSignalLmDisconnectComplete(
                    &((LM_EV_DISCONNECT_COMPLETE_T *)p_event_data)->data);
        }
        break;
        
        default:
            /* Ignore any other event */ 
    
        break;

    }

    return TRUE;
}
