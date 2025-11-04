/******************************************************************************
* \file cy_imagesensor.c
* \version 1.0
*
* Implements Camera image sensor init part to strean video from Sony IMX715
* or THine camera. default 4K resolution is set during configuration.
*
*******************************************************************************
* \copyright
* (c) (2021-2025), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
******************************************************************************/
/******************/
/* Include Files  */
/******************/
#include "cy_imagesensor.h"
#include "cy_usb_app.h"
#include "cy_usb_i2c.h"
#include "cy_usb_uvc_device.h"

/******************/
/* Global variable */
/******************/
extern bool glIsSensorConfigured;

#if SONY_IMX715_EN
const cy_stc_sensor_config_t Cy_ImageSensorInitArray_SonyIMX_4k[] = {
/******************************************************************************
* IMX715-AAQR1 or IMX415 2/2-line binning CSI-2_2lane 27MHz AD:10bit
*  Output:12bit 594Mbps Master Mode 21.002fps Integration Time 47.457ms
* Tool ver : Ver1.0
******************************************************************************/
        {0x3000, 0x01}, // Enter standby mode
        {0x3008, 0x5D},  // BCWAIT_TIME[9:0]  //74.25 {0x3008, 0xFF},  //27    {0x3008, 0x5D},
        {0x300A, 0x42},  // CPWAIT_TIME[9:0]  //74.25 {0x300A, 0xB6},  //74.25 {0x300A, 0x42},
        {0x301C, 0x00},  // WINMODE[3:0]
        {0x3022, 0x00},  // ADDMODE[1:0]

        {0x3024, 0xCA},  // VMAX[19:0]
        {0x3025, 0x08},  //
        {0x3028, 0x26},  // HMAX[15:0] 4C
        {0x3029, 0x02},  // 7D0 04

        {0x3030, 0x02},  //HREVERSE[0], VREVERSE[1]; 0 Normal, 1 Inverted//0x01

        {0x3031, 0x01},  // ADBIT[1:0] //0x00
        {0x3032, 0x01},  // MDBIT[1:0]
        {0x3033, 0x04},  // SYS_MODE[3:0]         //74.25   {0x3033, 0x07},    //27     {0x3033, 0x07},

        {0x3050, 0x4B},  // SHR0[19:0]
        {0x3051, 0x00},  // SHR0[19:0]
        {0x3052, 0x00},  // SHR0[19:0]

        {0x30C1, 0x00},  // XVS_DRV[1:0]

        {0x30CC, 0x00},  // XVS_LNG[1:0] [5:4] //1h: 0x08 2h:0x10 3h : 0x18

        {0x30CD, 0x00},  // XHS_LNG[1:0] [5:4]

        //Colour pattern Generator settings start
#if SONY_SENSOR_COLORBAR_EN
        {0x30E2, 0x00},  // BLKLEVEL [7:0] // for PG mode set to "000h"
        {0x30E3, 0x00},  // BLKLEVEL [1:0]

        {0x30E4, 0x01},  // TPG_EN_DUOUT [0]

        //0h: All 000h Pattern
        //1h: All FFFh Pattern
        //2h: All 555h Pattern
        //3h: All AAAh Pattern
        //4h: 555h / AAAh Toggle Pattern
        //5h: AAAh / 555h Toggle Pattern
        //6h: 000h / 555h Toggle Pattern
        //7h: 555h / 000h Toggle Pattern
        //8h: 000h / FFFh Toggle Pattern
        //9h: FFFh / 000h Toggle Pattern
        //Ah: Horizontal Color-bar Chart
        //Bh: Vertical Color-bar Chart
        {0x30E6, 0x0A},  // TPG_PATSEL_DUOUT [4:0] ( values 0h to Bh)

        {0x30E8, 0x00},  // TPG_COLORWIDTH [1:0](0 :120, 1:240, 2:480) pxl

        {0x3110, 0x20},  // TESTCLKEN_MIPI [5]

        {0x32C8, 0x00},  // DIG_CLP_MODE [0]

        {0x3390, 0x00},  // WRJ_OPEN [0]
#endif
        //Colour pattern Generator settings Ends

        {0x3116, 0x23},  // INCKSEL2[7:0]     //74.25 {0x3116, 0x28},        //27   {0x3116, 0x23},
        {0x3118, 0xC6},  // INCKSEL3[10:0]    //74.25 {0x3118, 0x80},        //27   {0x3118, 0x84},
        {0x311A, 0xE7},  // INCKSEL4[10:0]    //74.25 {0x311A, 0xE0},        //27   {0x311A, 0xE7},
        {0x311E, 0x23},  // INCKSEL5[7:0]     //74.25 {0x311E, 0x28},        //27   {0x311E, 0x23},
        {0x32D4, 0x21},  // -
        {0x32EC, 0xA1},  // -
        {0x344C, 0x2B},  // -
        {0x344D, 0x01},  // -
        {0x344E, 0xED},  // -
        {0x344F, 0x01},  // -
        {0x3450, 0xF6},  // -
        {0x3451, 0x02},  // -
        {0x3452, 0x7F},  // -
        {0x3453, 0x03},  // -
        {0x358A, 0x04},  // -
        {0x35A1, 0x02},  // -
        {0x35EC, 0x27},  // -
        {0x35EE, 0x8D},  // -
        {0x35F0, 0x8D},  // -
        {0x35F2, 0x29},  // -
        {0x36BC, 0x0C},  // -
        {0x36CC, 0x53},  // -
        {0x36CD, 0x00},  // -
        {0x36CE, 0x3C},  // -
        {0x36D0, 0x8C},  // -
        {0x36D1, 0x00},  // -
        {0x36D2, 0x71},  // -
        {0x36D4, 0x3C},  // -
        {0x36D6, 0x53},  // -
        {0x36D7, 0x00},  // -
        {0x36D8, 0x71},  // -
        {0x36DA, 0x8C},  // -
        {0x36DB, 0x00},  // -
        {0x3701, 0x01},  // ADBIT1[7:0]            //0x00
        {0x3720, 0x00},  // -
        {0x3724, 0x02},  // -
        {0x3726, 0x02},  // -
        {0x3732, 0x02},  // -
        {0x3734, 0x03},  // -
        {0x3736, 0x03},  // -
        {0x3742, 0x03},  // -
        {0x3862, 0xE0},  // -
        {0x38CC, 0x30},  // -
        {0x38CD, 0x2F},  // -
        {0x395C, 0x0C},  // -
        {0x39A4, 0x07},  // -
        {0x39A8, 0x32},  // -
        {0x39AA, 0x32},  // -
        {0x39AC, 0x32},  // -
        {0x39AE, 0x32},  // -
        {0x39B0, 0x32},  // -
        {0x39B2, 0x2F},  // -
        {0x39B4, 0x2D},  // -
        {0x39B6, 0x28},  // -
        {0x39B8, 0x30},  // -
        {0x39BA, 0x30},  // -
        {0x39BC, 0x30},  // -
        {0x39BE, 0x30},  // -
        {0x39C0, 0x30},  // -
        {0x39C2, 0x2E},  // -
        {0x39C4, 0x2B},  // -
        {0x39C6, 0x25},  // -
        {0x3A42, 0xD1},  // -
        {0x3A4C, 0x77},  // -
        {0x3AE0, 0x02},  // -
        {0x3AEC, 0x0C},  // -
        {0x3B00, 0x2E},  // -
        {0x3B06, 0x29},  // -
        {0x3B98, 0x25},  // -
        {0x3B99, 0x21},  // -
        {0x3B9B, 0x13},  // -
        {0x3B9C, 0x13},  // -
        {0x3B9D, 0x13},  // -
        {0x3B9E, 0x13},  // -
        {0x3BA1, 0x00},  // -
        {0x3BA2, 0x06},  // -
        {0x3BA3, 0x0B},  // -
        {0x3BA4, 0x10},  // -
        {0x3BA5, 0x14},  // -
        {0x3BA6, 0x18},  // -
        {0x3BA7, 0x1A},  // -
        {0x3BA8, 0x1A},  // -
        {0x3BA9, 0x1A},  // -
        {0x3BAC, 0xED},  // -
        {0x3BAD, 0x01},  // -
        {0x3BAE, 0xF6},  // -
        {0x3BAF, 0x02},  // -
        {0x3BB0, 0xA2},  // -
        {0x3BB1, 0x03},  // -
        {0x3BB2, 0xE0},  // -
        {0x3BB3, 0x03},  // -
        {0x3BB4, 0xE0},  // -
        {0x3BB5, 0x03},  // -
        {0x3BB6, 0xE0},  // -
        {0x3BB7, 0x03},  // -
        {0x3BB8, 0xE0},  // -
        {0x3BBA, 0xE0},  // -
        {0x3BBC, 0xDA},  // -
        {0x3BBE, 0x88},  // -
        {0x3BC0, 0x44},  // -
        {0x3BC2, 0x7B},  // -
        {0x3BC4, 0xA2},  // -
        {0x3BC8, 0xBD},  // -
        {0x3BCA, 0xBD},  // -
        {0x4000, 0x11},
        {0x4001, 0x03},  // LANEMODE[2:0]
        {0x4004, 0xC0},  // TXCLKESC_FREQ[15:0]  //74.25 {0x4004, 0x90},      //27 {0x4004, 0xC0},
        {0x4005, 0x06},  //                      //74.25 {0x4005, 0x12},      //27 {0x4005, 0x06},
        {0x400C, 0x01},  // INCKSEL6             //74.25 {0x300A, 0x00},      //27 {0x300A, 0x00},
        {0x4018, 0xB7},  // TCLKPOST[15:0]
        {0x401A, 0x67},  // TCLKPREPARE[15:0]
        {0x401C, 0x6F},  // TCLKTRAIL[15:0]
        {0x401E, 0xDF},  // TCLKZERO[15:0]
        {0x401F, 0x01},  //
        {0x4020, 0x6F},  // THSPREPARE[15:0]
        {0x4022, 0xCF},  // THSZERO[15:0]
        {0x4024, 0x6F},  // THSTRAIL[15:0]
        {0x4026, 0xB7},  // THSEXIT[15:0]
        {0x4028, 0x5F},  // TLPX[15:0]
        {0x4074, 0x00},  // INCKSEL7 [2:0]     //74.25 {0x4074, 0x01},       //27 {0x4074, 0x01},
        {0x3000, 0x00},  // Exit standby mode
};

