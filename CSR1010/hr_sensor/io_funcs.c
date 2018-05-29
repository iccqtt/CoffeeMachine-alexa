#include <pio.h>
#include <timer.h>          /* Chip timer functions */
#include <panic.h>          /* Support for applications to panic */
#include <debug.h>          /* Simple host interface to the UART driver */
#include <ls_app_if.h>      /* Link Supervisor application interface */

#include "io_funcs.h"
#include "hr_sensor.h"
#include "heart_rate_service.h"

#define PIO_LED0        (21)        /* PIO connected to the LED0 on CSR1000 */
#define PIO_LED1        (23)         /* PIO connected to the LED1 on CSR1000 */

/* First timeout at which the timer has to fire a callback */
#define TIMER_TIMEOUT1 (1 * SECOND)

/* Start timer */
static void startTimer(uint32 timeout, timer_callback_arg handler);

/*Contador para cronometragem dos reles*/
static uint8 countTimer = 0;

/* Callback after first timeout */
static void timerCallback1(timer_id const id);

/* Callback after second timeout */
static void timerCallback2(timer_id const id);

/* Read the current system time and write to UART */
static void printCurrentTime(void);

/* Convert an integer value into an ASCII string and send to the UART */
static uint8 writeASCIICodedNumber(uint32 value);

/* HR Sensor application data instance */
HR_DATA_T g_hr_data;

/*============================================================================*
 *  Private Function Implementations
 *============================================================================*/

extern void ligaCafeteira(uint8 command)
{
   
    if (command == 0) {    
        /* Liga cafeteira */
        PioSet( PIO_LED1, 0x0 );
    } else {
        /* Desliga cafeteira */
        PioSet( PIO_LED1, 0x1 );
    }    
}

extern void startCafeCurto(void)
{
    countTimer = 0;
    
     /* Set LED0 according to bit 0 of desired pattern */
    PioSet( PIO_LED0, 0x0 );
    
    /* Start the first timer */
    startTimer(TIMER_TIMEOUT1, timerCallback1);
    
    
}

extern void startCafeLongo(void)
{
    countTimer = 0;    
    
    /* Set LED0 according to bit 1 of desired pattern */
    PioSet( PIO_LED0, 0x0 );
    
    /* Start the first timer */
    startTimer(TIMER_TIMEOUT1, timerCallback2);
}

static void startTimer(uint32 timeout, timer_callback_arg handler)
{
    /* Now starting a timer */
    const timer_id tId = TimerCreate(timeout, TRUE, handler);
    
    /* If a timer could not be created, panic to restart the app */
    if (tId == TIMER_INVALID)
    {
        DebugWriteString("\r\nFailed to start timer");
        
        /* Panic with panic code 0xfe */
        Panic(0xfe);
    }
}

static void timerCallback1(timer_id const id)
{
    countTimer = countTimer + 1;
   
    /* Report current system time */
    printCurrentTime();
    
    if(countTimer < 8) {    
        /* Now restart the timer for second callback */
        startTimer( TIMER_TIMEOUT1, timerCallback1 );
    } else {
        uint8   cf_length = 0x02;
        uint8* p_cf_data = g_hr_data.hr_meas_data;
        p_cf_data[0] = cf_control_point_short_coffee;
        p_cf_data[1] = app_status_ok;
        
        /* Set LED0 according to bit 0 of desired pattern */
        PioSet( PIO_LED0, 0x1 );

        DebugWriteString( "\r\n" );
        DebugWriteString( " ************FIM*****************" );
        HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                         p_cf_data);                       
    }
}

static void timerCallback2(timer_id const id)
{
    countTimer = countTimer + 1;
    
    /* Report current system time */
    printCurrentTime();
    
    if(countTimer < 16) {    
        /* Now restart the timer for second callback */
        startTimer( TIMER_TIMEOUT1, timerCallback2 );
    } else {
        uint8   cf_length = 0x02;
        uint8* p_cf_data = g_hr_data.hr_meas_data;
        p_cf_data[0] = cf_control_point_long_coffee;
        p_cf_data[1] = app_status_ok;
        
        /* Set LED0 according to bit 1 of desired pattern */
        PioSet( PIO_LED0, 0x1 );
        DebugWriteString( "\r\n" );
        DebugWriteString( " ************FIM*****************" );
        HeartRateSendMeasValue(g_hr_data.st_ucid, cf_length, 
                                                         p_cf_data);        
    }
}

static void printCurrentTime(void)
{
    /* Read current system time */
    const uint32 now = TimeGet32();
    
    /* Report current system time */
    DebugWriteString("\n\nCurrent system time: ");
    writeASCIICodedNumber(now / MINUTE);
    DebugWriteString("m ");
    writeASCIICodedNumber((now % MINUTE)/SECOND);
    DebugWriteString("s\r\n");
}

static uint8 writeASCIICodedNumber(uint32 value)
{
#define BUFFER_SIZE 11          /* Buffer size required to hold maximum value */
    
    uint8  i = BUFFER_SIZE;     /* Loop counter */
    uint32 remainder = value;   /* Remaining value to send */
    char   buffer[BUFFER_SIZE]; /* Buffer for ASCII string */

    /* Ensure the string is correctly terminated */    
    buffer[--i] = '\0';
    
    /* Loop at least once and until the whole value has been converted */
    do
    {
        /* Convert the unit value into ASCII and store in the buffer */
        buffer[--i] = (remainder % 10) + '0';
        
        /* Shift the value right one decimal */
        remainder /= 10;
    } while (remainder > 0);

    /* Send the string to the UART */
    DebugWriteString(buffer + i);
    
    /* Return length of ASCII string sent to UART */
    return (BUFFER_SIZE - 1) - i;
}