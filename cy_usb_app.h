/******************************************************************************
* \file cy_usb_app.h
* \version 1.0
*
* Defines the interfaces used in the FX20 USB Video Class application.
*
*******************************************************************************
* \copyright
* (c) (2025), Cypress Semiconductor Corporation (an Infineon company) or
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

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* UVC frame with partial buffer size */
/* Set this Macro to (0) when streaming in Linux. This flag does not effect in Auto DMA mode */
#define PARTIAL_FRAME_EN               (0) /* Default value = 0 */

/* Macro for HW Board Selection */
#define FX20_Ti180                     (1)

#if WL_EN && PORT1_EN
/* can not use Port1 along with Wide Link */
#error "INVALID: WL_EN requires PORT1_EN to be 0"
#endif

#if LVCMOS_EN && CUSTOM_TRAIN_ENABLE
/* Firmware based PHY training is not needed for LVCMOS mode */
#error "INVALID: LVCMOS_EN requires CUSTOM_TRAIN_ENABLE to be 0"
#endif

#if LVCMOS_EN && (!LVCMOS_DDR_EN) && LVDS_LOW_POWER_EN
/* FPGA V70 does not support Low power mode in LVCMOS mode */
#error "INVALID: LVDS_LOW_POWER_EN can't be used in LVCMOS SDR mode. Set LVDS_LOW_POWER_EN=0"
#endif

#if !LVCMOS_EN && DEEPSLEEP_EN && !LVDS_LOW_POWER_EN
#error "INVALID: DEEPSLEEP_EN requires LVDS_LOW_POWER_EN=1 to reduce the power consumption \
and re-training the LVDS block"
#endif

#if DEEPSLEEP_EN && LVCMOS_EN && LVCMOS_DDR_EN && !WL_EN && LVDS_LOW_POWER_EN
/* Training is needed LVCMOS DDR WL mode when CPU and LVDS/LVCMOS
 block enters into low power mode */
#error "INVALID: DEEPSLEEP_EN requires LVCMOS_EN=0 or LVCMOS_DDR_EN=1 and WL_EN=1"
#endif

#if DEEPSLEEP_EN && LVCMOS_EN && LVCMOS_DDR_EN && WL_EN && !LVDS_LOW_POWER_EN
/* LVDS_LOW_POWER_EN should set where training is needed (LVDS, LVCMOS DDR WL) */
#error "INVALID: DEEPSLEEP_EN requires LVCMOS_EN=0 or LVCMOS_DDR_EN=1 and WL_EN=1 and LVDS_LOW_POWER_EN=1"
#endif

#if SONY_IMX715_EN && THINE_EN
/* Code example supports single camera at a time */
#error LOG_COLOR(RED, "SELECT ONLY ONE SENSOR INTERFACE \n\r");
#endif

/* INMD enabled? DMA channel should be auto */
#if INMD_EN
#define MANUAL_DMA_CHANNEL      (0)                     /* Do not modify */
#define AUTO_DMA_CHANNEL        (!MANUAL_DMA_CHANNEL)   /* Do not modify */
#else
/* DMA channel and UVC header configuration */
#define MANUAL_DMA_CHANNEL      (1)                     /* Set (1) for Manual DMA configuration, and (0) for Auto DMA configuration */
#define AUTO_DMA_CHANNEL        (!MANUAL_DMA_CHANNEL)
#endif

#if MANUAL_DMA_CHANNEL
#define UVC_HEADER_BY_FX20      (1)                     /* Set (1) for adding the header in Firmware, Set (0) for adding the header by FPGA */
#else
#define UVC_HEADER_BY_FX20      (0)                     /* Do not modify */
#endif

#define UVC_HEADER_BY_FPGA      (AUTO_DMA_CHANNEL || (!UVC_HEADER_BY_FX20))


/* FPGA Configuration mode selection*/
typedef enum cy_en_fpgaConfigMode_t
{
    ACTIVE_SERIAL_MODE,
    PASSIVE_SERIAL_MODE
} cy_en_fpgaConfigMode_t;
/*********END*********/

#if FX20_Ti180 /* Ti180 DVK support LVCMOS DDR/SDR, LVDS modes */

    #if (LVCMOS_EN && ((!LVCMOS_DDR_EN) || (!WL_EN)))
    /* LINK training is not needed for LVCMOS SDR and LVCMOS DDR NL modes */
    #define LINK_TRAINING_EN           (0)
    #else
    /* Link Training is needed for LVCMOS DDR WL and LVDS modes */
    #define LINK_TRAINING_EN           (1)
    #endif

    #define FPGA_CONFIG_MODE           (PASSIVE_SERIAL_MODE)
