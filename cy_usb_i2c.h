/******************************************************************************
* \file cy_usb_i2c.h
* \version 1.0
*
* Defines I2C related macros and functions
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

#ifndef _CY_USB_I2C_H_
#define _CY_USB_I2C_H_

#include "cy_usb_app.h"

#if defined(__cplusplus)
}
#endif

#define FPGASLAVE_ADDR                 (0x0D)      //FPGA i2c address (Do not change)
#define FX10_I2C_FQ_100KHZ             (100000)
#define FX10_I2C_FQ_400KHZ             (400000)
#define FX10_I2C_FQ_1MHZ               (1000000)

#define I2C_DATARATE                   FX10_I2C_FQ_100KHZ

extern cy_stc_scb_i2c_context_t I2C_context;

/******************************************************************************
* Function Name: Cy_USB_I2CInit(void)
*******************************************************************************
* Summary:
* I2C Master Init function
*
* Parameters:
*  void
*
* Return:
*  Does not return.
******************************************************************************/
void Cy_USB_I2CInit (void);

/******************************************************************************
* Function Name: Cy_I2C_MasterEvent(uint32_t events)
*******************************************************************************
* Summary:
*  I2C master event callback function, handling various i2c master event
*
* Parameters:
* \param events
*  i2c master events
*
* Return:
*  Does not return.
******************************************************************************/
void Cy_I2C_MasterEvent(uint32_t events);

/******************************************************************************
* Function Name: Cy_I2C_Read(uint16_t slaveAddress,uint16_t registerAddress,
*                    uint8_t *data,
*                    uint8_t addressWidth,
*                    uint8_t dataWidth)
*******************************************************************************
* Summary:
*  Function to read data from i2c slave
*
* Parameters:
* \param slaveAddress
* 7-bit i2c slave address
* \param registerAddress
* i2c slave register address
* \param data
* pointer to data receive buffer
* \param addressWidth
* address size (in bytes)
* \param dataWidth
* data size (in bytes)
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_I2C_Read (uint16_t slaveAddress,uint32_t registerAddress,
                                                uint8_t *data,
                                                uint8_t addressWidth,
                                                uint8_t dataWidth);

/******************************************************************************
* Function Name: Cy_I2C_Write(uint16_t slaveAddress,uint16_t registerAddress,
*                    uint8_t data,
*                    uint8_t addressWidth,
*                    uint8_t dataWidth)
*******************************************************************************
* Summary:
* Function to write data to i2c slave 
*
* Parameters:
* \param slaveAddress
* 7-bit i2c slave address
* \param registerAddress
* i2c slave register address
* \param data
* pointer to data buffer
* \param addressWidth
* address size (in bytes)
* \param dataWidth
* data size (in bytes)
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_I2C_Write (uint16_t slaveAddress,uint16_t registerAddress,
                                                uint8_t data,
                                                uint8_t addressWidth,
                                                uint8_t dataWidth);

/******************************************************************************
* Function Name: Cy_I2C_FPGAGetVersion(cy_stc_usb_app_ctxt_t *pAppCtx)
*******************************************************************************
* Summary:
*  Function to read FPGA version
*
* Parameters:
*  \param pAppCtxt
* application layer context pointer
*
* Return:
* i2c status
******************************************************************************/
cy_en_scb_i2c_status_t Cy_I2C_FPGAGetVersion(cy_stc_usb_app_ctxt_t *pAppCtxt);

#if defined(__cplusplus)
}
#endif

#endif //End _CY_USB_I2C_H_
