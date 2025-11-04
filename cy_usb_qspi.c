/******************************************************************************
* \file cy_usb_qspi.c
* \version 1.0
*
* Implements function for writing FPGA configuration file to QSPI flash memory
* and configuring FPGA over QSPI interface
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

#include "cy_usb_qspi.h"
#include "cy_smif.h"

cy_stc_smif_context_t qspiContext;
bool glIsFPGAConfigured = false;
cy_en_spiFlashType_t glFlashType = FLASH_UNKNOWN;
cy_en_passiveSerialMode_t glpassiveSerialMode = PASSIVE_x4;
cy_en_smif_slave_select_t glSlaveSelectMode = CY_SMIF_SLAVE_SELECT_0;
cy_en_smif_txfr_width_t   glCommandWidth[NUM_SPI_FLASH]     = {CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_QUAD};
cy_en_smif_txfr_width_t   glReadWriteWidth[NUM_SPI_FLASH]   = {CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_SINGLE, CY_SMIF_WIDTH_OCTAL};
uint8_t glSlaveSelectIndex[NUM_SPI_FLASH] = {CY_SMIF_SLAVE_SELECT_0, CY_SMIF_SLAVE_SELECT_1, (CY_SMIF_SLAVE_SELECT_0 | CY_SMIF_SLAVE_SELECT_1)};
static cy_stc_cfi_flash_map_t glCfiFlashMap[NUM_SPI_FLASH];
cy_en_flash_index_t glFlashMode = SPI_FLASH_0;
cy_stc_externalFlashMetadata_t glFpgaFileMetadata;

/* QSPI/ SMIF Config*/
static const cy_stc_smif_config_t qspiConfig =
{
    .mode = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay = 7u,
    .rxClockSel = (uint32_t)CY_SMIF_SEL_INV_INTERNAL_CLK,
    .blockEvent = (uint32_t)CY_SMIF_BUS_ERROR,
};

/******************************************************************************
* Function name: Cy_SPI_FlashReset(cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to send reset command to selected flash
*
* \param flashIndex
* SPI Flash Index
*
* \return
* status
*
******************************************************************************/
static cy_en_smif_status_t Cy_SPI_FlashReset(cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_APP_SPI_RESET_ENABLE_CMD,
            CY_SMIF_WIDTH_QUAD,
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_APP_SPI_SW_RESET_CMD,
            CY_SMIF_WIDTH_QUAD,
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    /*tRPH delay SFS256 Flash*/
    Cy_SysLib_DelayUs(50);

    return status;
} /* End of Cy_SPI_FlashReset() */

/******************************************************************************
* Function name: Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray,uint8_t numAddressBytes)
*******************************************************************************
*
* Description: Function to convert 32-nit value to byte array
* \param value
* 32-bit value to be converted
*
* \param byteArray
* Pointer to byte array

* \param numAddressBytes
* Number of bytes
*
* \return
* status
*
******************************************************************************/
void Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray,uint8_t numAddressBytes)
{
    do
    {
        numAddressBytes--;
        byteArray[numAddressBytes] = (uint8_t)(value & 0x000000FF);
        value >>= 8; /* Shift to get the next byte */
    } while (numAddressBytes > 0U);
} /* Cy_SPI_AddressToArray() */

/******************************************************************************
* Function name: Cy_App_QSPIStatus1Read(cy_en_smif_slave_select_t slaveSelect)
*******************************************************************************
*
* Description: Function to read status1 register of selected SPI flash
* 32-bit value to be converted
*
* \param slaveSelect
* SPI Flash Index
*
*
* \return
* register value
*
******************************************************************************/
uint8_t Cy_App_QSPIStatus1Read(cy_en_smif_slave_select_t slaveSelect)
{
    uint8_t statusVal = 0;
    Cy_SMIF_TransmitCommand(SMIF0,
                            CY_APP_QSPI_STATUS_1_READ_CMD,
                            CY_SMIF_WIDTH_SINGLE,
                            NULL,
                            CY_SMIF_CMD_WITHOUT_PARAM,
                            CY_SMIF_WIDTH_NA,
                            slaveSelect,
                            CY_SMIF_TX_NOT_LAST_BYTE,
                            &qspiContext);

    Cy_SMIF_ReceiveDataBlocking(SMIF0, &statusVal, 1u, CY_SMIF_WIDTH_SINGLE, &qspiContext);
    DBG_APP_TRACE("SLV%d:Status Register 01: 0x%x\r\n",slaveSelect-1,statusVal);
    return statusVal;
} /* End of Cy_App_QSPIStatus1Read() */

