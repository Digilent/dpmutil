/************************************************************************/
/*                                                                      */
/*  ZmodDAC.c - ZmodDAC functions implementation                        */
/*                                                                      */
/************************************************************************/
/*  Author: Michael T. Alexander                                        */
/*  Copyright 2020, Digilent Inc.                                       */
/************************************************************************/
/*  Module Description:                                                 */
/*                                                                      */
/*  This source file contains the implementation of functions that can  */
/*  be used to calculate and display the calibration constants          */
/*  associated with a Zmod DAC.                                         */
/*                                                                      */
/*  Note: the code for calculating the coefficients was provided by     */
/*  Tudor Gherman.                                                      */
/*                                                                      */
/************************************************************************/
/*  Revision History:                                                   */
/*                                                                      */
/*  01/13/2020 (MichaelA): created                                      */
/*  01/14/2020 (MichaelA): modified the way that coefficients are       */
/*      displayed based on feedback                                     */
/*  01/15/2020 (MichaelA): modified ComputeMultCoefDAC1411 and          */
/*      ComputeAddCoefDAC1411 to truncate to 18-bits for consistency    */
/*      with the ADC1410 conversion functions                           */
/*  01/15/2020 (MichaelA): modified FDisplayZmodDACCal to display the   */
/*      raw calibration constants as retrieved from flash               */
/*                                                                      */
/************************************************************************/

/* ------------------------------------------------------------ */
/*              Include File Definitions                        */
/* ------------------------------------------------------------ */

#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/i2c-dev.h>
#endif
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "stdtypes.h"
#include "syzygy.h"
#include "ZmodDAC.h"

/* ------------------------------------------------------------ */
/*              Miscellaneous Declarations                      */
/* ------------------------------------------------------------ */

#define DAC1411_IDEAL_RANGE_DAC_HIGH    5.0
#define DAC1411_IDEAL_RANGE_DAC_LOW     1.25
#define DAC1411_REAL_RANGE_DAC_HIGH     5.32
#define DAC1411_REAL_RANGE_DAC_LOW      1.33

/* ------------------------------------------------------------ */
/*              Local Type Definitions                          */
/* ------------------------------------------------------------ */


/* ------------------------------------------------------------ */
/*              Global Variables                                */
/* ------------------------------------------------------------ */

extern BOOL dpmutilfVerbose;

/* ------------------------------------------------------------ */
/*              Local Variables                                 */
/* ------------------------------------------------------------ */


/* ------------------------------------------------------------ */
/*              Forward Declarations                            */
/* ------------------------------------------------------------ */

int32_t ComputeMultCoefDAC1411(float cg, BOOL fHighGain);
int32_t ComputeAddCoefDAC1411(float ca, float cg, BOOL fHighGain);

/* ------------------------------------------------------------ */
/*              Procedure Definitions                           */
/* ------------------------------------------------------------ */