#endif

#if (FPGA_CONFIG_MODE == PASSIVE_SERIAL_MODE)
/*
 * CDONE is an I/O pin for the FPGA. It can be configurd as a output and pulled
 * low to delay FPGA's User Mode entry.
 * This approach can be used if the FPGA also has a softcore SoC implementation
 * and the SoC has to boot from SPI Flash.
 * This will create a scenario where there will be two SPI masters trying to
 * access the same SPI Flash.
 * After sending the FPGA bin file, FX20 will keep CDONE low to hold FPGA in config mode.
 * Then FX20 will release control of the SPI pins (High-Z) and make CDONE also
 * High-Z to allow FPGA to enter user mode and proceed to SoC boot.
 *
 * Choose CONFIG_CDONE_AS_INPUT=0 for supporting FPGA implementations with SoC (CIS_ISP).
 * Choose CONFIG_CDONE_AS_INPUT=1 for only FPGA bin files.
*/
#define EFINIX_FPGA_SOC_MERGED_FILE_SIZE (3756522)
/* CDONE pin as to be configured as output with paasive FPGA configuration is
 * used along with SoC (CIS_ISP). In case SoC configuration is not used then
 * CDONE should be INPUT only */
#define CONFIG_CDONE_AS_INPUT          (1) /* Set this to '0' when FPGA contains SoC */
#else /* ACTIVE_SERIAL_MODE */
/* No Need to control CDONE pin when Active FPGA configuration is used*/
#define CONFIG_CDONE_AS_INPUT          (1) /* Do not modify */
#endif


#if FX20_Ti180
#define LINK_READY_CTL_PORT            (PORT1_EN)
#define LINK_READY_CTL_PIN             (CY_LVDS_PHY_GPIO_CTL6)
#endif

/* I2C Related macro */
#define FPGA_I2C_ADDRESS_WIDTH         (2)
#define FPGA_I2C_DATA_WIDTH            (1)

/* GPIO port pins */
#define TI180_CRESET_GPIO              (P4_3_GPIO)
#define TI180_CRESET_GPIO_PORT         (P4_3_PORT)
#define TI180_CRESET_GPIO_PIN          (P4_3_PIN)

/* GPIO port pins */
#define TI180_CDONE_GPIO               (P4_4_GPIO)
#define TI180_CDONE_GPIO_PORT          (P4_4_PORT)
#define TI180_CDONE_GPIO_PIN           (P4_4_PIN)

/* P13.0 is a PMODE functionality in bootMode,
   however application firmware can reconfigure. */
#define PMODE_GPIO                     (P13_0_GPIO)
#define PMODE_PORT                     (P13_0_PORT)
#define PMODE_PIN                      (P13_0_PIN)

/* P13.1 is used for USER_LED functionality. */
#define USER_LED_GPIO                  (P13_1_GPIO)
#define USER_LED_PORT                  (P13_1_PORT)
#define USER_LED_PIN                   (P13_1_PIN)

/* P8.0 is used for User UART-RX (SCB1). */
#define UART_RX_HSIOM                  (P8_0_SCB1_UART_RX)
#define UART_RX_PORT                   (P8_0_PORT)
#define UART_RX_PIN                    (P8_0_PIN)

/* P8.1 is used for User UART-TX (SCB1). */
#define UART_TX_HSIOM                  (P8_1_SCB1_UART_TX)
#define UART_TX_PORT                   (P8_1_PORT)
#define UART_TX_PIN                    (P8_1_PIN)

#define DELAY_MICRO(us)                Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms)                Cy_SysLib_Delay(ms)

#define ASSERT(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) Cy_CheckStatusAndHandleFailure(__func__, __LINE__, condition, value, false, failureHandler);

#define SET_BIT(byte, mask)     (byte) |= (mask)
#define CLR_BIT(byte, mask)     (byte) &= ~(mask)
#define CHK_BIT(byte, mask)     ((byte) & (mask))

#define RED         "\033[0;31m"
#define CYAN        "\033[0;36m"
#define COLOR_RESET "\033[0m"

/* Regular text */
#define BLK         "\033[0;30m"
#define GRN         "\033[0;32m"
#define YEL         "\033[0;33m"
#define BLU         "\033[0;34m"
#define MAG         "\033[0;35m"
#define CYN         "\033[0;36m"
#define WHT         "\033[0;37m"

