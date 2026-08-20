/******************************************************************************
* \file cy_usb_qspi.h
* \version 1.0
*
* Defines the interfaces used to write FPGA data to QSPI flash
* and configuring the FPGA.
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

#ifndef _CY_USB_QSPI_H_
#define _CY_USB_QSPI_H_

#include "cy_pdl.h"
#include "cy_usb_i2c.h"
#include "cy_debug.h"
#include "cy_usb_app.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*****************************************************************************
*                          SMIF enums
******************************************************************************/
typedef enum cy_en_spiFlashType_t
{
    IFX_SFL = 0x80,
    IFX_SFS = 0x81,
    FLASH_UNKNOWN = 0xFF
}cy_en_spiFlashType_t;

typedef enum cy_en_passiveSerialMode_t
{
    PASSIVE_x4,
    PASSIVE_x8
}cy_en_passiveSerialMode_t;

typedef enum cy_en_flash_index_t
{
  SPI_FLASH_0    = 0,
  SPI_FLASH_1    = 1,
  DUAL_SPI_FLASH = 2,
  NUM_SPI_FLASH,
}cy_en_flash_index_t;

typedef struct cy_stc_cfi_erase_block_info_t
{
  uint32_t numSectors;
  uint32_t sectorSize;
  uint32_t startingAddress;
  uint32_t lastAddress;
} cy_stc_cfi_erase_block_info_t ;

/* EZ-USB FX Control Center application appends a metadata section to any file it programs to the external flash.
 * This can be used by the application using the flash to find out details such as start address, size etc. about the programmed file */

typedef struct
{
    uint32_t startSignature;
    uint32_t fpgaFileStartAddress;
    uint32_t fpgaFileSize;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t endSignature;
}cy_stc_externalFlashMetadata_t;


/*****************************************************************************
*                          SMIF macros                                       *
******************************************************************************/

#define FPGA_CONFIG_MODE                                 (PASSIVE_SERIAL_MODE)
#define CY_APP_QSPI_METADATA_ADDRESS            (0x00)
#define CY_APP_QSPI_METADATA_SIZE               (24)
#define CY_APP_QSPI_METADATA_SIGNATURE          (0x23584649) /*IFX#*/

#if (FPGA_CONFIG_MODE == PASSIVE_SERIAL_MODE)
/*
 * CDONE is an I/O pin for the FPGA. It can be configurd as a output and pulled low to delay FPGA's User Mode entry.
 * This approach can be used if the FPGA also has a softcore SoC implementation and the SoC has to boot from SPI Flash.
 * This will create a scenario where there will be two SPI masters trying to access the same SPI Flash.
 * After sending the FPGA bin file, FX10 will keep CDONE low to hold FPGA in config mode.
 * Then FX10 will release control of the SPI pins (High-Z) and make CDONE also High-Z to allow FPGA to enter user mode and proceed to SoC boot.
 *
 * Choose CONFIG_CDONE_AS_INPUT=0 for suppporting FPGA implementations with SoC (CIS_ISP).
 * Choose CONFIG_CDONE_AS_INPUT=1 for only FPGA bin files.
*/
#define EFINIX_FPGA_SOC_MERGED_FILE_SIZE          (3756522)
#define CONFIG_CDONE_AS_INPUT                     (1)
#else
#define CONFIG_CDONE_AS_INPUT                     (1)
#endif


#define SMIF_HW SMIF0

#define SMIF_CLK_HSIOM                          (P6_0_SMIF_SPI_CLK)
#define SMIF_CLK_PORT                           (P6_0_PORT)
#define SMIF_CLK_PIN                            (P6_0_PIN)

#define SMIF_SELECT0_HSIOM                      (P6_1_SMIF_SPI_SELECT0)
#define SMIF_SELECT0_PORT                       (P6_1_PORT)
#define SMIF_SELECT0_PIN                        (P6_1_PIN)

#define SMIF_SELECT1_HSIOM                      (P6_2_SMIF_SPI_SELECT1)
#define SMIF_SELECT1_PORT                       (P6_2_PORT)
#define SMIF_SELECT1_PIN                        (P6_2_PIN)

#define SMIF_DATA0_HSIOM                        (P7_0_SMIF_SPI_DATA0)
#define SMIF_DATA0_PORT                         (P7_0_PORT)
#define SMIF_DATA0_PIN                          (P7_0_PIN)

#define SMIF_DATA1_HSIOM                        (P7_1_SMIF_SPI_DATA1)
#define SMIF_DATA1_PORT                         (P7_1_PORT)
#define SMIF_DATA1_PIN                          (P7_1_PIN)

#define SMIF_DATA2_HSIOM                        (P7_2_SMIF_SPI_DATA2)
#define SMIF_DATA2_PORT                         (P7_2_PORT)
#define SMIF_DATA2_PIN                          (P7_2_PIN)

#define SMIF_DATA3_HSIOM                        (P7_3_SMIF_SPI_DATA3)
#define SMIF_DATA3_PORT                         (P7_3_PORT)
#define SMIF_DATA3_PIN                          (P7_3_PIN)