/* ------------------------------------------------------------ */
/***    FDisplayZmodDACCal
**
**  Parameters:
**  	fdI2cDev        - open file descriptor for underlying I2C device (linux only)
**      addrI2cSlave    - I2C bus address for the slave
**
**  Return Value:
**      fTrue for success, fFalse otherwise
**
**  Errors:
**      none
**
**  Description:
**      This function reads the factory calibration and user calibration
**      areas from the ZmodDAC with the specified I2C bus address,
**      computes the multiplicative and additive coefficients, and then
**      displays them using stdout.
*/
BOOL
FDisplayZmodDACCal(int fdI2cDev, BYTE addrI2cSlave) {

    ZMOD_DAC_CAL    dacal;
    WORD            cbRead;
    time_t          t;
    struct tm       time;
    char            szDate[256];

    if ( ! SyzygyI2cRead(fdI2cDev, addrI2cSlave, addrDacFactCalStart, (BYTE*)&dacal, sizeof(ZMOD_DAC_CAL), &cbRead) ) {
        printf("Error: failed to read ZmodDAC factory calibration from 0x%02X\n", addrI2cSlave);
        printf("Error: received %d of %d bytes\n", cbRead, sizeof(ZMOD_DAC_CAL));
        return fFalse;
    }

    t = (time_t)dacal.date;
    localtime_r(&t, &time);
    if ( 0 != strftime(szDate, sizeof(szDate), "%B %d, %Y at %T", &time) ) {
        printf("\n    Factory Calibration:   %s\n", szDate);
    }

    printf("    CHAN_1_LG_GAIN:        %f\n", dacal.cal[0][0][0]);
    printf("    CHAN_1_LG_OFFSET:      %f\n", dacal.cal[0][0][1]);
    printf("    CHAN_1_HG_GAIN:        %f\n", dacal.cal[0][1][0]);
    printf("    CHAN_1_HG_OFFSET:      %f\n", dacal.cal[0][1][1]);
    printf("    CHAN_2_LG_GAIN:        %f\n", dacal.cal[1][0][0]);
    printf("    CHAN_2_LG_OFFSET:      %f\n", dacal.cal[1][0][1]);
    printf("    CHAN_2_HG_GAIN:        %f\n", dacal.cal[1][1][0]);
    printf("    CHAN_2_HG_OFFSET:      %f\n", dacal.cal[1][1][1]);

    printf("    Ch1LgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[0][0][0], fFalse));
    printf("    Ch1LgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[0][0][1], dacal.cal[0][0][0], fFalse));
    printf("    Ch1HgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[0][1][0], fTrue));
    printf("    Ch1HgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[0][1][1], dacal.cal[0][1][0], fTrue));
    printf("    Ch2LgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[1][0][0], fFalse));
    printf("    Ch2LgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[1][0][1], dacal.cal[1][0][0], fFalse));
    printf("    Ch2HgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[1][1][0], fTrue));
    printf("    Ch2HgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[1][1][1], dacal.cal[1][1][0], fTrue));

    if ( ! SyzygyI2cRead(fdI2cDev, addrI2cSlave, addrDacUserCalStart, (BYTE*)&dacal, sizeof(ZMOD_DAC_CAL), &cbRead) ) {
        printf("Error: failed to read ZmodDAC user calibration from 0x%02X\n", addrI2cSlave);
        printf("Error: received %d of %d bytes\n", cbRead, sizeof(ZMOD_DAC_CAL));
        return fFalse;
    }

    t = (time_t)dacal.date;
    localtime_r(&t, &time);
    if ( 0 != strftime(szDate, sizeof(szDate), "%B %d, %Y at %T", &time) ) {
        printf("\n    User Calibration:      %s\n", szDate);
    }

    printf("    CHAN_1_LG_GAIN:        %f\n", dacal.cal[0][0][0]);
    printf("    CHAN_1_LG_OFFSET:      %f\n", dacal.cal[0][0][1]);
    printf("    CHAN_1_HG_GAIN:        %f\n", dacal.cal[0][1][0]);
    printf("    CHAN_1_HG_OFFSET:      %f\n", dacal.cal[0][1][1]);
    printf("    CHAN_2_LG_GAIN:        %f\n", dacal.cal[1][0][0]);
    printf("    CHAN_2_LG_OFFSET:      %f\n", dacal.cal[1][0][1]);
    printf("    CHAN_2_HG_GAIN:        %f\n", dacal.cal[1][1][0]);
    printf("    CHAN_2_HG_OFFSET:      %f\n", dacal.cal[1][1][1]);

    printf("    Ch1LgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[0][0][0], fFalse));
    printf("    Ch1LgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[0][0][1], dacal.cal[0][0][0], fFalse));
    printf("    Ch1HgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[0][1][0], fTrue));
    printf("    Ch1HgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[0][1][1], dacal.cal[0][1][0], fTrue));
    printf("    Ch2LgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[1][0][0], fFalse));
    printf("    Ch2LgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[1][0][1], dacal.cal[1][0][0], fFalse));
    printf("    Ch2HgCoefMultStatic:   0x%05X\n", (unsigned int)ComputeMultCoefDAC1411(dacal.cal[1][1][0], fTrue));
    printf("    Ch2HgCoefAddStatic:    0x%05X\n", (unsigned int)ComputeAddCoefDAC1411(dacal.cal[1][1][1], dacal.cal[1][1][0], fTrue));

    return fTrue;
}

/* ------------------------------------------------------------ */
/***    ComputeMultCoefDAC1411
**
**  Parameters:
**      cg              - gain coefficient from ZmodDAC flash memory
**      fHighGain       - fTrue for high gain setting, fFalse for low gain
**                        read
**
**  Return Value:
**      signed 32-bit value containing the multiplicative coefficient in the
**      18 least significant bits: bit 17 is the sign, bits 16:0 are the value
**
**  Errors:
**      none
**
**  Description:
**      This function computes a signed 18-bit value corresponding to the
**      multiplicative calibration coefficient of the ZmodDAC.
*/
int32_t
ComputeMultCoefDAC1411(float cg, BOOL fHighGain) {

    float   fval;
    int32_t ival;

    fval = (fHighGain ? (DAC1411_IDEAL_RANGE_DAC_HIGH/DAC1411_REAL_RANGE_DAC_HIGH):(DAC1411_IDEAL_RANGE_DAC_LOW/DAC1411_REAL_RANGE_DAC_LOW))/(1 + cg)*(float)(1<<16); // extra 14 positions so that the sign is on 31 position instead of 17
    ival = (int32_t)(fval + 0.5); // round
    ival &= (1<<18) - 1; // keep only 18 bits

    return ival;
}

/* ------------------------------------------------------------ */
/***    ComputeAddCoefDAC1411
**
**  Parameters:
**      ca              - additive coefficient from ZmodDAC flash memory
**      cg              - gain coefficient from ZmodDAC flash memory
**      fHighGain       - fTrue for high gain setting, fFalse for low gain
**                        read
**
**  Return Value:
**      signed 32-bit value containing the additive coefficient in the 18
**      least significant bits: bit 17 is the sign, bits 16:0 are the value
**
**  Errors:
**      none
**
**  Description:
**      This function computes a signed 18-bit value corresponding to the
**      additive calibration coefficient of the ZmodDAC.
*/
int32_t
ComputeAddCoefDAC1411(float ca, float cg, BOOL fHighGain) {

    float   fval;
    int32_t ival;

    fval = -ca * (float)(uint32_t)(1<<17) / ((fHighGain ? DAC1411_REAL_RANGE_DAC_HIGH:DAC1411_REAL_RANGE_DAC_LOW) * (1 + cg));
    ival = (int32_t)(fval + 0.5); // round
    ival &= (1<<18) - 1; // keep only 18 bits

    return ival;
}

