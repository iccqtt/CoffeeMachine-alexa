/******************************************************************************
 *  Copyright (C) Cambridge Silicon Radio Limited 2012
 *  Part of uEnergy SDK 2.0.0
 *  2.0.0.0
 *
 *  FILE
 *      nvm_access.c
 *
 *  DESCRIPTION
 *      This file defines routines used by application to access NVM.
 *
 *****************************************************************************/

/*============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <nvm.h>
#include <i2c.h>
#include <panic.h>

/*============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "nvm_access.h"
#include "app_gatt.h"

/*============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Disable
 *
 *  DESCRIPTION
 *      This function is used to perform things necessary to save power on NVM 
 *      once the read/write operations are done.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void Nvm_Disable(void)
{
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down);
    NvmDisable();
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Read
 *
 *  DESCRIPTION
 *      Read words from the NVM Store after preparing the NVM to be readable. 
 *      After the read operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Read words starting at the word offset, and store them in the supplied
 *      buffer.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/

extern void Nvm_Read(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;

    /* Read from NVM. Firmware re-enables the NVM if it is disabled */
    result = NvmRead(buffer, length, offset);

    /* Disable NVM to save power after read operation */
    Nvm_Disable();

    /* Report panic is NVM read is not successful */
    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_read);
    }

}


/*----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Write
 *
 *  DESCRIPTION
 *      Write words to the NVM store after preparing the NVM to be writable. 
 *      After the write operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Write words from the supplied buffer into the NVM Store, starting at the
 *      given word offset.
 *
 *  RETURNS
 *      Nothing
 *
 *---------------------------------------------------------------------------*/

extern void Nvm_Write(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;

    /* Write to NVM. Firmware re-enables the NVM if it is disabled */
    result = NvmWrite(buffer, length, offset);

    /* Disable NVM to save power after write operation */
    Nvm_Disable();

    /* Report panic is NVM write is not successful */
    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_write);
    }

}