/******************************************************************************
* Function name: Cy_App_QSPIStatus2Read(cy_en_smif_slave_select_t slaveSelect)
*******************************************************************************
*
* Description: Function to read status2 register of selected SPI flash
* 32-bit value to be converted
*
* \param slaveSelect
* SPI Flash Index
*
*
* \return
* register value
*
******************************************************************************/
uint8_t Cy_App_QSPIStatus2Read(cy_en_smif_slave_select_t slaveSelect)
{
    uint8_t statusVal = 0;
    Cy_SMIF_TransmitCommand(SMIF0,
                            CY_APP_QSPI_STATUS_2_READ_CMD,
                            CY_SMIF_WIDTH_SINGLE,
                            NULL,
                            CY_SMIF_CMD_WITHOUT_PARAM,
                            CY_SMIF_WIDTH_NA,
                            slaveSelect,
                            CY_SMIF_TX_NOT_LAST_BYTE,
                            &qspiContext);

    Cy_SMIF_ReceiveDataBlocking(SMIF0, &statusVal, 1u, CY_SMIF_WIDTH_SINGLE, &qspiContext);
    DBG_APP_TRACE("SLV%d:Status Register 02: 0x%x\r\n",slaveSelect-1,statusVal);
    return statusVal;
} /* End of Cy_App_QSPIStatus2Read */


/******************************************************************************
* Function name: Cy_QSPI_WriteEnable(cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to Set write enable latch of the selected flash.
*              This is needed before doing program and erase operations.
*
* \param flashIndex
* SPI Flash Index
*
* \return
* status
*
******************************************************************************/
static cy_en_smif_status_t Cy_QSPI_WriteEnable(cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t statusVal = 0;

    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_WRITE_ENABLE_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    /* Check if WRITE_ENABLE LATCH is set */
    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_STATUS_READ_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW,
            &statusVal,
            1u,
            glReadWriteWidth[flashIndex],
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if(statusVal & CY_SPI_WRITE_ENABLE_LATCH_MASK)
    {
        status = CY_SMIF_SUCCESS;
    }
    else
    {
        status = CY_SMIF_BUSY;
        DBG_APP_ERR("Write Enable failed\r\n");
    }

    return status;
} /* End of Cy_QSPI_WriteEnable() */

/******************************************************************************
* Function name: Cy_QSPI_ReadID(cy_en_smif_slave_select_t slaveSelect, uint8_t *idBuffer)
*******************************************************************************
*
* Description: Function to read QSPI Flash ID
*
* \param slaveSelect
* SPI Flash Index
*
** \param idBuffer
* ID buffer pointer
*
* \return
* void
*
******************************************************************************/
void Cy_QSPI_ReadID(cy_en_smif_slave_select_t slaveSelect, uint8_t *idBuffer)
{
    Cy_SMIF_TransmitCommand(SMIF0,
                            CY_APP_QSPI_READ_ID_CMD,
                            CY_SMIF_WIDTH_SINGLE,
                            NULL,
                            CY_SMIF_CMD_WITHOUT_PARAM,
                            CY_SMIF_WIDTH_NA,
                            slaveSelect,
                            CY_SMIF_TX_NOT_LAST_BYTE,
                            &qspiContext);

    Cy_SMIF_ReceiveDataBlocking(SMIF0, idBuffer, CY_APP_QSPI_ID_SIZE,
                                CY_SMIF_WIDTH_SINGLE, &qspiContext);

    DBG_APP_TRACE("SLV%d:QSPI ID:",slaveSelect-1);
} /* End of Cy_QSPI_ReadID() */

/******************************************************************************
* Function name: Cy_QSPI_IsMemBusy(cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to check if Write In Progress (WIP) bit of the flash is cleared
*
* \param flashIndex
* SPI Flash Index
*
* \return
* status
*
******************************************************************************/
bool Cy_QSPI_IsMemBusy(cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t statusVal;
    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_STATUS_READ_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);

    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW,
            &statusVal,
            1u,
            glReadWriteWidth[flashIndex],
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    /* If the Memory is busy, it returns true */
    return ((statusVal & 0x01) == 0x01);
} /* End of Cy_QSPI_IsMemBusy() */

/******************************************************************************
* Function name: Cy_SPI_ReadConfigRegister(cy_en_flash_index_t flashIndex, uint8_t *readValue)
*******************************************************************************
*
* Description: Function to read selected flash's config register
*
* \param flashIndex
* SPI Flash Index
*
* \param readValue
* Pointer to read data
*
* \return
* status
*
******************************************************************************/
static cy_en_smif_status_t Cy_SPI_ReadConfigRegister(cy_en_flash_index_t flashIndex, uint8_t *readValue)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    if((flashIndex == DUAL_SPI_FLASH) || (NULL == readValue))
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    DBG_APP_TRACE("Read Config Register..\r\n");
    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_CONFIG_REG_READ_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            glCommandWidth[flashIndex],
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    if(CY_SMIF_SUCCESS == status)
    {
        status =  Cy_SMIF_ReceiveDataBlocking(SMIF_HW, readValue, 1u, glReadWriteWidth[flashIndex], &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
        DBG_APP_TRACE("Config Register Value: 0x%x\r\n", *readValue);
    }
    return status;
} /* End of Cy_SPI_ReadConfigRegister() */