/* Image sensor Configuration settings for Format : RAW12 Resolution : Resolution1  1944 x 1116 video stream  */
const cy_stc_sensor_config_t Cy_ImageSensorInitArray_SonyIMX_1080p[] = {
    /*
    IMX715-AAQR1 2/2-line binning CSI-2_2lane 27MHz AD:10bit Output:12bit 594Mbps Master Mode 21.002fps Integration Time 47.457ms
    Tool ver : Ver1.0
    */
        {0x3000, 0x01}, // Enter standby mode
        {0x3008, 0x5D},  // BCWAIT_TIME[9:0]
        {0x300A, 0x42},  // CPWAIT_TIME[9:0]
        {0x3020, 0x01},  // HADD
        {0x3021, 0x01},  // VADD
        {0x3022, 0x01},  // ADDMODE[1:0]

        {0x3024, 0xCA},  // VMAX[19:0]
        {0x3025, 0x08},  //
        {0x3028, 0x26},  // HMAX[15:0]
        {0x3029, 0x02},  //

        {0x3030, 0x00},  //HREVERSE[0], VREVERSE[1]; 0 Normal, 1 Inverted

        {0x3031, 0x01},  // ADBIT[1:0]
        {0x3032, 0x01},  // MDBIT[1:0]
        {0x3033, 0x07},  // SYS_MODE[3:0]

        {0x3050, 0x4B},  // SHR0[19:0]
        {0x3051, 0x00},  // SHR0[19:0]
        {0x3052, 0x00},  // SHR0[19:0]

        {0x30C1, 0x00},  // XVS_DRV[1:0]
        {0x30D9, 0x02},  // DIG_CLP_VSTART[4:0]
        {0x30DA, 0x01},  // DIG_CLP_VNUM[1:0]

        //Colour pattern Generator settings start
#if SONY_SENSOR_COLORBAR_EN
        {0x30E2, 0x00},  // BLKLEVEL [7:0] // for PG mode set to "000h"
        {0x30E3, 0x00},  // BLKLEVEL [1:0]

        {0x30E4, 0x01},  // TPG_EN_DUOUT [0]

        //0h: All 000h Pattern
        //1h: All FFFh Pattern
        //2h: All 555h Pattern
        //3h: All AAAh Pattern
        //4h: 555h / AAAh Toggle Pattern
        //5h: AAAh / 555h Toggle Pattern
        //6h: 000h / 555h Toggle Pattern
        //7h: 555h / 000h Toggle Pattern
        //8h: 000h / FFFh Toggle Pattern
        //9h: FFFh / 000h Toggle Pattern
        //Ah: Horizontal Color-bar Chart
        //Bh: Vertical Color-bar Chart
        {0x30E6, 0x0A},  // TPG_PATSEL_DUOUT [4:0] ( values 0h to Bh)

        {0x30E8, 0x00},  // TPG_COLORWIDTH [1:0](0 :120, 1:240, 2:480) pxl

        {0x3110, 0x20},  // TESTCLKEN_MIPI [5]

        {0x32C8, 0x00},  // DIG_CLP_MODE [0]

        {0x3390, 0x00},  // WRJ_OPEN [0]
#endif
        //Colour pattern Generator settings Ends

        {0x3116, 0x23},  // INCKSEL2[7:0]
        {0x3118, 0x84},  // INCKSEL3[10:0]
        {0x311A, 0xE7},  // INCKSEL4[10:0]
        {0x311E, 0x23},  // INCKSEL5[7:0]
        {0x32D4, 0x21},  // -
        {0x32EC, 0xA1},  // -
        {0x344C, 0x2B},  // -
        {0x344D, 0x01},  // -
        {0x344E, 0xED},  // -
        {0x344F, 0x01},  // -
        {0x3450, 0xF6},  // -
        {0x3451, 0x02},  // -
        {0x3452, 0x7F},  // -
        {0x3453, 0x03},  // -
        {0x358A, 0x04},  // -
        {0x35A1, 0x02},  // -
        {0x35EC, 0x27},  // -
        {0x35EE, 0x8D},  // -
        {0x35F0, 0x8D},  // -
        {0x35F2, 0x29},  // -
        {0x36BC, 0x0C},  // -
        {0x36CC, 0x53},  // -
        {0x36CD, 0x00},  // -
        {0x36CE, 0x3C},  // -
        {0x36D0, 0x8C},  // -
        {0x36D1, 0x00},  // -
        {0x36D2, 0x71},  // -
        {0x36D4, 0x3C},  // -
        {0x36D6, 0x53},  // -
        {0x36D7, 0x00},  // -
        {0x36D8, 0x71},  // -
        {0x36DA, 0x8C},  // -
        {0x36DB, 0x00},  // -
        {0x3701, 0x01},  // ADBIT1[7:0]
        {0x3720, 0x00},  // -
        {0x3724, 0x02},  // -
        {0x3726, 0x02},  // -
        {0x3732, 0x02},  // -
        {0x3734, 0x03},  // -
        {0x3736, 0x03},  // -
        {0x3742, 0x03},  // -
        {0x3862, 0xE0},  // -
        {0x38CC, 0x30},  // -
        {0x38CD, 0x2F},  // -
        {0x395C, 0x0C},  // -
        {0x39A4, 0x07},  // -
        {0x39A8, 0x32},  // -
        {0x39AA, 0x32},  // -
        {0x39AC, 0x32},  // -
        {0x39AE, 0x32},  // -
        {0x39B0, 0x32},  // -
        {0x39B2, 0x2F},  // -
        {0x39B4, 0x2D},  // -
        {0x39B6, 0x28},  // -
        {0x39B8, 0x30},  // -
        {0x39BA, 0x30},  // -
        {0x39BC, 0x30},  // -
        {0x39BE, 0x30},  // -
        {0x39C0, 0x30},  // -
        {0x39C2, 0x2E},  // -
        {0x39C4, 0x2B},  // -
        {0x39C6, 0x25},  // -
        {0x3A42, 0xD1},  // -
        {0x3A4C, 0x77},  // -
        {0x3AE0, 0x02},  // -
        {0x3AEC, 0x0C},  // -
        {0x3B00, 0x2E},  // -
        {0x3B06, 0x29},  // -
        {0x3B98, 0x25},  // -
        {0x3B99, 0x21},  // -
        {0x3B9B, 0x13},  // -
        {0x3B9C, 0x13},  // -
        {0x3B9D, 0x13},  // -
        {0x3B9E, 0x13},  // -
        {0x3BA1, 0x00},  // -
        {0x3BA2, 0x06},  // -
        {0x3BA3, 0x0B},  // -
        {0x3BA4, 0x10},  // -
        {0x3BA5, 0x14},  // -
        {0x3BA6, 0x18},  // -
        {0x3BA7, 0x1A},  // -
        {0x3BA8, 0x1A},  // -
        {0x3BA9, 0x1A},  // -
        {0x3BAC, 0xED},  // -
        {0x3BAD, 0x01},  // -
        {0x3BAE, 0xF6},  // -
        {0x3BAF, 0x02},  // -
        {0x3BB0, 0xA2},  // -
        {0x3BB1, 0x03},  // -
        {0x3BB2, 0xE0},  // -
        {0x3BB3, 0x03},  // -
        {0x3BB4, 0xE0},  // -
        {0x3BB5, 0x03},  // -
        {0x3BB6, 0xE0},  // -
        {0x3BB7, 0x03},  // -
        {0x3BB8, 0xE0},  // -
        {0x3BBA, 0xE0},  // -
        {0x3BBC, 0xDA},  // -
        {0x3BBE, 0x88},  // -
        {0x3BC0, 0x44},  // -
        {0x3BC2, 0x7B},  // -
        {0x3BC4, 0xA2},  // -
        {0x3BC8, 0xBD},  // -
        {0x3BCA, 0xBD},  // -
        {0x4000, 0x11},
        {0x4001, 0x03},  // LANEMODE[2:0]
        {0x4004, 0xC0},  // TXCLKESC_FREQ[15:0]
        {0x4005, 0x06},  //
        {0x400C, 0x00},  // INCKSEL6
        {0x4018, 0x67},  // TCLKPOST[15:0]
        {0x401A, 0x27},  // TCLKPREPARE[15:0]
        {0x401C, 0x27},  // TCLKTRAIL[15:0]
        {0x401E, 0xB7},  // TCLKZERO[15:0]
        {0x401F, 0x00},  //
        {0x4020, 0x2F},  // THSPREPARE[15:0]
        {0x4022, 0x4F},  // THSZERO[15:0]
        {0x4024, 0x2F},  // THSTRAIL[15:0]
        {0x4026, 0x47},  // THSEXIT[15:0]
        {0x4028, 0x27},  // TLPX[15:0]
        {0x4074, 0x01},  // INCKSEL7 [2:0]

        {0x3000, 0x00}, // Exit standby mode
};