#define SMIF_DATA4_HSIOM                        (P7_4_SMIF_SPI_DATA4)
#define SMIF_DATA4_PORT                         (P7_4_PORT)
#define SMIF_DATA4_PIN                          (P7_4_PIN)

#define SMIF_DATA5_HSIOM                        (P7_5_SMIF_SPI_DATA5)
#define SMIF_DATA5_PORT                         (P7_5_PORT)
#define SMIF_DATA5_PIN                          (P7_5_PIN)

#define SMIF_DATA6_HSIOM                        (P7_6_SMIF_SPI_DATA6)
#define SMIF_DATA6_PORT                         (P7_6_PORT)
#define SMIF_DATA6_PIN                          (P7_6_PIN)

#define SMIF_DATA7_HSIOM                        (P7_7_SMIF_SPI_DATA7)
#define SMIF_DATA7_PORT                         (P7_7_PORT)
#define SMIF_DATA7_PIN                          (P7_7_PIN)

#define T120_CDONE_PIN                          (P4_4_PIN)
#define T120_CDONE_PORT                         (P4_4_PORT)

#define T120_INIT_RESET_PORT                    (P4_3_PORT)
#define T120_INIT_RESET_PIN                     (P4_3_PIN)

#define T120_PROGRAM_N_PORT                     (P6_4_PORT)
#define T120_PROGRAM_N_PIN                      (P6_4_PIN)

#define T120_FPGA_SSN_PORT                      (P6_3_PORT)
#define T120_FPGA_SSN_PIN                       (P6_3_PIN)

#define HDMI_KIT_USER_LED_PORT                  (P13_1_PORT)
#define HDMI_KIT_USER_LED_PIN                   (P13_1_PIN)

#define HDMI_KIT_FPGA_SOFTRESET_PORT            (P11_5_PORT)
#define HDMI_KIT_FPGA_SOFTRESET_PIN             (P11_5_PIN)

#define CY_APP_QSPI_STATUS_1_READ_CMD           (0x05)
#define CY_APP_QSPI_STATUS_2_READ_CMD           (0x07)
#define CY_APP_QSPI_QREAD_CMD                   (0xEB)
#define CY_APP_QSPI_QREAD_MODE_CMD              (0x01)
#define CY_APP_QSPI_READ_ID_CMD                 (0x9F)

#define CY_APP_QSPI_NUM_ADDRESS_BYTES           (3)
#define CY_APP_QSPI_ID_SIZE                     (16)
#define CY_APP_QSPI_QREAD_MODE_WIDTH            (CY_SMIF_WIDTH_QUAD)
#define CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFS  (8)
#define CY_APP_QSPI_QREAD_NUM_DUMMY_CYCLES_SFL  (4)
#define CY_APP_QSPI_DEVICE_ID_OFFSET            (5)
#define MAX_BUFFER_SIZE                         (2048u)
#define TEST_BITFILE_SIZE                       (3550770)
#define TEST_BITFILE_SIZE_TI180                 (4139280)
#define MAX_DUMMY_CYCLES_COUNT                  (8192)
#define EFINIX_MAX_CONFIG_FILE_SIZE             (6262080)

#define FPGA_ADDRESS_OFFSET                     (0)
#define DUMMY_CYCLE                             (150)

/* SPI Commands for Infineon SFS256 Flash */
#define CY_SPI_CONFIG_REG_READ_CMD              (0x35)
#define CY_SPI_WRITE_ENABLE_CMD                 (0x06)
#define CY_SPI_READ_ID_CMD                      (0x9F)
#define CY_SPI_STATUS_READ_CMD                  (0x05)
#define CY_SPI_PROGRAM_CMD                      (0x02)
#define CY_SPI_SECTOR_ERASE_CMD                 (0xD8)
#define CY_SPI_HYBRID_SECTOR_ERASE_CMD          (0x20)
#define CY_APP_SPI_READ_ANY_REG_CMD             (0x65)
#define CY_APP_SPI_WRITE_ANY_REGISTER_CMD       (0x71)
#define CY_APP_SPI_RESET_ENABLE_CMD             (0x66)
#define CY_APP_SPI_SW_RESET_CMD                 (0x99)
#define CY_SPI_QPI_PROGRAM_CMD                  (0x02)
#define CY_SPI_QREAD_MODE_CMD                   (0x01)
#define CY_SPI_QPI_READ_CMD                     (0xEB)
#define CY_SPI_READ_CMD                         (0x03)

#define CY_CFI_DEVICE_SIZE_OFFSET               (0x27)
#define CY_CFI_ERASE_NUM_SECTORS_OFFSET         (0x2D)
#define CY_CFI_ERASE_REGION_SIZE_INFO_SIZE      (0x04)
#define CY_CFI_ERASE_SECTOR_SIZE_OFFSET         (0x2F)
#define CY_CFI_MAX_SIZE_NUM_ERASE_SECTORS       (0xFF)
#define CY_CFI_NUM_ERASE_REGION_OFFSET          (0x2C)
#define CY_QSPI_NUM_DUMMY_CYCLES                (0x08) /*SFS256 Flash*/