/******************************************************************************
* Function name: Cy_QSPI_ReadAnyRegister(cy_en_flash_index_t flashIndex,uint32_t address)
*******************************************************************************
*
* Description: Function to read selected flash's register
*
* \param flashIndex
* SPI Flash Index
*
* \param address
* register address
*
* \return
* register value
*
******************************************************************************/
static uint8_t Cy_QSPI_ReadAnyRegister(cy_en_flash_index_t flashIndex,uint32_t address)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t regValue = 0;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];
    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return 0xFF;
    }

    Cy_SPI_AddressToArray(address, addrArray, SPI_ADDRESS_BYTE_COUNT);

    status = Cy_SMIF_TransmitCommand(SMIF0,
            CY_APP_SPI_READ_ANY_REG_CMD,
            glCommandWidth[flashIndex],
            addrArray,
            SPI_ADDRESS_BYTE_COUNT,
            glCommandWidth[flashIndex],
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

    Cy_SMIF_SendDummyCycles(SMIF_HW, CY_QSPI_NUM_DUMMY_CYCLES);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF0, &regValue, 1u, glReadWriteWidth[flashIndex], &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    DBG_APP_TRACE("SLV%d:Read Register: 0x%x Read value = 0x%x\r\n",flashIndex,address, regValue);
    return regValue;
} /* End of Cy_QSPI_ReadAnyRegister() */

/******************************************************************************
* Function name: Cy_QSPI_WriteAnyRegister(cy_en_flash_index_t flashIndex,uint32_t address)
*******************************************************************************
*
* Description: Function to write to register address of the selected flash
*
* \param flashIndex
* SPI Flash Index
*
* \param address
* register address
*
* \param value
* register value
*
* \return
* status
*
******************************************************************************/

static cy_en_smif_status_t  Cy_QSPI_WriteAnyRegister(cy_en_flash_index_t flashIndex,uint32_t regAddress, uint8_t value)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];

    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    Cy_SPI_AddressToArray(regAddress, addrArray,SPI_ADDRESS_BYTE_COUNT);
    DBG_APP_TRACE("Write %d to Register: 0x%x\r\n", value, regAddress);

    status = Cy_QSPI_WriteEnable(flashIndex);
    Cy_SysLib_Delay(200);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_TransmitCommand(SMIF0,
                CY_APP_SPI_WRITE_ANY_REGISTER_CMD,
                glCommandWidth[flashIndex],
                addrArray,
                SPI_ADDRESS_BYTE_COUNT,
                glCommandWidth[flashIndex],
                (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                CY_SMIF_TX_NOT_LAST_BYTE,
                &qspiContext);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

        status = Cy_SMIF_TransmitDataBlocking(SMIF0, &value, 1, glReadWriteWidth[flashIndex], &qspiContext);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    }
    return status;
} /* End of Cy_QSPI_WriteAnyRegister() */

/******************************************************************************
* Function name: Cy_SPI_ReadCFIMap(cy_stc_cfi_flash_map_t *cfiFlashMap, cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to read Common Flash Interface (CFI) Table
*
*
* \param cfiFlashMap
* CFI Flash Table pointer
*
* \param flashIndex
* SPI Flash Index
*
* \return
* status
*
******************************************************************************/
static cy_en_smif_status_t Cy_SPI_ReadCFIMap (cy_stc_cfi_flash_map_t *cfiFlashMap, cy_en_flash_index_t flashIndex)
{
    uint8_t sectorIndex = 0;
    uint8_t eraseRegionIndex = 0;
    uint8_t rxBuffer[CY_CFI_TABLE_LENGTH] = {0};
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_READ_ID_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW, rxBuffer, CY_CFI_TABLE_LENGTH, glReadWriteWidth[flashIndex], &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    cfiFlashMap->deviceSizeFactor = rxBuffer[CY_CFI_DEVICE_SIZE_OFFSET];
    cfiFlashMap->deviceSize = (uint32_t)(1u << cfiFlashMap->deviceSizeFactor);
    DBG_APP_INFO("DeviceSize = 0x%x[%d]\r\n", (cfiFlashMap->deviceSize), (cfiFlashMap->deviceSize));

    /* Parse the CFI buffer and understand possible memory array layouts */
    cfiFlashMap->numEraseRegions = rxBuffer[CY_CFI_NUM_ERASE_REGION_OFFSET];
    DBG_APP_INFO("Number of erase regions = %d\r\n", cfiFlashMap->numEraseRegions);

    if(cfiFlashMap->numEraseRegions < CY_CFI_TABLE_LENGTH)
    {
        //The part has multiple erase layouts, possibly because it supports hybrid layout
        for(eraseRegionIndex = 0 , sectorIndex = 0;
                eraseRegionIndex < (cfiFlashMap->numEraseRegions);
                eraseRegionIndex++, sectorIndex += CY_CFI_ERASE_REGION_SIZE_INFO_SIZE)
        {
            cfiFlashMap->memoryLayout[eraseRegionIndex].numSectors = 1 + (rxBuffer[sectorIndex + CY_CFI_ERASE_NUM_SECTORS_OFFSET] |
                    (rxBuffer[sectorIndex + CY_CFI_ERASE_NUM_SECTORS_OFFSET + 1] << 8));

            cfiFlashMap->memoryLayout[eraseRegionIndex].sectorSize = 256 * (rxBuffer[sectorIndex + CY_CFI_ERASE_SECTOR_SIZE_OFFSET] |
                    (rxBuffer[sectorIndex + CY_CFI_ERASE_SECTOR_SIZE_OFFSET + 1] << 8));
            if(eraseRegionIndex)
            {
                cfiFlashMap->memoryLayout[eraseRegionIndex].startingAddress = (cfiFlashMap->memoryLayout[eraseRegionIndex - 1].startingAddress +
                        cfiFlashMap->memoryLayout[eraseRegionIndex - 1].sectorSize *
                        cfiFlashMap->memoryLayout[eraseRegionIndex - 1].numSectors);
            }
            else
            {
                cfiFlashMap->memoryLayout[eraseRegionIndex].startingAddress = 0;
            }

            cfiFlashMap->memoryLayout[eraseRegionIndex].lastAddress = cfiFlashMap->memoryLayout[eraseRegionIndex].startingAddress +
                cfiFlashMap->memoryLayout[eraseRegionIndex].sectorSize - 1;

            if(cfiFlashMap->memoryLayout[eraseRegionIndex].sectorSize == 0x1000)
            {
                cfiFlashMap->num4KBParameterRegions++;
            }

            DBG_APP_TRACE("Erase region:%d, numSectors=%d, sectorSize=0x%x, startingAddress=0x%x\r\n",eraseRegionIndex,
                    cfiFlashMap->memoryLayout[eraseRegionIndex].numSectors,
                    cfiFlashMap->memoryLayout[eraseRegionIndex].sectorSize,
                    cfiFlashMap->memoryLayout[eraseRegionIndex].startingAddress);
        }
    }
    return status;
} /* End of Cy_SPI_ReadCFIMap() */

