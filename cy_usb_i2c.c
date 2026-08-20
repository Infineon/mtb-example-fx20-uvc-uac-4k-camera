/******************************************************************************
* \file cy_usb_i2c.c
* \version 1.0
*
* Implements the I2C data handling
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


#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usb_i2c.h"


cy_stc_scb_i2c_context_t I2C_context;
#define I2C_BUFF_SIZE    (10)
uint8_t scb0_i2c_buffer[I2C_BUFF_SIZE];

/******************************************************************************
* Function Name: Cy_I2C_MasterRead(CySCB_Type *base, uint16_t dev_addr,
*                                    const uint8_t *data, uint16_t size,
*                                    bool send_stop)
*******************************************************************************
* Summary:
* Low level function for reading data from I2C slave
*
* Parameters:
* \param base
* I2C SCB base address
* \param dev_addr
* i2c slave device address
* \param data
* pointer to data buffer
* \param size
* data size (in bytes)
* \param send_stop
* set to 1 to send stop condition else 0
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_I2C_MasterRead(CySCB_Type *base,
                                            uint16_t dev_addr,
                                            uint8_t *data,
                                            uint16_t size,
                                            bool send_stop)
{
    uint32_t timeout = 0;

    cy_en_scb_i2c_command_t ack = CY_SCB_I2C_ACK;

    /* Start transaction, send dev_addr */
    cy_en_scb_i2c_status_t status = (I2C_context.state == CY_SCB_I2C_IDLE)
        ? Cy_SCB_I2C_MasterSendStart(base, dev_addr, CY_SCB_I2C_READ_XFER, timeout,  &I2C_context)
        : Cy_SCB_I2C_MasterSendReStart(base, dev_addr, CY_SCB_I2C_READ_XFER, timeout,  &I2C_context);

    if (status == CY_SCB_I2C_SUCCESS)
    {
        while (size > 0) {
            if (size == 1)
            {
                ack = CY_SCB_I2C_NAK;
            }
            status = Cy_SCB_I2C_MasterReadByte(base, ack, (uint8_t *)data, timeout,  &I2C_context);
            if (status != CY_SCB_I2C_SUCCESS)
            {
                break;
            }
            --size;
            ++data;
        }
    }

    if (send_stop)
    {
        /* SCB in I2C mode is very time sensitive. In practice we have to request STOP after */
        /* each block, otherwise it may break the transmission */
        Cy_SCB_I2C_MasterSendStop(base, timeout,  &I2C_context);
    }
    return status;
} /* End of Cy_I2C_MasterRead() */