/* Regular bold text */
#define BBLK        "\033[1;30m"
#define BWHT        "\033[1;37m"

/* Regular underline text */
#define UWHT        "\033[4;37m"

/* High intensty text */
#define HBLK        "\033[0;90m"
#define HRED        "\033[0;91m"
#define HGRN        "\033[0;92m"
#define HYEL        "\033[0;93m"
#define HBLU        "\033[0;94m"
#define HMAG        "\033[0;95m"
#define HCYN        "\033[0;96m"
#define HWHT        "\033[0;97m"

/* Bold high intensity text */
#define BHBLK       "\033[1;90m"
#define BHWHT       "\033[1;97m"

#define LOG_COLOR(...)      Cy_Debug_AddToLog(1,CYAN);\
                            Cy_Debug_AddToLog(1,__VA_ARGS__); \
                            Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_ERROR(...)      Cy_Debug_AddToLog(1,RED);\
                            Cy_Debug_AddToLog(1,__VA_ARGS__); \
                            Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_CLR(CLR, ...)   Cy_Debug_AddToLog(1,CLR);\
                            Cy_Debug_AddToLog(1,__VA_ARGS__); \
                            Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_TRACE()         LOG_COLOR("-->[%s]:%d\r\n",__func__,__LINE__);

#define APP_WATCHDOG_INTERVAL_MS       (1200)
#define APP_ONE_SEC_INTERVAL_MS        (1000)

