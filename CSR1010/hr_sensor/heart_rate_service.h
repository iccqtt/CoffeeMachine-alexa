/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012
 *  Part of uEnergy SDK 2.0.0
 *  2.0.0.0
 *
 *  FILE
 *      heart_rate_service.h
 *
 *  DESCRIPTION
 *      Header definitions for heart rate service
 *
 ******************************************************************************/

#ifndef __HEART_RATE_SERVICE_H__
#define __HEART_RATE_SERVICE_H__

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"

/*============================================================================*
 *  Public Definitions
 *============================================================================*/

/* HEART RATE MEASUREMENT FLAGS */

#define SENSOR_MEASUREVAL_FORMAT_UINT8   (0x00)
#define SENSOR_MEASUREVAL_FORMAT_UINT16  (0x01)

/* Sensor contact bit */

/* Sensor contact feature support (bit 2 of the flag) */
#define SENSOR_CONTACT_FEATURE_SUPPORTED (0x04)

/* Sensor contact bits when feature is supported (bit 1 - sensor contact bit)*/
#define SENSOR_NOT_IN_CONTACT \
                                       (SENSOR_CONTACT_FEATURE_SUPPORTED | 0x00)
#define SENSOR_IN_CONTACT\
                                       (SENSOR_CONTACT_FEATURE_SUPPORTED | 0x02)

/* Energy expended status bit (Bit 3 of the flag) */
#define ENERGY_EXP_AVAILABLE            (0x08)

/* RR-Interval status bit (Bit 4 of the flag) */
#define RR_INTERVAL_PRESENT             (0x10)

/* Energy expended is sent at every 10th measurement as per 
 * HR profile version 1.0 recommendation
 */
#define HR_MEAS_ENERGY_EXP_PERIOD       (10)

/* As per section 3.1.1.3 of HR service spec ver 1.0, maximum value of 
 * Energy expended characteristic is 65535 (0xFFFF) Kile Joules
 */
#define MAX_ENERGY_EXPENDED_IN_KJOULES  (0xFFFF)

typedef enum
{
    cf_control_point_turn_off = 0x00,
    cf_control_point_turn_on = 0x01,
    cf_control_point_short_coffee = 0x02,
    cf_control_point_long_coffee = 0x03,
    cf_control_point_level_water = 0x04,
    cf_control_point_level_coffee = 0x05,
    cf_control_point_glass_position = 0x06,    
    cf_control_point_update = 0x07
    
} cf_control_point;

typedef enum
{
    app_status_on = 1,
    app_status_off = 0,    
    app_status_coffee_machine_being_used = 2,
    app_status_doing_coffee = 3,
    app_status_ok = 1,
    app_status_water_level_empty = 0,
    app_status_water_level_full = 1,
    app_status_glass_positioned = 1,
    app_status_glass_not_positioned = 0
    
}app_status;

/*============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function is used to initialise heart rate service data structure.*/
extern void HRDataInit(void);

/* This function is used to initialise heart rate service data structure at 
 * chip reset
 */
extern void HRInitChipReset(void);

/* This function returns whether notifications are enabled for heart rate 
 * measurement characteristic
 */
extern bool IsHeartRateNotifyEnabled(void);

/* This function increments energy expended value for every HR measurement */
extern void HeartRateIncEnergyExpended(uint16 energy_exp);

/* This function returns energy expended value */
extern uint16 HeartRateGetEnergyExpended(void);

/* This function is used to read heart rate service specific data store in 
 * NVM
 */
extern void HeartRateReadDataFromNVM(bool nvm_fresh_start, uint16 *p_offset);

/* This functions sends the heart rate measurement value to the connected 
 * client
 */
extern void HeartRateSendMeasValue(uint16 ucid, uint8 hrm_length, 
                        uint8 *p_hr_meas);

/* This function handles read operation on heart rate service attributes
 * maintained by the application
 */
extern void HeartRateHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operation on heart rate service attributes 
 * maintained by the application
 */
extern void HeartRateHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function is used to check if the handle belongs to the heart rate 
 * service
 */
extern bool HeartRateCheckHandleRange(uint16 handle);

/* This function is used by application to notify bonding status to 
 * heart rate service
 */
extern void HeartRateBondingNotify(void);


#endif /* __HEART_RATE_SERVICE_H__ */