/******************************************************************************
* Function name: Cy_SPI_ReadID(uint8_t *rxBuffer, cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to read SPI Flash ID
*
* \param flashIndex
* SPI Flash Index
*
** \param rxBuffer
* ID buffer pointer
*
* \return
* void
*
******************************************************************************/
cy_en_smif_status_t Cy_SPI_ReadID(uint8_t *rxBuffer, cy_en_flash_index_t flashIndex)
{
    DBG_APP_TRACE("Cy_SPI_ReadID Init \n\r");
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    if(flashIndex == DUAL_SPI_FLASH)
    {
        DBG_APP_ERR("[%s]Invalid flashIndex. Access both flash memories separately\r\n",__func__);
        return CY_SMIF_BAD_PARAM;
    }

    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            CY_SPI_READ_ID_CMD,
            glCommandWidth[flashIndex],
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW, rxBuffer, CY_FLASH_ID_LENGTH, glReadWriteWidth[flashIndex], &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    return status;
} /* End of Cy_SPI_ReadID() */

/******************************************************************************
* Function name: Cy_SPI_FlashInit(cy_en_flash_index_t flashIndex, bool quadEnable, bool qpiEnable)
*******************************************************************************
*
* Description: Function to initialize SPI Flash
*
* Quad Mode - Data in x4 mode, Command in x1 mode
* QPI Mode - Data in x4 mode, Command in x4 mode
*
* QPI enabled implies Quad enable.
*
* Enable only Quad mode when writes to flash can be in x1 mode and only reads need to be in x4 mode (eg: Passive x4 mode with one x4 flash memory)
* Enable QPI mode when writes and reads should be in x4 mode (eg: Passive x8 mode with two x4 flash memories)
*
* \param flashIndex
* SPI Flash Index
*
* \param quadEnable
* Quad Mode enable

* \param qpiEnable
* QPI mode enable
*
* \return
* status
*
******************************************************************************/
cy_en_smif_status_t Cy_SPI_FlashInit (cy_en_flash_index_t flashIndex, bool quadEnable, bool qpiEnable)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t readValue = 0;
    uint8_t flashID[CY_FLASH_ID_LENGTH]={0};
    uint8_t regValue = 0;

    if (qpiEnable)
    {
        quadEnable = true;
    }

    Cy_SPI_FlashReset(flashIndex);

    if(quadEnable)
    {
        /* Enable QPI and QUAD Mode bits for SFS256 Flash */
        regValue = Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR1V_REG);
        Cy_QSPI_WriteAnyRegister(flashIndex, CY_SFS256_CR1V_REG, regValue | 0x02);
        glReadWriteWidth[flashIndex]  = CY_SMIF_WIDTH_QUAD;
        if(qpiEnable)
        {
        regValue = Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR2V_REG);
        Cy_QSPI_WriteAnyRegister(flashIndex, CY_SFS256_CR2V_REG, regValue | 0x40);
            glCommandWidth[flashIndex]    = CY_SMIF_WIDTH_QUAD;
        }
    }
    status = Cy_SPI_ReadCFIMap(&glCfiFlashMap[flashIndex], flashIndex);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    Cy_SPI_ReadID(flashID, flashIndex);

    Cy_SPI_ReadConfigRegister(flashIndex, &readValue);
    Cy_QSPI_ReadAnyRegister(flashIndex, 4);
    Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR3V_REG);
    Cy_QSPI_ReadAnyRegister(flashIndex, 4);
    Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR3V_REG);
    Cy_QSPI_ReadAnyRegister(flashIndex, 2);
    Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR1V_REG);
    Cy_QSPI_ReadAnyRegister(flashIndex, CY_SFS256_CR2V_REG);
    return status;
} /* End of Cy_SPI_FlashInit() */