/* Related to WINUSB */
#define MS_VENDOR_CODE                 (0xF1)
extern uint8_t glOsString[];
extern uint8_t glOsCompatibilityId[];
extern uint8_t glOsFeature[];

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT          (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN           (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR          (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE              (0u)

/* Number of buffers used for PDM data transfer. */
#define PDM_APP_BUFFER_CNT             (12u)

/* Refer FX20 Architecture TRM to know supported PHY Training pattern bytes*/
#define PHY_TRAINING_PATTERN_BYTE      (0x9C)
#define LINK_TRAINING_PATTERN_BYTE     (0x125A4B78)

/* USBD layer return code shared between USBD layer and Application layer. */
typedef enum cy_en_usb_app_ret_code_ {
    CY_USB_APP_STATUS_SUCCESS=0,
    CY_USB_APP_STATUS_FAILURE,
}cy_en_usb_app_ret_code_t;

typedef enum cy_en_streamControl_t
{
    STOP,
    START
} cy_en_streamControl_t;

typedef enum cy_en_fpga_i2c_reg_map_
{
    /* Common Register Info */
    FPGA_MAJOR_VERSION_ADDRESS             = 0x00,
    FPGA_MINOR_VERSION_ADDRESS             = 0x00,

    FPGA_UVC_U3V_SELECTION_ADDRESS         = 0x01,
    FPGA_UVC_ENABLE                        = 1,
    FPGA_U3V_ENABLE                        = 0,
    FPGA_FX20_USBHS_ENABLE                 = 0x80,

    FPGA_UVC_HEADER_CTRL_ADDRESS           = 0x02,
    FPGA_UVC_HEADER_DISABLE                = 0x00, /* FX10 will add UVC Header */
    FPGA_UVC_HEADER_ENABLE                 = 0x01,
    FPGA_INMD_UVC_HEADER_ENABLE            = 0x02,

    FPGA_LVDS_PHY_TRAINING_ADDRESS         = 0x03,
    FPGA_LVDS_PHY_TRAINING_DATA            = 0x39,

    FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS = 0x04,
    FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS = 0x05,
    FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS = 0x06,
    FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS = 0x07,

    FPGA_ACTIVE_DIVICE_MASK_ADDRESS        = 0x08,

    FPGA_LOW_PWR_MODE_ADDRESS              = 0x09,
    LVDS_EXIT_LOWPOWER_MODE                = 0x00, /* Clear all bits to exit low power mode */
    LVDS_L1_MODE_ENTER                     = 0x01, /* Bit 0 to enter L0 mode */
    LVDS_L3_MODE_ENTER                     = 0x02, /* Bit 1 to enter L3 mode */


    FPGA_PHY_LINK_CONTROL_ADDRESS          = 0x0A,
    FPGA_TRAINING_DISABLE                  = 0x00,
    FPGA_PHY_CONTROL                       = 0x01, /* PHY Training is required */
    FPGA_LINK_CONTROL                      = 0x02, /* Link Training is required */
    PHY_TRAINING_DONE                      = 0X20, /* PHY training is completed */
    P0_TRAINING_DONE                       = 0X40, /* Port 0 training is completed*/
    P1_TRAINING_DONE                       = 0X80, /* Port 1 training is completed */

    FPGA_EXT_CONTROLLER_STATUS_ADDRESS     = 0X0B,
    DMA_READY_STATUS                       = 0x01, /* DMA Ready flag status */
    DDR_CONFIG_STATUS                      = 0x02, /* DDR configuration status */
    DDR_BUSY_STATUS                        = 0x04, /* DDR Controller busy status */
    DDR_CMD_QUEUE_FULL_STATUS              = 0x08, /* Command queue full status */
    DATPATH_IDLE_STATUS                    = 0x10, /* Datapath is idle or not */

    FPGA_RESERVED_ADDRESS                  = 0X0C,

    FPGA_LVDS_GEAR_RATIO_ADDRESS           = 0X0D,
    GEAR_RATIO_DEFLT_2_1                   = 0x00,
    GEAR_RATIO_2_1                         = 0x02,
    GEAR_RATIO_4_1                         = 0x04,
    GEAR_RATIO_8_1                         = 0x08,
    /* Device related Info start */
    /* Device offset address */
    DEVICE0_OFFSET                         = 0x20,
    DEVICE1_OFFSET                         = 0x3C,
    DEVICE2_OFFSET                         = 0x58,
    DEVICE3_OFFSET                         = 0x74,
    DEVICE4_OFFSET                         = 0x90,
    DEVICE5_OFFSET                         = 0xAC,
    DEVICE6_OFFSET                         = 0xC8,
    DEVICE7_OFFSET                         = 0xE4,

    FPGA_DEVICE_STREAM_ENABLE_ADDRESS      = 0x00,
    CAMERA_APP_DISABLE                     = 0x00,
    DMA_CH_RESET                           = 0x01,
    CAMERA_APP_ENABLE                      = 0x02,
    APP_STOP_NOTIFICATION                  = 0x04,

    FPGA_DEVICE_STREAM_MODE_ADDRESS        = 0x01,
    NO_CONVERSION                          = 0,
    INTERLEAVED_MODE                       = 0x01,
    STILL_CAPTURE                          = 0x02,
    MONO_8_CONVERSION                      = 0x04,
    YUV422_420_CONVERSION                  = 0x08,

    DEVICE_IMAGE_HEIGHT_LSB_ADDRESS        = 0x02,
    DEVICE_IMAGE_HEIGHT_MSB_ADDRESS        = 0x03,
    DEVICE_IMAGE_WIDTH_LSB_ADDRESS         = 0x04,
    DEVICE_IMAGE_WIDTH_MSB_ADDRESS         = 0x05,

    DEVICE_FPS_ADDRESS                     = 0x06,

    DEVICE_PIXEL_WIDTH_ADDRESS             = 0x07,
    _8_BIT_PIXEL                           = 8,
    _12BIT_PIXEL                           = 12,
    _16BIT_PIXEL                           = 16,
    _24BIT_PIXEL                           = 24,
    _32BIT_PIXEL                           = 32,

    DEVICE_SOURCE_TYPE_ADDRESS             = 0x08,
    INTERNAL_COLORBAR                      = 0x00,
    HDMI_SOURCE                            = 0x01,
    MIPI_SOURCE                            = 0x02,

    DEVICE_FLAG_STATUS_ADDRESS             = 0x09,
    SLAVE_FIFO_ALMOST_EMPTY                = 0x01, /* Slave FIFO almost empty status */
    INTER_MED_FIFO_EMPTY                   = 0x02, /* Intermediate FIFO empty status */
    INTER_MED_FIFO_FULL                    = 0x04, /* Intermediate FIFO full status */
    DDR_FULL_FRAME_WRITE_COMPLETE          = 0x10, /* DDR write status (Full frame write complete) */
    DDR_FULL_FRAME_READ_COMPLETE           = 0x20, /* DDR write status (Full frame read complete) */

    DEVICE_MIPI_STATUS_ADDRESS             = 0x0A,

    DEVICE_HDMI_SOURCE_INFO_ADDRESS        = 0x0B, /* Read before write */
    HDMI_DISCONECT                         = 0x00, /* Bit 0- Source connection status */
    THIN_MIPI                              = 0x00, /* Bit 0- */
    HDMI_CONNECT                           = 0x01, /* Bit 0- Source connection status */
    HDMI_DUAL_CH                           = 0x02, /* Bit 1- 0 for single channel and 1 for dual channel */
    HDMI_COLOR_FORMAT_YUV444               = 0x04, /* Bit 2- 0 for YUV 422 mode and 1 for YUV 444 mode */
    SONY_CIS                               = 0x10, /* Bit 4- Enable CIS ISP IP (Valid for only MIPI source) */
    SONY_MIPI                              = 0x20, /* Bit 5- Enable Crop Algorithm (Valid for only MIPI source) */

    DEVICE_U3V_STREAM_MODE_ADDRESS         = 0x0C,
    DEVICE_U3V_CHUNK_MODE_ADDRESS          = 0x0D,
    DEVICE_U3V_TRIGGER_MODE_ADDRESS        = 0x0E,
    DEVICE_ACTIVE_TREAD_INFO_ADDRESS       = 0x0F,
    DEVICE_THREAD1_INFO_ADDRESS            = 0x10,
    DEVICE_THREAD2_INFO_ADDRESS            = 0x11,
    DEVICE_THREAD1_SOCKET_INFO_ADDRESS     = 0x12,
    DEVICE_THREAD2_SOCKET_INFO_ADDRESS     = 0x13,

    DEVICE_FLAG_INFO_ADDRESS               = 0x14,
    FX10_READY_TO_REC_DATA                 = 0x08,
    NEW_UVC_PACKET_START                   = 0x02,
    NEW_FRAME_START                        = 0x01,

    DEVICE_COUNTER_CRC_INFO_ADDRESS        = 0x15,
    DEVICE_BUFFER_SIZE_LSB_ADDRESS         = 0x16,
    DEVICE_BUFFER_SIZE_MSB_ADDRESS         = 0x17,

} cy_en_fpga_i2c_reg_map_t;

/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 * It maintains some usb system information which comes from USBD and it also
 * maintains info about functionality.
 */
struct cy_stc_usb_app_ctxt_
{
    uint8_t firstInitDone;
    cy_en_usb_device_state_t devState;
    cy_en_usb_device_state_t prevDevState;
    cy_en_usb_speed_t devSpeed;
    uint8_t devAddr;
    uint8_t activeCfgNum;
    uint8_t prevAltSetting;
    cy_en_usb_enum_method_t enumMethod;
    cy_en_usb_speed_t desiredSpeed;

    cy_stc_app_endp_dma_set_t endpInDma[CY_USB_MAX_ENDP_NUMBER];
    cy_stc_app_endp_dma_set_t endpOutDma[CY_USB_MAX_ENDP_NUMBER];
    DMAC_Type *pCpuDmacBase;
    DW_Type *pCpuDw0Base;
    DW_Type *pCpuDw1Base;

    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt;
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt;
    cy_stc_hbdma_channel_t *hbBulkInChannel;
    cy_stc_scb_uart_context_t *pUartCtxt;       /* SCB-UART context pointer. */

    /* UVC specific fields. */
    uint8_t uvcInEpNum;                         /* Index of UVC streaming endpoint. */

    bool isLpmEnabled;                          /** Whether LPM transitions are enabled. */
    uint32_t lpmEnableTime;                     /** Timestamp at which LPM should be re-enabled. */
    /* Global Task handles */
    TaskHandle_t uvcDevicetaskHandle;           /* UVC application task. */
    QueueHandle_t uvcMsgQueue;                  /* Message queue used to send messages to the UVC task. */

    bool usbConnectDone;
    bool vbusChangeIntr;                        /* VBus change interrupt received flag. */
    bool vbusPresent;                           /* VBus presence indicator flag. */
    bool usbConnected;                          /* Whether USB connection is enabled. */
    bool usbFsPortActive;
    bool printStartupMessage;
    bool port1Enabled;
    uint8_t glfpga_status;
    volatile uint8_t glPrintFlag;

    TimerHandle_t vbusDebounceTimer;            /* VBus change debounce timer handle. */
    TimerHandle_t FpgaStatusTimer;
    TimerHandle_t fpsTimer;
    TimerHandle_t evtLogTimer;                  /* Timer to print eventLog. */

    uint16_t glDmaProdBufSize;
    uint32_t glfps;
    uint32_t glDmaBufCnt;
    uint32_t glDmaBufCnt_prv;
    volatile uint32_t glProd;
    volatile uint32_t glCons;
    uint32_t glProdCount;
    uint32_t glConsCount;
    uint32_t glFrameSizeTransferred;
    uint32_t glFrameSize;
    volatile uint32_t glFrameCount;
    volatile uint32_t glPartialBufSize;

    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;

    uint32_t *pUsbEvtLogBuf;


#if AUDIO_IF_EN
    /* UAC interface specific fields. */
    cy_stc_hbdma_channel_t *pPDMToUsbChn;       /* PDM to USB DMA channel handle. */
    uint8_t *pPDMRxBuffer[PDM_APP_BUFFER_CNT];  /* Pointer of buffers to read PDM data into. */
    uint16_t pdmRxDataLen[PDM_APP_BUFFER_CNT];  /* Amount of data in each PDM RX buffer. */
    uint8_t  pdmRxBufIndex;                     /* Index of current PDM read buffer. */
    uint8_t  pdmRxFreeBufCount;                 /* Number of free PDM RX buffers. */
    uint8_t  pdmPendingDmaFlag;                 /* Flag indicating whether DMA transfer is pending on
                                                    each PDM RX channel. */
    TaskHandle_t  uacAppTaskHandle;             /* Handle to the UAC application task. */
    QueueHandle_t uacMsgQueue;                  /* Handle to UAC application message queue. */
    TimerHandle_t pdmActivateTimer;             /* Handle of timer used to activate PDM channels. */
#endif /* AUDIO_IF_EN */

    uint8_t fpgaVersion;
    uint8_t glpassiveSerialMode;
    uint8_t *qspiWriteBuffer;
    uint8_t *qspiReadBuffer;
#if LVDS_LOW_POWER_EN
    uint8_t bmLinkL1EnterStatus;                 /* Bitmask that shows l1 status for each link */
    uint8_t bmLinkL3EnterStatus;                 /* Bitmask that shows l3 status for each link */
#endif
};

typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;
extern cy_stc_hbdma_channel_t lvdsLbPgmChannel;

/*****************************************************************************
* Function Name: Cy_HbDma_LoopbackCb(cy_stc_hbdma_channel_t *handle, cy_en_hbdma_cb_type_t type,
                              cy_stc_hbdma_buff_status_t *pbufStat, void *userCtx)
******************************************************************************
* Summary:
* HBDMA callback function to commit data to loopback channel
*
* \param handle
* HBDMA channel handle
*
* \param cy_en_hbdma_cb_type_t
* HBDMA channel type
*
* \param pbufStat
* fHBDMA buffer status
*
* \param userCtx
* user context
*
* \return
* None
*
 *******************************************************************************/
void Cy_HbDma_LoopbackCb(cy_stc_hbdma_channel_t *handle, cy_en_hbdma_cb_type_t type, cy_stc_hbdma_buff_status_t *pbufStat, void *userCtx);

/*****************************************************************************************
* Function Name: Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                    DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
*****************************************************************************************
* Summary:
* This function Initializes application related data structures, register callback
* creates task for device function.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer Context pointer
*
* \param pCpuDmacBase
* DMAC base address
*
* \param pCpuDw0Base
* DataWire 0 base address
*
* \param pCpuDw1Base
* DataWire 1 base address
*
* \param pHbDmaMgrCtxt
* HBDMA Manager Context
*
* \return
* None
*
 ************************************************************************************ */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base, cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

/*****************************************************************************
* Function Name: Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
*  This function will register all calback with USBD layer.
*
* \param pAppCtxt
* application layer context pointer.
*
* \return
* None
*
*******************************************************************************/
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);

/****************************************************************************
* Function Name: Cy_USB_AppSetCfgCallback(void *pUserCtxt,
                                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                        cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when set configuration is received
*
* \param pUserCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer.
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppSetCfgCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusResetCallback(void *pAppCtxt,cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when bus detects RESET
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when RESET is completed
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when speed is identified or
* speed change is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppSetupCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when SETUP packet is received
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
*
* Callback function will be invoked by USBD when Suspend signal/message is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                         cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* Callback function will be invoked by USBD when Resume signal/message is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppResumeCallback (void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function Name: Cy_USB_AppSetIntfCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
* Summary:
* Callback function will be invoked by USBD when SET Interface is called
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);


/******************************************************************************
* Function name: Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                         cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when L1 Sleep message comes.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                            cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when L1 Resume message comes.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when ZLP message comes
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when SLP message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when SET Feature message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when Clear Feature message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/******************************************************************************
* Function name: CyApp_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                              cy_en_usb_speed_t usbSpeed)
*******************************************************************************
*
* This function register USB descriptor with USBD layer
*
* \param pAppCtxt
* application layer context pointer.
*
* \param usbSpeed
* USBD speed
*
* \return
* None
*
******************************************************************************/
void CyApp_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_usb_speed_t usbSpeed);

/******************************************************************************
* Function name: Cy_USB_AppQueueRead(void *pAppCtxt, uint8_t endpNumber,
                                    uint8_t *pBuffer, uint16_t dataSize)
*******************************************************************************
*
* Function to queue read operation on an OUT endpoint.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param endpNumber
* USB endpoint number
*
* \param pBuffer
* data buffer pointer
*
* \param dataSize
* data size
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, uint8_t *pBuffer, uint16_t dataSize);

/******************************************************************************
* Function name: Cy_USB_AppReadShortPacket(void *pAppCtxt, uint8_t endpNumber,
                                    uint16_t pktSize)
*******************************************************************************
*
* Function to modify an ongoing DMA read operation to take care of a short packet.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param endpNumber
* USB endpoint number
*
* \param pktSize
* data packet size
*
*
* \return
* Total size of data in the DMA buffer including data which was already read by the channel
*
*******************************************************************************/
uint16_t Cy_USB_AppReadShortPacket(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, uint16_t pktSize);



/******************************************************************************
* Function name: Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                                       cy_israddress userIsr)
*******************************************************************************
*
* Function to register an ISR for the DMA channel associated with an endpoint
*
* \param endpNumber
* USB endpoint number
*
* \param endpDirection
* Endpoint direction
*
* \param userIsr
*  ISR function pointer. Can be NULL if interrupt is to be disabled.
*
* \return
* None
*
******************************************************************************/
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr);