cy_stc_sensor_config_t imx715_StartOperation [] =
{
        {0x3002,0x00},
};

#endif

/******************/
/* User Functions */
/******************/
/******************************************************************************
*   Function:        Cy_ImageSensorSendI2cTable(uint8_t SensorAddress,
*                                   cy_stc_sensor_config_t *sensorConfig,
*                                   uint32_t count)
*
*   Input(s):       SensorAddress, *SensorConfig, count
*
*   Output(s):      Returns 0 for no error
*
*   Description:    Function to send image sensor configuration table to image
*                   sensor
*
*   Calls:          Cy_I2C_Write()
*
******************************************************************************/
cy_en_scb_i2c_status_t Cy_ImageSensorSendI2cTable( uint8_t SensorAddress, const cy_stc_sensor_config_t *SensorConfig, uint32_t count)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    uint32_t looper = 0;
    uint16_t regAdd = 0;
    uint8_t  regValue = 0;

//    if(SensorConfig == NULL);
//    DBG_APP_ERR("Empty SensorConfig\r\n");
    // ASSERT(count > 0,count);

    for(;looper < count; looper++)
    {
        regAdd   = SensorConfig[looper].sensorAddr;
        regValue = SensorConfig[looper].sensorVal;
        status = Cy_I2C_Write(SensorAddress, regAdd, regValue,2,1);
        ASSERT_NON_BLOCK(CY_SCB_I2C_SUCCESS == status ,status);
        if (CY_SCB_I2C_SUCCESS != status)
        {
            glIsSensorConfigured = false;
            break;
        }
    }

    return status;
} /* End of Cy_ImageSensorSendI2cTable() */