#define SPI_ADDRESS_BYTE_COUNT                  (3)
#define CY_FLASH_ID_LENGTH                      (0x08)
#define CY_CFI_TABLE_LENGTH                     (0x56)
#define CY_SFS256_CR1V_REG                      (0x800002)
#define CY_SFS256_CR2V_REG                      (0x800003)
#define CY_SFS256_CR3V_REG                      (0x800004)
#define CY_SPI_WRITE_ENABLE_LATCH_MASK          (0x02)

extern cy_stc_smif_context_t qspiContext;

/*****************************************************************************
* Function Name:Cy_FPGAConfigPins(cy_stc_usb_app_ctxt_t *pAppCtxt
                                , cy_en_fpgaConfigMode_t mode)
******************************************************************************
* Summary:
* Function to Configure pins for FPGA
*
* Parameters:
*  \param pAppCtxt
*   User Context
*  \param mode
*   FPGA mode
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_FPGAConfigPins(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_fpgaConfigMode_t mode);

/*****************************************************************************
* Function Name:Cy_FPGAConfigure(cy_stc_usb_app_ctxt_t *pAppCtxt
                                , cy_en_fpgaConfigMode_t mode)
******************************************************************************
* Summary:
* Fucntion to configure FPGA
*
* Parameters:
*  \param pAppCtxt
*   User Context
*  \param mode
*   FPGA mode
*
* Return:
*  Does not return.
*****************************************************************************/
bool Cy_FPGAConfigure(cy_stc_usb_app_ctxt_t *pAppCtxt,cy_en_fpgaConfigMode_t mode);


/*****************************************************************************
* Function Name:Cy_QSPI_ConfigureSMIFPins(bool init)
******************************************************************************
* Summary:
* Function to Configure SMIF pins
*
* Parameters:
*  \param pAppCtxt
*   User Context
*  \param init
*   Initialize
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_QSPI_ConfigureSMIFPins(bool init);


/*****************************************************************************
* Function Name:Cy_QSPI_Start(cy_stc_usb_app_ctxt_t *pUsbApp,
                                cy_stc_hbdma_buf_mgr_t *hbw_bufmgr)
******************************************************************************
* Summary:
* Function to start QSPI block
*
* Parameters:
*  \param pUsbApp
*   User Context
*  \param hbw_bufmgr
*   Pointer to DMA buffer
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_QSPI_Start(cy_stc_usb_app_ctxt_t *pUsbApp,cy_stc_hbdma_buf_mgr_t *hbw_bufmgr);

/*****************************************************************************
* Function Name:Cy_QSPI_ReadID(cy_en_smif_slave_select_t slaveSelect,
                                     uint8_t *idBuffer)
******************************************************************************
* Summary:
* Function to Read ID from QSPI
*
* Parameters:
*  \param slaveSelect
*   Slave Select line
*  \param idBuffer
*   Pointer to buffer
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_QSPI_ReadID(cy_en_smif_slave_select_t slaveSelect, uint8_t *idBuffer);

/*****************************************************************************
* Function Name:Cy_USB_QSPIChipErase(cy_en_smif_slave_select_t slaveSelect)
******************************************************************************
* Summary:
* Function to Erase the Chip
*
* Parameters:
*  \param slaveSelect
*   Slave Select line
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_USB_QSPIChipErase(cy_en_smif_slave_select_t slaveSelect);


/*****************************************************************************
* Function Name:Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray,
                            uint8_t numAddressBytes)
******************************************************************************
* Summary:
* Function to change address to array
*
* Parameters:
*   \param value
*   Value to convert to array
*  \param byteArray
*   Pointer to array
*  \param numAddressBytes
*   Number of bytes in address
*
* Return:
*  returns data
*****************************************************************************/
void Cy_SPI_AddressToArray(uint32_t value, uint8_t *byteArray, uint8_t numAddressBytes);

/*****************************************************************************
* Function Name:Cy_QSPI_IsMemBusy(cy_en_flash_index_t flashIndex))
******************************************************************************
* Summary:
* Function to handler QSPI Vendor commands
*
* Parameters:
*   \param flashIndex
*   Flash Index
*
* Return:
*  returns 0 if Flash is not busy
*****************************************************************************/
bool Cy_QSPI_IsMemBusy(cy_en_flash_index_t flashIndex);


/*****************************************************************************
* Function Name:Cy_SPI_FlashInit(cy_en_flash_index_t flashIndex, bool qpiEnable)
******************************************************************************
* Summary:
* Function to handler QSPI Vendor commands
*
* Parameters:
*   \param flashIndex
*   Flash Index
*   \param qpiEnable
*   Enable QPI Mode
*
* Return:
*  returns cy_en_smif_status_t
*****************************************************************************/
cy_en_smif_status_t Cy_SPI_FlashInit (cy_en_flash_index_t flashIndex, bool quadEnable, bool qpiEnable);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_QSPI_H_ */
/* End of File */