/*****************************************************************************
 * Function Name: Cy_UVC_DataWire_ISR(void)
 *****************************************************************************
 *
 * Handler for DataWire interrupts associated with DMA transfers into
 * USB-HS Video Streaming endpoint.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_UVC_DataWire_ISR(void);

/******************************************************************************
 * Function Name: Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt)
 ******************************************************************************
 * Summary
 *  PSVP specific USB connect function.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt);

/******************************************************************************
 * Function Name: Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt)
 ******************************************************************************
 * Summary
 *  PSVP specific USB disconnect function.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt);

/******************************************************************************
 * Function Name: Cy_LVDS_LVCMOS_Init(void)
 ******************************************************************************
 * Summary
 *  Initialize the LVDS interface. Currently, only the SIP #0 is being initialized
 *  and configured to allow transfers into the HBW SRAM through DMA.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_LVCMOS_Init(void);

/******************************************************************************
 * Function Name: Cy_Update_LvdsLinkClock
 ******************************************************************************
 * Summary
 *  This function updates the clock which the LVCMOS link layer is using while
 *  operating in link loopback mode. If the active USB connection is USB-HS,
 *  the clock needs to be slowed down so that there is no overflow happening
 *  on the interface.
 *
 * Parameters:
 *  isHs: Whether active USB connection is USB-HS.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_Update_LvdsLinkClock(bool isHs);

/******************************************************************************
* Function Name: Cy_USB_AppConfigureEndp(cy_stc_usb_app_ctxt_t *pUsbdCtxt,
                                         uint8_t *pEndpDscr)
*******************************************************************************
* Summary:
*  Configure all endpoints used by application (except EP0)
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pEndpDscr
* Endpoint descriptor pointer
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr);

/******************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParamsSs
 ******************************************************************************
 * Summary:
 *  Perform high bandwidth DMA initialization associated with a USB endpoint.
 *
 * Parameters:
 *  \param pUsbApp
 *  Pointer to application context structure.
 *
 *  \param pEndpDscr
 * Endpoint descriptor
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp, uint8_t *pEndpDscr);

/******************************************************************************
 * Function Name: Cy_PDM_AppMxPdmInit(void)
 ******************************************************************************
 * Summary
 *  Initialize the PDM module as required for the USB audio class interface.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_AppMxPdmInit(void);

/******************************************************************************
 * Function Name: Cy_PDM_AppMxPdmDeinit(void)
 ******************************************************************************
 * Summary
 *  Disable the PDM receive functionality when the audio stream is stopped.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_AppMxPdmDeinit(void);

/******************************************************************************
 * Function Name: Cy_UAC_AppInitDmaIntr
 ******************************************************************************
 * Summary:
 *  Enable or disable the interrupt handlers for the DataWire channels reading
 *  data out from the PDM receive FIFOs.
 *
 * Parameters:
 *  \param enable
 * Whether the interrupts are to be enabled or disabled.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppInitDmaIntr(bool enable);

/******************************************************************************
 * Function Name: Cy_PDM_InEpDma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Interrupt service routine for completion of data transfer on USB 2.x
 *  IN endpoint used for audio streaming.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_InEpDma_ISR(void);


/******************************************************************************
 * Function Name: Cy_UAC_AppInit
 ******************************************************************************
 * Summary:
 *  Perform all UAC specific application initialization.
 *
 * Parameters:
 *  \param pAppCtxt
 * Handle to the application context data structure.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt);
/******************************************************************************
 * Function Name: Cy_PDM_AppQueueRead
 ******************************************************************************
 * Summary:
 * Initiate DataWire transfer to read data from PDM receive FIFO(s) to RAM
 * buffers allocated as part of DMA channels.
 *
 * Parameters:
 *  \param pAppCtxt
 * Pointer to application context structure.
 *
 *  \param dataLength
 * Length of data (in bytes) to read from each RX FIFO.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_AppQueueRead(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t dataLength);
/******************************************************************************
 * Function Name: Cy_UAC_AppSetIntfHandler
 ******************************************************************************
 * Summary:
 *  Set Interface request handler for the UAC audio streaming interface.
 *
 * Parameters:
 *  \param pAppCtxt
 * Pointer to the application context structure.
 *
 *  \param altSetting
 * Selected alternate setting for the audio streaming interface.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppSetIntfHandler(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t altSetting);

/******************************************************************************
* Function Name: Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                            cy_stc_usbss_cal_ctxt_t *pSSCal)
*******************************************************************************
* Summary:
*  Function to print out the USB event log buffer content.
*
* Parameters:
*  \param pAppCtxt
*  Pointer to application context data structure.
*  \param pSSCal
*  Pointer to SSCAL context data structure.
*
* Return:
*  void
******************************************************************************/
void Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal);