/******************************************************************************
* Function Name: Cy_I2C_MasterWrite(CySCB_Type *base, uint16_t dev_addr,
*                                    const uint8_t *data, uint16_t size,
*                                    bool send_stop)
*******************************************************************************
* Summary:
* Low level function for writing data to I2C slave
*
* Parameters:
* \param base
* I2C SCB base address
* \param dev_addr
* i2c slave device address
* \param data
* pointer to data buffer
* \param size
* data size (in bytes)
* \param send_stop
* set to 1 to send stop condition else 0
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_I2C_MasterWrite(CySCB_Type *base,
                                            uint16_t dev_addr,
                                            const uint8_t *data,
                                            uint16_t size,
                                            bool send_stop)
{
    /* Clean-up hardware before transfer. Note RX FIFO is empty at here. */
    Cy_SCB_ClearMasterInterrupt(base, CY_SCB_I2C_MASTER_INTR_ALL);
    Cy_SCB_ClearTxFifo(base);
    cy_en_scb_i2c_status_t status;
    status = (I2C_context.state == CY_SCB_I2C_IDLE)
        ? Cy_SCB_I2C_MasterSendStart(base, dev_addr, CY_SCB_I2C_WRITE_XFER, 0, &I2C_context)
        : Cy_SCB_I2C_MasterSendReStart(base, dev_addr, CY_SCB_I2C_WRITE_XFER,0, &I2C_context);

    if (status == CY_SCB_I2C_SUCCESS)
    {
        while (size > 0)
        {
            status = Cy_SCB_I2C_MasterWriteByte(base, *data, 0, &I2C_context);
            if (status != CY_SCB_I2C_SUCCESS)
            {
                break;
            }
            --size;
            ++data;
        }
    }

    if (send_stop)
    {
        /* SCB in I2C mode is very time sensitive. In practice we have to request STOP after */
        /* each block, otherwise it may break the transmission */
        Cy_SCB_I2C_MasterSendStop(base, 0, &I2C_context);
    }

    return status;
} /* End of Cy_I2C_MasterWrite() */

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
cy_en_scb_i2c_status_t Cy_I2C_Read (uint16_t slaveAddress,
                                    uint32_t registerAddress,
                                    uint8_t *data,
                                    uint8_t addressWidth,
                                    uint8_t dataWidth)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;
    uint8_t mem_addr_buf[4] = {0};
    /* Copy address information into buffer */
    if (addressWidth == 1)
    {
        mem_addr_buf[0] = (uint8_t)registerAddress;
    }
    else if (addressWidth == 2)
    {
        mem_addr_buf[0] = (uint8_t)(registerAddress >> 8);
        mem_addr_buf[1] = (uint8_t)registerAddress;
    }
    else if (addressWidth == 4)
    {
        mem_addr_buf[0] = (uint8_t)(registerAddress >> 24);
        mem_addr_buf[1] = (uint8_t)(registerAddress >> 16);
        mem_addr_buf[2] = (uint8_t)(registerAddress >> 8);
        mem_addr_buf[3] = (uint8_t)registerAddress;
    }
    else
    {
        return CY_SCB_I2C_BAD_PARAM;
    }
    status = Cy_I2C_MasterWrite(SCB0, slaveAddress, mem_addr_buf, addressWidth, true);
    if(status ==  CY_SCB_I2C_SUCCESS)
        status = Cy_I2C_MasterRead(SCB0, slaveAddress, data, dataWidth, true);
   return status;
} /* End of Cy_I2C_Read() */