#if SONY_IMX715_EN
/******************************************************************************
* Function:       Cy_ImageSensorConfigure (cy_stc_sensor_config_t *SensorConfig, uint16_t count)
*
* Input(s):       SensorConfig, count
*
* Output(s):      Returns 0 for no error
*
* Description:    Function to initialize image sensor
*                 generally image sensor will have i2c comunication to config
*
* Calls:          Cy_ImageSensorSendI2cTable()
*
******************************************************************************/
uint32_t Cy_ImageSensorConfigure(const cy_stc_sensor_config_t *SensorConfig, uint16_t count)
{
    uint32_t status = true;
    /* Send Sony image sensor configuration details */
    DBG_APP_INFO("Cy_ImageSensorConfigure  count = %d\r\n",count);
    status = Cy_ImageSensorSendI2cTable(SENSOR_SLAVE_ADDR, SensorConfig, count);

    /* 200ms delay needed by the sony image sensor before the start */
    for(count = 0; count < 200; count++)
    Cy_SysLib_DelayUs(1000); /* Delay 200ms */

    /* Send start command to sony image sensor */
    status = Cy_ImageSensorSendI2cTable(SENSOR_SLAVE_ADDR, imx715_StartOperation, 1);
    glIsSensorConfigured = (status == CY_SCB_I2C_SUCCESS)? true:false;
    return status;
} /* End of Cy_ImageSensorConfigure() */