/******************************************************************************
* Function name: Cy_QSPI_Read(uint32_t address, uint8_t *rxBuffer, uint32_t length,
*                            cy_en_flash_index_t flashIndex)
*******************************************************************************
*
* Description: Function to read the SPI Flash
*
* \param address
* SPI Flash address
*
* \param rxBuffer
* Data buffer pointer
*
* \param length
* length of data received
* \param flashIndex
* SPI Flash index
*
* \return
* status
*
******************************************************************************/

cy_en_smif_status_t Cy_QSPI_Read(uint32_t address, uint8_t *rxBuffer, uint32_t length, cy_en_flash_index_t flashIndex)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t addrArray[SPI_ADDRESS_BYTE_COUNT];
    uint8_t readCommand = CY_SPI_READ_CMD;

    if ((glCommandWidth[flashIndex] == CY_SMIF_WIDTH_QUAD) ||
        (glReadWriteWidth[flashIndex] == CY_SMIF_WIDTH_QUAD))
    {
        readCommand = CY_SPI_QPI_READ_CMD;
    }

    Cy_SPI_AddressToArray(address, addrArray, SPI_ADDRESS_BYTE_COUNT);
    status = Cy_SMIF_TransmitCommand(SMIF_HW,
            readCommand,
            glCommandWidth[flashIndex],
            addrArray,
            SPI_ADDRESS_BYTE_COUNT,
            glReadWriteWidth[flashIndex],
            (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
            CY_SMIF_TX_NOT_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if(glReadWriteWidth[flashIndex] != CY_SMIF_WIDTH_SINGLE)
    {
        Cy_SMIF_TransmitCommand(SMIF_HW,
                CY_SPI_QREAD_MODE_CMD,
                glReadWriteWidth[flashIndex],
                NULL,
                CY_SMIF_CMD_WITHOUT_PARAM,
                CY_SMIF_WIDTH_NA,
                (cy_en_smif_slave_select_t)glSlaveSelectIndex[flashIndex],
                CY_SMIF_TX_NOT_LAST_BYTE,
                &qspiContext);

        Cy_SMIF_SendDummyCycles(SMIF_HW, CY_QSPI_NUM_DUMMY_CYCLES);
    }

    if(status == CY_SMIF_SUCCESS)
    {
        status = Cy_SMIF_ReceiveDataBlocking(SMIF_HW, rxBuffer, length, glReadWriteWidth[flashIndex], &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
    }
    return status;
} /* End of Cy_QSPI_Read() */

/******************************************************************************
* Function name: Cy_QSPI_Start(cy_stc_usb_app_ctxt_t *pAppCtxt,cy_stc_hbdma_buf_mgr_t *hbw_bufmgr)
*******************************************************************************
*
* Description: Function to start the QSPI/SMIF block
*
* \param pAppCtxt
* application layer context pointer
*
* \param hbw_bufmgr
* HBDMA buffer manager pointer
*
* \return
* status
*
******************************************************************************/
void Cy_QSPI_Start(cy_stc_usb_app_ctxt_t *pAppCtxt,cy_stc_hbdma_buf_mgr_t *hbw_bufmgr)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    pAppCtxt->qspiWriteBuffer = (uint8_t *)Cy_HBDma_BufMgr_Alloc(hbw_bufmgr, MAX_BUFFER_SIZE);
    pAppCtxt->qspiReadBuffer  = (uint8_t *)Cy_HBDma_BufMgr_Alloc(hbw_bufmgr, MAX_BUFFER_SIZE);
    memset(pAppCtxt->qspiWriteBuffer, 0, MAX_BUFFER_SIZE);
    memset(pAppCtxt->qspiReadBuffer, 0, MAX_BUFFER_SIZE);

    /* Change QSPI Clock to 150 MHz / <DIVIDER> value */
    Cy_SysClk_ClkHfDisable(1);
    Cy_SysClk_ClkHfSetSource(1, CY_SYSCLK_CLKHF_IN_CLKPATH2);
    Cy_SysClk_ClkHfSetDivider(1, CY_SYSCLK_CLKHF_NO_DIVIDE);
    Cy_SysClk_ClkHfEnable(1);

    /*Initialize SMIF Pins for QSPI*/
    Cy_QSPI_ConfigureSMIFPins(true);

    status = Cy_SMIF_Init(SMIF_HW, &qspiConfig, 10000u, &qspiContext);
    ASSERT(CY_SMIF_SUCCESS == status, status);

    Cy_SMIF_SetDataSelect(SMIF_HW, CY_SMIF_SLAVE_SELECT_0, CY_SMIF_DATA_SEL0);
    Cy_SMIF_SetDataSelect(SMIF_HW, CY_SMIF_SLAVE_SELECT_1, CY_SMIF_DATA_SEL2);

    Cy_SMIF_Enable(SMIF_HW, &qspiContext);

    DBG_APP_TRACE("Cy_USB_QSPIEnabled \n\r:");
} /* End of Cy_QSPI_Start() */

/******************************************************************************
* Function name: Cy_QSPI_ConfigureSMIFPins(bool init)
*******************************************************************************
*
* Description: Function to configure SMIF pins
*
* \param init
* initialize SMIF pins if true
*
*
* \return
* void
*
******************************************************************************/
void Cy_QSPI_ConfigureSMIFPins(bool init)
{
    cy_en_gpio_status_t status = CY_GPIO_SUCCESS;
    cy_stc_gpio_pin_config_t pinCfg;

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    if(init)
    {
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        /* Configure P6.0 as QSPI Clock */
        pinCfg.hsiom = SMIF_CLK_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.0 as floating GPIO */
        pinCfg.hsiom = P6_0_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_CLK_PORT, SMIF_CLK_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        /* Configure P6.1 as QSPI Select 0 */
        pinCfg.outVal = 1;
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        pinCfg.hsiom = SMIF_SELECT0_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.1 as floating GPIO */
        pinCfg.hsiom = P6_1_GPIO;
    }

    status = Cy_GPIO_Pin_Init(SMIF_SELECT0_PORT, SMIF_SELECT0_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        /* Configure P6.2 as QSPI Select 1 */
        pinCfg.outVal = 1;
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        pinCfg.hsiom = SMIF_SELECT1_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P6.2 as floating GPIO */
        pinCfg.hsiom = P6_2_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_SELECT1_PORT, SMIF_SELECT1_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.0 as QSPI Data 0 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA0_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.0 as floating GPIO */
        pinCfg.hsiom = P7_0_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA0_PORT, SMIF_DATA0_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.1 as QSPI Data 1 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA1_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.1 as floating GPIO */
        pinCfg.hsiom = P7_1_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA1_PORT, SMIF_DATA1_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.2 as QSPI Data 2*/
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA2_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.2 as floating GPIO */
        pinCfg.hsiom = P7_2_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA2_PORT, SMIF_DATA2_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.3 as QSPI Data 3*/
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA3_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.3 as floating GPIO */
        pinCfg.hsiom = P7_3_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA3_PORT, SMIF_DATA3_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.4 as QSPI Data 4 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA4_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.4 as floating GPIO */
        pinCfg.hsiom = P7_4_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA4_PORT, SMIF_DATA4_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.5 as QSPI Data 5 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA5_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.5 as floating GPIO */
        pinCfg.hsiom = P7_5_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA5_PORT, SMIF_DATA5_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.6 as QSPI Data 6 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA6_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.6 as floating GPIO */
        pinCfg.hsiom = P7_6_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA6_PORT, SMIF_DATA6_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);

    if(init)
    {
        pinCfg.outVal = 0;
        /* Configure P7.7 as QSPI Data 7 */
        pinCfg.driveMode = CY_GPIO_DM_STRONG;
        pinCfg.hsiom = SMIF_DATA7_HSIOM;
    }
    else
    {
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        /* Configure P7.7 as floating GPIO */
        pinCfg.hsiom = P7_7_GPIO;
    }
    status = Cy_GPIO_Pin_Init(SMIF_DATA7_PORT, SMIF_DATA7_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == status, status);
} /* End of Cy_QSPI_ConfigureSMIFPins() */

/******************************************************************************
* Function name: Cy_FPGAConfigPins(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode)
*******************************************************************************
*
* Description: Function to initialize FPGA configuration pins
*
* \param pAppCtxt
* application layer context pointer
*
* \param mode
* fpga configuration mode
*
*
* \return
* void
*
******************************************************************************/
void Cy_FPGAConfigPins(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode)
{
    cy_en_gpio_status_t status = CY_GPIO_SUCCESS;
    cy_stc_gpio_pin_config_t pinCfg;
    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure output GPIOs. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom = HSIOM_SEL_GPIO;

    pinCfg.hsiom     = P8_4_GPIO;
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    status = Cy_GPIO_Pin_Init(P8_4_PORT, P8_4_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    pinCfg.hsiom     = P8_5_GPIO;
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    status = Cy_GPIO_Pin_Init(P8_5_PORT, P8_5_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    status = Cy_GPIO_Pin_Init(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    Cy_GPIO_Clr(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN);

    status = Cy_GPIO_Pin_Init(T120_FPGA_SSN_PORT, T120_FPGA_SSN_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    if(mode == ACTIVE_SERIAL_MODE)
    {
        /*FPGA's SSL_N should be HIGH for Active Serial*/
        Cy_GPIO_Set(T120_FPGA_SSN_PORT, T120_FPGA_SSN_PIN);
    }
    else
    {
        Cy_GPIO_Clr(T120_FPGA_SSN_PORT, T120_FPGA_SSN_PIN);
    }

    status = Cy_GPIO_Pin_Init(T120_PROGRAM_N_PORT, T120_PROGRAM_N_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    /* PROGRAM# is connected to CSI pin of FPGA. CSI pin should be always HIGH */
    Cy_GPIO_Set(T120_PROGRAM_N_PORT, T120_PROGRAM_N_PIN);

    status = Cy_GPIO_Pin_Init(HDMI_KIT_USER_LED_PORT, HDMI_KIT_USER_LED_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    status = Cy_GPIO_Pin_Init(HDMI_KIT_FPGA_SOFTRESET_PORT, HDMI_KIT_FPGA_SOFTRESET_PIN, &pinCfg);
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);

    Cy_GPIO_Set(HDMI_KIT_FPGA_SOFTRESET_PORT, HDMI_KIT_FPGA_SOFTRESET_PIN);

#if CONFIG_CDONE_AS_INPUT
    /* Configure input GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    status = Cy_GPIO_Pin_Init(T120_CDONE_PORT, T120_CDONE_PIN, &pinCfg);
#else
    status = Cy_GPIO_Pin_Init(T120_CDONE_PORT, T120_CDONE_PIN, &pinCfg);
    Cy_GPIO_Clr(T120_CDONE_PORT, T120_CDONE_PIN);
#endif
    ASSERT_NON_BLOCK(status == CY_GPIO_SUCCESS, status);
} /* End of Cy_FPGAConfigPins() */

/******************************************************************************
* Function name: Cy_FPGAConfigure(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode)
*******************************************************************************
*
* Description: Function to initialize initiate FPGA configuration
*
* \param pAppCtxt
* application layer context pointer
*
* \param mode
* fpga configuration mode
*
*
* \return
* void
*
******************************************************************************/
bool Cy_FPGAConfigure(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode)
{
    uint32_t cdoneVal = 0;
    uint32_t maxWait = 1000;

    if(mode == ACTIVE_SERIAL_MODE)
    {
        Cy_QSPI_ConfigureSMIFPins(false);

        DBG_APP_INFO("Starting Active Serial FPGA Configuration\r\n");

        Cy_GPIO_Set(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN);

        Cy_SysLib_Delay(2000);

        cdoneVal = false;
        while (cdoneVal == false)
        {
            /*Check if CDONE is LOW*/
            cdoneVal = Cy_GPIO_Read(T120_CDONE_PORT, T120_CDONE_PIN);
            Cy_SysLib_Delay(1);
            maxWait--;
            if (!maxWait)
            {
                break;
            }
        }

        if (cdoneVal)
        {
            glIsFPGAConfigured = true;
            Cy_Debug_AddToLog(3, CYAN);
            Cy_Debug_AddToLog (3,"FPGA Configuration Done \n\r");
            Cy_Debug_AddToLog(3, COLOR_RESET);
        }
        else
        {
            /*Keep the FPGA in reset if the config fails. This is to prevent the FPGA
             *from taking control of the SPI lines which prevents FX10's access to QSPI Flash */
            Cy_GPIO_Clr(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN);
            Cy_Debug_AddToLog(1, CYAN);
            Cy_Debug_AddToLog (1,"FPGA Configuration Failed! \n\r");
            Cy_Debug_AddToLog(1, COLOR_RESET);
        }

        /*Re-init SMIF pins. This should not affect the RISC-V boot
         *also as there is sufficient delay of 2 seconds*/
        Cy_QSPI_ConfigureSMIFPins(true);
    }
    else
    {
#if CONFIG_CDONE_AS_INPUT
        uint32_t bitFileSize = EFINIX_MAX_CONFIG_FILE_SIZE;
#else
        uint32_t bitFileSize = EFINIX_FPGA_SOC_MERGED_FILE_SIZE; /*#TODO: Replace with size read from Flash*/
#endif

        Cy_Debug_AddToLog(3, CYAN);
        Cy_Debug_AddToLog (3," Bit file Size %x \n\r",bitFileSize);
        Cy_Debug_AddToLog(3, COLOR_RESET);

        uint32_t bitFileStartAddress = 0;
        cy_en_smif_status_t status = CY_SMIF_SUCCESS;

        uint8_t addrArray[3];

        Cy_QSPI_Read(CY_APP_QSPI_METADATA_ADDRESS, (uint8_t *)&glFpgaFileMetadata,
        CY_APP_QSPI_METADATA_SIZE, glFlashMode);


        if(glFpgaFileMetadata.startSignature == CY_APP_QSPI_METADATA_SIGNATURE)
        {
            DBG_APP_TRACE("Metadata details: FPGA File Size:0x%x FPGA File Start:0x%x\r\n",
                    glFpgaFileMetadata.fpgaFileSize,
                    glFpgaFileMetadata.fpgaFileStartAddress);
            bitFileStartAddress = glFpgaFileMetadata.fpgaFileStartAddress;
        }
        else
        {
            bitFileStartAddress = 0x00;
        }

        Cy_SPI_AddressToArray(bitFileStartAddress, addrArray, CY_APP_QSPI_NUM_ADDRESS_BYTES);

        DBG_APP_INFO("Passive Serial FPGA Config: offset 0x%x\r\n",bitFileStartAddress);
        Cy_GPIO_Clr(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN);


        Cy_SysLib_Delay(2);

#if CONFIG_CDONE_AS_INPUT
        cdoneVal = true;
        maxWait = 1000;
        while (cdoneVal == true)
        {
            /*Check if CDONE is LOW*/
            cdoneVal = Cy_GPIO_Read(T120_CDONE_PORT, T120_CDONE_PIN);
            Cy_SysLib_Delay(1);
            maxWait--;
            if (!maxWait)
            {
                break;
            }
        }
#else
        Cy_GPIO_Clr(T120_CDONE_PORT, T120_CDONE_PIN);
#endif

        status = Cy_SMIF_TransmitCommand(SMIF0,
                                CY_APP_QSPI_QREAD_CMD,
                                CY_SMIF_WIDTH_SINGLE,
                                addrArray,
                                CY_APP_QSPI_NUM_ADDRESS_BYTES,
                                CY_SMIF_WIDTH_QUAD,
                                glSlaveSelectMode,
                                CY_SMIF_TX_NOT_LAST_BYTE,
                                &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

        status = Cy_SMIF_TransmitCommand(SMIF0,
                                CY_APP_QSPI_QREAD_MODE_CMD,
                                CY_SMIF_WIDTH_QUAD,
                                NULL,
                                CY_SMIF_CMD_WITHOUT_PARAM,
                                CY_SMIF_WIDTH_NA,
                                glSlaveSelectMode,
                                CY_SMIF_TX_NOT_LAST_BYTE,
                                &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

        status = Cy_SMIF_SendDummyCycles(SMIF0, CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFL);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

        Cy_GPIO_Set(T120_INIT_RESET_PORT, T120_INIT_RESET_PIN);

        Cy_SysLib_Delay(6); /*Minimum time between deassertion of CRESET_N to first valid configuration data.*/

        // Passive x4 - One clock cycle shifts out 4 bits. Two clock cycles => 1 byte. For X bytes, X *2 clock cycles are required
        uint32_t cycles = MAX_DUMMY_CYCLES_COUNT;
        uint32_t remainingCycles = (pAppCtxt->glpassiveSerialMode == PASSIVE_x4)? bitFileSize*2 : bitFileSize;

        /* Extra clocks to get the FPGA into user mode */
        remainingCycles += DUMMY_CYCLE;


        while (remainingCycles > 0)
        {
            cycles = CY_USB_MIN(remainingCycles, MAX_DUMMY_CYCLES_COUNT);
            if (Cy_SMIF_GetCmdFifoStatus(SMIF0) < 3)
            {
                status = Cy_SMIF_SendDummyCycles(SMIF0, cycles);
                ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);
                remainingCycles -= cycles;
            }
        }


#if CONFIG_CDONE_AS_INPUT
        cdoneVal = false;
        maxWait = 1000;
        while (cdoneVal == false)
        {
            /* Check if CDONE is LOW */
            cdoneVal = Cy_GPIO_Read(T120_CDONE_PORT, T120_CDONE_PIN);
            Cy_SysLib_Delay(1);
            maxWait--;
            if (!maxWait)
            {
                break;
            }
        }
#else
        cy_stc_gpio_pin_config_t pinCfg;
        memset((void *)&pinCfg, 0, sizeof(pinCfg));
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        pinCfg.hsiom = HSIOM_SEL_GPIO;

        Cy_QSPI_ConfigureSMIFPins(false);

        Cy_GPIO_Pin_Init(T120_CDONE_PORT, T120_CDONE_PIN, &pinCfg);
#endif

        maxWait = 1000;
        cdoneVal = false;
        while (cdoneVal == false)
        {
            /* Check if CDONE is LOW */
            cdoneVal = Cy_GPIO_Read(T120_CDONE_PORT, T120_CDONE_PIN);
            Cy_SysLib_Delay(1);
            maxWait--;
            if (!maxWait)
            {
                break;
            }
        }

        if (cdoneVal)
        {
            glIsFPGAConfigured = true;
            Cy_Debug_AddToLog(3, CYAN);
            Cy_Debug_AddToLog (3,"FPGA Configuration Done \n\r");
            Cy_Debug_AddToLog(3, COLOR_RESET);
            Cy_GPIO_Clr(HDMI_KIT_FPGA_SOFTRESET_PORT,HDMI_KIT_FPGA_SOFTRESET_PIN);
            Cy_SysLib_Delay(10);
            Cy_GPIO_Set(HDMI_KIT_FPGA_SOFTRESET_PORT,HDMI_KIT_FPGA_SOFTRESET_PIN);
            DBG_APP_TRACE("FPGA Soft Reset Done\r\n");
        }
        else
        {
            Cy_Debug_AddToLog(1, RED);
            Cy_Debug_AddToLog (1,"FPGA Configuration Failed \n\r");
            Cy_Debug_AddToLog(1, COLOR_RESET);
        }

#if (!CONFIG_CDONE_AS_INPUT)
        DELAY_MILLI(3000);
        /*Re-init SMIF pins. This should not affect the RISC-V boot
         *also as there is sufficient delay of 3 seconds and SOC SPI activity was seen for <2 secs*/
        Cy_QSPI_ConfigureSMIFPins(true);
#endif
    }
    return glIsFPGAConfigured;
} /* End of Cy_FPGAConfigure() */