/******************************************************************************
* Function Name: Cy_I2C_Write(uint16_t slaveAddress,
                              uint16_t registerAddress,
                              uint8_t data,
                              uint8_t addressWidth,
                              uint8_t dataWidth)
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
cy_en_scb_i2c_status_t Cy_I2C_Write (uint16_t slaveAddress,
                                    uint16_t registerAddress,
                                    uint8_t data,
                                    uint8_t addressWidth,
                                    uint8_t dataWidth)
{
    /* Print only FPGA-related I2C transactions for ease of debugging */
    if(slaveAddress == FPGASLAVE_ADDR)
    DBG_APP_INFO("I2cWrite SlaveID=0x%x, RegAddr=0x%x, Data=0x%x\n\r",slaveAddress, registerAddress, data);
    int i = 0 , data_size = 0;
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;

    memset(scb0_i2c_buffer, 0, sizeof(scb0_i2c_buffer));
    /* Copy address information into buffer */
    if(2 == addressWidth) {
        scb0_i2c_buffer[i++] = CY_USB_GET_MSB(registerAddress);
        scb0_i2c_buffer[i++] = CY_USB_GET_LSB(registerAddress);
    }
    else if(1 == addressWidth) {
        scb0_i2c_buffer[i++] = CY_USB_GET_LSB(registerAddress);
    }
    /* Add byte data to buffer */
    /* Required to modify this function in case of data byte is more than one byte */
    scb0_i2c_buffer[i++] = data;

    data_size = (addressWidth + dataWidth);
    for(i=0;i<3;i++)
    {
        /* Send I2C data using blocking mode */
        status = Cy_I2C_MasterWrite(SCB0, slaveAddress,&scb0_i2c_buffer[0],data_size,true);
        if(CY_SCB_I2C_SUCCESS == status)
        break;
        Cy_SysLib_DelayUs(100);
    }
    return status;
} /* End of Cy_I2C_Write() */

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
cy_en_scb_i2c_status_t Cy_I2C_FPGAGetVersion(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    uint8_t fpgaStatus = 0;

    status = Cy_I2C_Read(FPGASLAVE_ADDR, FPGA_MAJOR_VERSION_ADDRESS, &(pAppCtxt->fpgaVersion),
                           FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* FPGA Version contains MAJOR + MINOR number. MAJOR version comes on Higher
    nibble of byte and MINOR version comes on Lower nibble of the byte.
    MAJOR and MINOR version is seperated by ('.') */
    DBG_APP_INFO("FPGA version: %x.%x\r\n",(pAppCtxt->fpgaVersion & 0xF0)>>4, pAppCtxt->fpgaVersion & 0x0F);

    status = Cy_I2C_Read(FPGASLAVE_ADDR, DEVICE0_OFFSET+DEVICE_FLAG_STATUS_ADDRESS,
                          &fpgaStatus, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(CY_SCB_I2C_SUCCESS == status, status);
    DBG_APP_INFO("FPGA Status Register = 0x%x\r\n",fpgaStatus);

    return status;
} /* End of Cy_I2C_FPGAGetVersion() */

/******************************************************************************
* Function Name: I2CMaster_ISR
*******************************************************************************
* Summary:
*  Does some init for running rtos based auto reload software timer
*  which periodically enables ITP interrupt for LDM exchange mechanism
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
void Cy_I2C_MasterISR(void)
{
    Cy_SCB_I2C_Interrupt(SCB0, &I2C_context);
}

/******************************************************************************
* Function Name: Cy_I2C_ConfigureSCB0Clock
*******************************************************************************
* Summary:
*  Configure the i2c clock for SCB0
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
void Cy_I2C_ConfigureSCB0Clock(uint8_t Index)
{
    /* Get the Periperal frequency. */
    uint32_t clk_peri = Cy_SysClk_ClkPeriGetFrequency();
    DBG_APP_INFO("clk_peri = %d clock index  = %d\r\n", clk_peri,  Index);

    Cy_SysClk_PeriphDisableDivider (CY_SYSCLK_DIV_8_BIT, Index);
    switch (clk_peri)
    {
        case 50000000:
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 9);
        break;

        case 75000000:
        if (I2C_DATARATE == FX10_I2C_FQ_100KHZ)
        {
            /*  SCB clock = clk_peri/38 = 1.97 MHz (because 100Khz requires scb clk [1.55-3.2M])
            Table 28-17. I2 C frequency and oversampling requirements in I2C Master mode (Default AF enabled) */
            DBG_APP_INFO("Data Rate = %dHz Divider = %d SCB index = %d\r\n",I2C_DATARATE, 38,  Index);
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 37);
        }
        else if (I2C_DATARATE == FX10_I2C_FQ_400KHZ)
        {
            /*  SCB clock = clk_peri/9 = 8.3MHz (because 400Khz requires scb clk [7.82-10M])
            Table 28-17. I2 C frequency and oversampling requirements in I2C Master mode (Default AF enabled)*/
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 8);
        }
        else if (I2C_DATARATE == FX10_I2C_FQ_1MHZ)
        {
            /*  SCB clock = clk_peri/3 = 25MHz (because 1Mhz requires scb clk [14.32-25.8M])
            Table 28-17. I2 C frequency and oversampling requirements in I2C Master mode (Disable AF and try) */
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 4);
        }

        break;

        case 100000000:
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 11);
        break;

        default:
            Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_8_BIT, Index, 9);
        break;
    }

    Cy_SysClk_PeriphEnableDivider (CY_SYSCLK_DIV_8_BIT, Index);
    Cy_SysLib_DelayUs (10);

    /* Connect the PERI clock to the SCB input. */
    Cy_SysClk_PeriphAssignDivider((en_clk_dst_t)(PCLK_SCB0_CLOCK), CY_SYSCLK_DIV_8_BIT, Index);
} /* End of Cy_I2C_ConfigureSCB0Clock() */

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
void Cy_I2C_MasterEvent(uint32_t events)
{
    switch (events)
    {
    case CY_SCB_I2C_MASTER_WR_IN_FIFO_EVENT:
        break;
    case CY_SCB_I2C_MASTER_WR_CMPLT_EVENT:
        break;
    case CY_SCB_I2C_MASTER_RD_CMPLT_EVENT:
        break;
    case CY_SCB_I2C_MASTER_ERR_EVENT:
        break;
    default:
        break;
    }
} /* End of Cy_I2C_MasterEvent() */