/******************************************************************************
*   Function:       Cy_ImageSensorSetResolution(uint32_t width,uint32_t height)
*
*   Input(s):       width, height
*
*   Output(s):      Returns 0 for no error
*
*   Description:    Function to set image sensor resolution
*                   generally image sensor will have i2c comunication to config
*
*   Calls:          Cy_ImageSensorSendI2cTable()
*
******************************************************************************/
uint32_t Cy_ImageSensorSetResolution(uint32_t width,uint32_t height)
{
    uint32_t status;
    uint16_t count = 0;
    DBG_APP_INFO("Resolution %d x %d\r\n",width,height);
    if((width == 3840) && (height == 2160))
    {
        count = sizeof(Cy_ImageSensorInitArray_SonyIMX_4k)/sizeof(cy_stc_sensor_config_t);
        status = Cy_ImageSensorConfigure(Cy_ImageSensorInitArray_SonyIMX_4k, count);
    }
    else if((width == 1920) && (height == 1080))
    {
        count = sizeof(Cy_ImageSensorInitArray_SonyIMX_1080p)/sizeof(cy_stc_sensor_config_t);
        status = Cy_ImageSensorConfigure(Cy_ImageSensorInitArray_SonyIMX_1080p, count);
    }
    else
    {
        status = 0xFE;
        DBG_APP_ERR("Error:Unsupported resolution.Try  3840x2160, 1920x1080\r\n");
    }
    return status;
} /* End of Cy_ImageSensorSetResolution () */