/******************************************************************************
* Function name: Cy_CheckStatus(const char *function, uint32_t line,
*                               uint8_t condition, uint32_t value,
*                               uint8_t isBlocking)
*******************************************************************************
*
* Description: Function that handles prints error log
*
* \param function
* Pointer to function
*
* \param line
* Line number where error is seen
*
* \param condition
*  condition of failure
*
* \param value
*  error code
*
* \param isBlocking
*  blocking function
*
* \return
* None
*
******************************************************************************/
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

/******************************************************************************
* Function name: Cy_USB_FailHandler(void)
*******************************************************************************
*
* Description: Error Handler
*
*
* \return
* None
*
******************************************************************************/
void Cy_FailHandler(void);

/******************************************************************************
* Function name: Cy_CheckStatusHandleFailure(const char *function, uint32_t line,
*                               uint8_t condition, uint32_t value,
*                               uint8_t isBlocking, void (*failureHandler)(void))
*******************************************************************************
*
* Description: Function that handles prints error log
*
* \param function
* Pointer to function
*
* \param line
* LineNumber where error is seen
*
* \param condition
* Line number where error is seen
*
* \param value
*  error code
*
* \param isBlocking
*  blocking function
*
* \param failureHandler
*  failure handler function
*
* \return
* None
*
******************************************************************************/
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)());

/******************************************************************************
* Function Name: Cy_FPGAPhyLnkTraining(void)
*******************************************************************************
*
*  I2C wRites to FPGA to set up Phy & Link trianing patterns
*
* Parameters:
*
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_FPGAPhyLnkTraining (void);

/******************************************************************************
* Function Name: cy_usb_get_lvds_phy_gear_ratio(void)
*******************************************************************************
*
* Get the LVDS PHY Gearing ration
*
* Parameters:
*
*
* Return:
*  Gearing ratio used to configure the LVDS IP
******************************************************************************/
uint8_t cy_usb_get_lvds_phy_gear_ratio (void);

/*****************************************************************************
 * Function Name: Cy_App_LvdsRetraining
 *****************************************************************************
 *
 * Perform LVDS link retraining by toggling LINK_READY to signal the FPGA
 * and restarting PHY training, GPIF state machine, and data streaming.
 *
 * \param pAppCtxt
 * Pointer to the application context structure.
 *
 * \return
 * CY_LVDS_SUCCESS or error status from the LVDS PDL functions.
 *
 ****************************************************************************/
void Cy_App_LvdsRetraining (cy_stc_usb_app_ctxt_t *pAppCtxt);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */

/* End of File */