/******************************************************************************
* Function Name: Cy_USB_I2CInit(void)
*******************************************************************************
* Summary:
* I2C Master Init function
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
void Cy_USB_I2CInit (void)
{
    #define SCB0_CLK_INDX (1)
    cy_stc_gpio_pin_config_t    pinCfg;
    cy_stc_scb_i2c_config_t     i2cCfg;
    cy_stc_sysint_t             intrCfg;
    uint32_t dataRate;
    uint32_t dataClock;

    Cy_SCB_I2C_DeInit(SCB0);
    Cy_I2C_ConfigureSCB0Clock(SCB0_CLK_INDX);
    memset ((void *)&pinCfg, 0, sizeof(pinCfg));
    memset ((void *)&i2cCfg, 0, sizeof(i2cCfg));

    /* Configure SCB0 pins (P10.0 and P10.1) in Open-Drain drive mode. */
    pinCfg.driveMode = CY_GPIO_DM_OD_DRIVESLOW;
    pinCfg.hsiom     = P10_0_SCB0_I2C_SCL;
    Cy_GPIO_Pin_Init(P10_0_PORT, P10_0_PIN, &pinCfg);

    pinCfg.hsiom     = P10_1_SCB0_I2C_SDA;
    Cy_GPIO_Pin_Init(P10_1_PORT, P10_1_PIN, &pinCfg);

    i2cCfg.i2cMode = CY_SCB_I2C_MASTER;
    i2cCfg.useRxFifo = true;
    i2cCfg.useTxFifo = true;
    i2cCfg.slaveAddress = FPGASLAVE_ADDR;
    i2cCfg.slaveAddressMask = 0x0;
    i2cCfg.lowPhaseDutyCycle = 7;
    i2cCfg.highPhaseDutyCycle = 5;

    Cy_SCB_I2C_Init (SCB0, &i2cCfg, &I2C_context);

    dataClock = Cy_SysClk_PeriphGetFrequency(CY_SYSCLK_DIV_8_BIT, SCB0_CLK_INDX);
    dataRate = Cy_SCB_I2C_SetDataRate(SCB0, I2C_DATARATE, dataClock);
    /* Register interrupt handler for SCB-I2C. */
    DBG_APP_INFO("I2C dataClock: %d\r\n", dataClock);
    DBG_APP_INFO("I2C dataRate: %d\r\n", dataRate);
    if ((dataRate > I2C_DATARATE) || (dataRate == 0U))
    {
        /* Can not reach desired data rate */
        while(1);
    }
    Cy_SCB_ClearRxFifo(SCB0);
    Cy_SCB_ClearTxFifo(SCB0);

    Cy_SCB_I2C_RegisterEventCallback(SCB0,(cy_cb_scb_i2c_handle_events_t) Cy_I2C_MasterEvent,
                                    &I2C_context);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = (IRQn_Type) scb_0_interrupt_IRQn;
    intrCfg.intrPriority = 1u;
    Cy_SysInt_Init(&intrCfg, Cy_I2C_MasterISR);

    /*  Enable interrupt and I2C block */
    NVIC_EnableIRQ((IRQn_Type)scb_0_interrupt_IRQn);
#else
    intrCfg.intrSrc = NvicMux3_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = scb_0_interrupt_IRQn;

    Cy_SysInt_Init(&intrCfg, Cy_I2C_MasterISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
#endif  /* CY_CPU_CORTEX_M4 */

    Cy_SCB_I2C_Enable (SCB0);

} /* End of Cy_USB_I2CInit() */