#elif THINE_EN
/******************************************************************************
*   Function:       cy_ImageSensorInitTHine(uint8_t SensorAddress)
*
*   Input(s):       SensorAddress
*
*   Output(s):      Returns 0 for no error
*
*   Description:    Function to initialize THine image sensor
*                   generally image sensor will have i2c comunication to config
*
*   Calls:          Cy_I2C_Write()
*
******************************************************************************/
void cy_ImageSensorInitTHine(uint8_t SensorAddress)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    LOG_CLR(WHT,"Thine Sensor Init Start\r\n");
    /* 75ms delay for thine ISP fw_load */
    Cy_SysLib_Delay(75);
    /*Assign MIPI TX and RX lanes correctly   given by THine*/
    status = Cy_I2C_Write(SensorAddress,0xF0F8,0x2D, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);

    status = Cy_I2C_Write(SensorAddress,0xF0F9,0x36, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);

    status = Cy_I2C_Write(SensorAddress,0xF0F6,0x1, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);

    // while((rdStatus & 0x80))
    // {
        // rdStatus = 0;
        // status = Cy_I2C_Read(SensorAddress,0xF0F7,&rdStatus, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
        // ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    // }
    Cy_SysLib_DelayUs(6000);

    status = Cy_I2C_Write(SensorAddress,0xF008,0x00, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);
#if 0 //1080p30
    /* Reg writes for 1080pYUV422 30fps*/
    Cy_I2C_Write(SensorAddress,0xF00D,0x0B, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF00F,0x81, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF009,0x04, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
#elif 0 //1080p60 YUV422
    Cy_I2C_Write(SensorAddress,0xF00D,0x0B, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF00F,0x82, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF009,0x04, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
#elif 1 //4k30 YUV422

    status = Cy_I2C_Write(SensorAddress,0xF00D,0x0D, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);

    status = Cy_I2C_Write(SensorAddress,0xF00F,0x81, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);

    status = Cy_I2C_Write(SensorAddress,0xF009,0x04, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);
#else //3M 30 fps YUV422
    Cy_I2C_Write(SensorAddress,0xF00D,0x20, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF00F,0x81, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_I2C_Write(SensorAddress,0xF009,0x04, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);

#endif
    status = Cy_I2C_Write(SensorAddress,0xF010,0x01, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);
    Cy_SysLib_DelayUs (60000);

    // rdStatus = 0;
    // while(!(rdStatus == 0x1))
    // {
        // status = Cy_I2C_Read(SensorAddress,0xF011,&rdStatus, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH, false);
        // ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    // }
    // Cy_SysLib_DelayUs(60000);

    status = Cy_I2C_Write(SensorAddress,0xF008,0x01, SENSOR_I2C_ADDRESS_WIDTH,  SENSOR_I2C_DATA_WIDTH);
    Cy_SysLib_DelayUs (60000);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    if ((status == CY_SCB_I2C_SUCCESS) && (SENSOR_SLAVE_ADDR == SensorAddress))
    {
        glIsSensorConfigured = true;;
        LOG_CLR(YEL,"Thine SENSOR CONFIG DONE, SensorAddress = 0x%x\r\n", SensorAddress);
    }else{
        glIsSensorConfigured = false;
        LOG_CLR(RED,"Thine SENSOR CONFIG FAILED, SensorAddress = 0x%x\r\n",SensorAddress);
    }
    /*THine test code ends here*/
}
#endif
/*[]*/
