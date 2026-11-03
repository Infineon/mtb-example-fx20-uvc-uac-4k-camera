/******************************************************************************
* \file cy_usb_app.c
* \version 1.0
*
* Implements the USB data handling part of the USB Video Class application.
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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usbss_cal_drv.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_lvds.h"
#include "cy_imagesensor.h"
#include "cy_usb_i2c.h"
#include "cy_usb_videocontrol.h"
#include "cy_usb_qspi.h"
#include "cy_usb_descriptors.h"

/* Buffer counter for different resolution */
uint32_t cy_uvc_commitLength = 0;
uint16_t cy_uvc_buffer_counter = 1;

/* Default Frame Index is  4K  as 1st index */
uint16_t cy_uvc_currentFrameIndex = CY_USB_UVC_SS_4K_FRAME_INDEX;
uint16_t cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_3840_2160;
uint16_t cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_3840_2160;

#define FPS_DEFAULT          (60)
#define DMA_BUFFER_SIZE      (CY_USB_UVC_STREAM_BUF_SIZE)

#if UVC_HEADER_BY_FX20
#define FPGA_DMA_BUFFER_SIZE (DMA_BUFFER_SIZE - CY_USB_UVC_MAX_HEADER)
#elif UVC_HEADER_BY_FPGA
#define FPGA_DMA_BUFFER_SIZE (DMA_BUFFER_SIZE)
#endif /* UVC_HEADER_BY_FX20 */

/* DMA buffer size in HS when FW build with Auto DMA */
#define FPGA_DMA_BUFFER_SIZE_HS (CY_USB_UVC_STREAM_BUF_SIZE_HS - CY_USB_UVC_MAX_HEADER)

#if ((AUDIO_IF_EN) && (!LVCMOS_EN) && (FPGA_ENABLE))
static uint32_t glAudioDataBuf[PDM_APP_BUFFER_CNT][UAC_XFER_BUF_SIZE / 4];
#endif /* ((AUDIO_IF_EN) && (!LVCMOS_EN) && (FPGA_ENABLE)) */

uint8_t glIsSensorConfigured;
bool glIsFPGARegConfigured = false;
extern bool glIsFPGAConfigured;
extern bool glIsLVDSPhyTrainingDone;
extern bool glIsLVDSLink0TrainingDone;
extern bool glIsLVDSLink1TrainingDone;
extern uint8_t glPhyLinkTrainControl;
extern cy_stc_hbdma_buf_mgr_t HBW_BufMgr;
extern cy_stc_usb_app_ctxt_t appCtxt;
extern cy_stc_usb_cal_ctxt_t hsCalCtxt;
extern cy_stc_debug_config_t dbgCfg;
extern void Cy_UVC_PrintLVDSConfigInfo(void);

/* UVC Probe Control Settings for a USB 3.0 connection. */
/* 3840*2160 @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_4K[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_4K_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_4K_FRAME_INDEX,       /* Use 1st Video frame index */
    0x0B, 0x8B, 0x02, 0x00,             /* Desired frame interval in the unit of 100ns: 60 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units */
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate*/
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x20, 0xFD, 0x00,             /* Max video frame size in bytes: 3840*2160*2 = 0x00FD2000*/
#if AUTO_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(FRAME_SIZE_BYTE_4K_WITH_HEADER), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(FRAME_SIZE_BYTE_4K_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE2(FRAME_SIZE_BYTE_4K_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE3(FRAME_SIZE_BYTE_4K_WITH_HEADER),
#elif MANUAL_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(DMA_BUFFER_SIZE), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE2(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE3(DMA_BUFFER_SIZE),
#endif
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* 1920*1080 @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_1080p[CY_USB_UVC_MAX_PROBE_SETTING] __attribute__((aligned(32)));
uint8_t cy_uvc_probectrl_1080p_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_1080P_FRAME_INDEX,    /* Use 2nd Video frame index */
    0x0B, 0x8B, 0x02, 0x00,             /* Desired frame interval in the unit of 100ns: 60 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units */
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate*/
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x48, 0x3F, 0x00,             /* Max video frame size in bytes: 1920*1080*2 = 0x003F 4800 */
#if AUTO_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(FRAME_SIZE_BYTE_1080P_WITH_HEADER), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(FRAME_SIZE_BYTE_1080P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE2(FRAME_SIZE_BYTE_1080P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE3(FRAME_SIZE_BYTE_1080P_WITH_HEADER),
#elif MANUAL_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(DMA_BUFFER_SIZE), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE2(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE3(DMA_BUFFER_SIZE),
#endif
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* 1280*720 @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_720p[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_720p_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_720P_FRAME_INDEX,     /* Use 2nd Video frame index */
    0x0B, 0x8B, 0x02, 0x00,             /* Desired frame interval in the unit of 100ns: 60 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units */
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate*/
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x20, 0x1C, 0x00,             /* Max video frame size in bytes: 1280*720*2 = 0x001C2000 */
#if AUTO_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(FRAME_SIZE_BYTE_720P_WITH_HEADER), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(FRAME_SIZE_BYTE_720P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE2(FRAME_SIZE_BYTE_720P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE3(FRAME_SIZE_BYTE_720P_WITH_HEADER),
#elif MANUAL_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(DMA_BUFFER_SIZE), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE2(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE3(DMA_BUFFER_SIZE),
#endif
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* 640*480  @15 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_VGA[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_VGA_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_VGA_FRAME_INDEX,      /* Use 3rd Video frame index */
    0x2A, 0x2C, 0x0A, 0x00,             /* Desired frame interval in the unit of 100ns: 15 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units*/
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate */
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x60, 0x09, 0x00,             /* Max video frame size in bytes: 640*480*2 = 0x00096000 */
#if AUTO_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(FRAME_SIZE_BYTE_480P_WITH_HEADER), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(FRAME_SIZE_BYTE_480P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE2(FRAME_SIZE_BYTE_480P_WITH_HEADER),
    CY_USB_DWORD_GET_BYTE3(FRAME_SIZE_BYTE_480P_WITH_HEADER),
#elif MANUAL_DMA_CHANNEL
    CY_USB_DWORD_GET_BYTE0(DMA_BUFFER_SIZE), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE2(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE3(DMA_BUFFER_SIZE),
#endif
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* USBHS: 640*480  @15 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_HS_VGA[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_HS_VGA_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_HS_VGA_FRAME_INDEX,      /* Use 1st Video frame index */
    0x2A, 0x2C, 0x0A, 0x00,             /* Desired frame interval in the unit of 100ns: 15 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units*/
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate */
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x60, 0x09, 0x00,             /* Max video frame size in bytes: 640*480*2 = 0x00096000 */
    CY_USB_DWORD_GET_BYTE0(DMA_BUFFER_SIZE), /* No. of bytes device can rx in single payload = DMA_BUFFER_SIZE bytes */
    CY_USB_DWORD_GET_BYTE1(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE2(DMA_BUFFER_SIZE),
    CY_USB_DWORD_GET_BYTE3(DMA_BUFFER_SIZE),
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* Video Probe Commit Control */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_commitctrl[CY_USB_UVC_MAX_PROBE_SETTING_ALIGNED];

/* Whether SET_CONFIG is complete or not. */
static volatile bool cy_uvc_devconfigured = false;
static volatile bool cy_uvc_IsApplnActive = false;

/* UVC Header */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_header[CY_USB_UVC_MAX_HEADER];
uint8_t cy_uvc_header_[] = {
    32,                                 /* Header Length */
    0x8C,                               /* Bit field header field */
    0x00, 0x00, 0x00, 0x00,             /* Presentation time stamp field */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Source clock reference field */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00        /* Zero padding for 32 byte align */
};

HBDMA_BUF_ATTRIBUTES uint32_t Ep0XferBuffer[32U];
HBDMA_BUF_ATTRIBUTES uint32_t SetSelDataBuffer[8];

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
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition,
                            uint32_t value, uint8_t isBlocking)
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
} /* End of Cy_CheckStatus() */

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
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line,
                                    uint8_t condition, uint32_t value,
                                    uint8_t isBlocking,
                                    void (*failureHandler)(void))
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);

        if(failureHandler != NULL)
        {
            (*failureHandler)();
        }
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
} /* End of Cy_CheckStatusAndHandleFailure() */

/******************************************************************************
* Function Name: Cy_AppPrintStartupLogs(void)
*******************************************************************************
* Summary:
* Prints Initial configuration information
*
* Parameters:
* void
*
* Return:
* void
******************************************************************************/
void Cy_AppPrintStartupLogs(void)
{
    vTaskDelay(1);
    LOG_COLOR("\r\n\r\n\r\n****************************************************\r\n");

    DBG_APP_INFO("DEMO_FX20 with Ti180 FPGA\r\n");
    DBG_APP_INFO("SPEED %d\r\n", appCtxt.devSpeed);

#if (LVCMOS_EN)
#if (LVCMOS_DDR_EN)
    DBG_APP_INFO("LVCMOS DDR Mode\r\n");
#else
    DBG_APP_INFO("LVCMOS SDR Mode\r\n");
#endif
#else
    DBG_APP_INFO("LVDS Mode\r\n");
    Cy_UVC_PrintLVDSConfigInfo();
#endif

#if WL_EN
    DBG_APP_INFO("Wide_Link Mode\r\n");
#elif PORT1_EN
    DBG_APP_INFO("PORT1 Narrow_Link Mode\r\n");
#else
    DBG_APP_INFO("PORT0 Narrow_Link Mode\r\n");
#endif

#if (LVCMOS_EN && LVCMOS_DDR_EN && WL_EN)
    LOG_COLOR("PhyTraining=%s, Link0Training=%s, Link1Training=%s: FPGA%sConfigured \n\r", "NA" , \
    (glIsLVDSLink0TrainingDone?"PASS":"FAIL"),(glIsLVDSLink1TrainingDone?"PASS":"FAIL"), (glIsFPGAConfigured?"-":"-Not"));
#elif (LVCMOS_EN && (!LVCMOS_DDR_EN || PORT1_EN || !WL_EN)) /* LVCMOS SDR, and LVCMOS DDR P0,P1 NL */
    LOG_COLOR("PhyTraining=%s, Link0Training=%s, Link1Training=%s: FPGA%sConfigured \n\r", "NA", "NA","NA", (glIsFPGAConfigured?"-":"-Not"));
#elif(!LVCMOS_EN) /* LVDS */
    LOG_COLOR("PhyTraining=%s, Link0Training=%s, Link1Training=%s: FPGA%sConfigured \n\r", (glIsLVDSPhyTrainingDone?"PASS":"FAIL")\
    ,(PORT1_EN?"NA":(glIsLVDSLink0TrainingDone?"PASS":"FAIL")),((PORT1_EN|WL_EN)?(glIsLVDSLink1TrainingDone?"PASS":"FAIL"):"NA"), (glIsFPGAConfigured?"-":"-Not"));
#endif

#ifdef FIRMWARE_ID
    DBG_APP_INFO("Firmware version = %s [%s]\r\n",FIRMWARE_VERSION,FIRMWARE_ID);
#endif

    if(glIsFPGAConfigured)
    {
        Cy_I2C_FPGAGetVersion(&appCtxt);
    }
    LOG_COLOR("****************************************************\r\n\r\n");
} /* End of Cy_AppPrintStartupLogs() */



/******************************************************************************
* Function Name: Cy_UVC_AppGpifIntr(void *pApp)
*******************************************************************************
* Summary:
* Function to handle UVC based on GPIF event callback
*
* Parameters:
* \param pApp
*
*\param appCtxt
*
* Return:
* void
******************************************************************************/
void Cy_UVC_AppGpifIntr(void *pApp)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;
    DBG_APP_INFO("Cy_UVC_AppGpifIntr\n\r");
    /* Reset the DMA channel through which data is received from the LVDS side. */
    Cy_HBDma_Channel_Reset(pAppCtxt->hbBulkInChannel);

    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, true);
    }

    /* Flush and reset the endpoint and clear the STALL bit. */
    Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN);
    Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN, false);
    Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum,CY_USB_ENDP_DIR_IN, false);

    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, false);
    }

    Cy_USBD_SendAckSetupDataStatusStage(pAppCtxt->pUsbdCtxt);
} /* End of Cy_UVC_AppGpifIntr() */

/******************************************************************************
* Function Name: Cy_USB_App_KeepLinkActive(cy_stc_usb_app_ctxt_t *pAppCtxt)
*******************************************************************************
* Summary:
* Function to keep USB link active
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* Return:
* void
******************************************************************************/
static void Cy_USB_App_KeepLinkActive (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
#if USB3_LPM_ENABLE
    if (pAppCtxt->isLpmEnabled) {
        /* Disable LPM for 100 ms after any DMA transfers have been completed. */
        DBG_APP_INFO("Disabling LPM\r\n");
        Cy_USBD_LpmDisable(pAppCtxt->pUsbdCtxt);
        pAppCtxt->isLpmEnabled  = false;
    }

    pAppCtxt->lpmEnableTime = Cy_USBD_GetTimerTick() + 100;
#endif /* USB3_LPM_ENABLE */
} /* End of Cy_USB_App_KeepLinkActive() */

/*LVDSSS_LVDS_AFE0_PHY_GPIO - GPIO[0-15] control DQ[0-15]. GPIO[16-25] control CTL[0-9] pads of each port  */
void setCtlPinValue(bool port, uint8_t ctlPin, bool value)
{
    ASSERT_NON_BLOCK(ctlPin <= 9,ctlPin);
    LOG_COLOR("LVDSSS_LVDS->AFE[%d].PHY_GPIO[16 + %d] = 0x%x\r\n",port, ctlPin, (0x80000000 | (1 << 2) | value));
    LVDSSS_LVDS->AFE[port].PHY_GPIO[16 + ctlPin] = (0x80000000 | (1 << 2) | value);
}

/******************************************************************************
* Function Name: Cy_UVC_AddHeader(uint8_t *buffer_p, uint8_t frameInd)
*******************************************************************************
* Summary:
* UVC header addition function
*
* Parameters:
* \param buffer_p
* Buffer pointer
*
*\param frameInd
* Video fram ID
*
* Return:
* void
******************************************************************************/
void Cy_UVC_AddHeader(uint8_t *buffer_p, uint8_t frameInd)
{
    /* Copy header to buffer */
    memcpy(buffer_p, (uint8_t *)cy_uvc_header, CY_USB_UVC_MAX_HEADER);

    /* Check if last packet of the frame. */
    if (frameInd == CY_USB_UVC_HEADER_EOF)
    {
        /* Modify UVC header to toggle Frame ID */
        cy_uvc_header[1] ^= CY_USB_UVC_HEADER_FRAME_ID;

        /* Indicate End of Frame in the buffer */
        buffer_p[1] |= CY_USB_UVC_HEADER_EOF;
    }
} /* end of function */


/******************************************************************************
* Function Name: Cy_UVC_PrintDMAReg(uint8_t adapterNo, uint8_t socketNo)
*******************************************************************************
* Summary:
* Prints the Register content on debug terminal.
* the register information helps the user to knoow the status of training
*
* Parameters:
* \param adapterNo
* DMA adaptor number (0 or 1)
*
* \param socketNo
* Socket number
*
* Return:
*  void
******************************************************************************/
void Cy_UVC_PrintDMAReg(uint8_t adapterNo, uint8_t socketNo)
{
    DBG_APP_INFO("Start RegisterPrint ............\r\n");
    DBG_APP_INFO("LINK_TRAINING_STS:%x\r\n",LVDSSS_LVDS->LINK_TRAINING_STS[0]);
    DBG_APP_INFO("LINK_TRAINING_STS:%x\r\n",LVDSSS_LVDS->LINK_TRAINING_STS[1]);
    DBG_APP_INFO("LVDS_INTR_WD0:%x \r\n",LVDSSS_LVDS->LVDS_INTR_WD0);
    DBG_APP_INFO("LVDS_INTR_WD1:%x \r\n",LVDSSS_LVDS->LVDS_INTR_WD1);
    DBG_APP_INFO("LVDS_ERROR:%x \r\n",LVDSSS_LVDS->LVDS_ERROR);
    DBG_APP_INFO("PHY_INTR:%x \r\n",LVDSSS_LVDS->AFE[0].PHY_INTR);
    DBG_APP_INFO("PHY_GENERAL_STATUS_2:%x \r\n",LVDSSS_LVDS->AFE[0].PHY_GENERAL_STATUS_2);
    DBG_APP_INFO("GPIF_INTR:%x \r\n",LVDSSS_LVDS->GPIF[0].GPIF_INTR);
    DBG_APP_INFO("GPIF_ERROR:%x \r\n",LVDSSS_LVDS->GPIF[0].GPIF_ERROR);
    DBG_APP_INFO("WAVEFORM_CTRL_STAT:%x \r\n",LVDSSS_LVDS->GPIF[0].GPIF_WAVEFORM_CTRL_STAT);
    DBG_APP_INFO("LAMBDA_STAT0:%x \r\n",LVDSSS_LVDS->GPIF[0].GPIF_LAMBDA_STAT0);
    DBG_APP_INFO("BETA_STAT:%x \r\n",LVDSSS_LVDS->GPIF[0].GPIF_BETA_STAT);
    //DBG_APP_INFO("SCK_DSCR:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_DSCR);
    //DBG_APP_INFO("SCK_SIZE:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_SIZE);
    //DBG_APP_INFO("SCK_COUNT:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_COUNT);
    //DBG_APP_INFO("SCK_STATUS:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_STATUS);
    //DBG_APP_INFO("SCK_INTR:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_INTR);
    //DBG_APP_INFO("SCK_INTR_MASK:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].SCK_INTR_MASK);
    //DBG_APP_INFO("DSCR_BUFFER:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].DSCR_BUFFER);
    //DBG_APP_INFO("DSCR_SYNC:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].DSCR_SYNC);
    //DBG_APP_INFO("DSCR_CHAIN:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].DSCR_CHAIN);
    //DBG_APP_INFO("DSCR_SIZE:%x \r\n",LVDSSS_LVDS->ADAPTER_DMA[adapterNo].SCK[socketNo].DSCR_SIZE);
    DBG_APP_INFO("End RegisterPrint............\r\n");
} /* End of Cy_UVC_PrintDMAReg() */

/******************************************************************************
* Function Name: Cy_UVC_StreamStartStop(uint8_t IsStreamStart)
*******************************************************************************
* Summary:
* Starts/Stops FPGA data streaming
*
* Parameters:
* \param device_offset
* device number (fpga implementation specific)
*
* \param IsStreamStart
* Pass 1 to start streaming from fpga elase 0
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_UVC_StreamStartStop(uint8_t IsStreamStart)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;

    cy_uvc_IsApplnActive = IsStreamStart?true:false;
    if(IsStreamStart)
    {
        status = Cy_I2C_Write(FPGASLAVE_ADDR,(DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS),CAMERA_APP_ENABLE,
                                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    }
    else
    {

        status = Cy_I2C_Write(FPGASLAVE_ADDR,(DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS),CAMERA_APP_DISABLE,
                                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    }
    (CY_SCB_I2C_SUCCESS == status)? DBG_APP_INFO("Video App = %d\n\r",IsStreamStart):\
                                    DBG_APP_ERR(" Cy_UVC_StreamStartStop: Video App start failed \n\r");
    return status;
} /* End of Cy_UVC_StreamStartStop() */


/******************************************************************************
* Function Name: Cy_ConfigFpgaRegister()
*******************************************************************************
* Summary:
*  FPGA Register Writes.
* Writes default FPGA I2C register information including DMA buffer size,
* Number of threads and socket used, video source type etc.
*
* Parameters:
*
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
static cy_en_scb_i2c_status_t Cy_ConfigFpgaRegister (void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    /* Default resolution*/
    uint16_t width = 3840;
    uint16_t height = 2160;

    /* Disable camera before configuring FPGA register */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS,CAMERA_APP_DISABLE,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* write FPGA register to enable UVC */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_U3V_SELECTION_ADDRESS,FPGA_UVC_ENABLE,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#if UVC_HEADER_BY_FPGA
        /* Disable adding UVC header by FPGA. UVC header is added by FX10 */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_ENABLE,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#elif UVC_HEADER_BY_FX20
    DBG_APP_INFO("FX20 Adds UVC Header\r\n");
        /* Disable adding UVC header by FPGA. UVC header is added by FX10 */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_DISABLE,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#endif
#if INMD_EN
    /* Disable adding UVC header by FPGA. UVC header is added by FX10 */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_INMD_UVC_HEADER_ENABLE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#endif
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(500);

    /* Number of active device list*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_ACTIVE_DIVICE_MASK_ADDRESS,0x01,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(500);

    /* write FPGA register to Disable format conversion */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_MODE_ADDRESS,NO_CONVERSION,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select video source */
#if SONY_IMX715_EN || THINE_EN
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,MIPI_SOURCE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if SONY_IMX715_EN
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,SONY_MIPI,
                                          FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#elif THINE_EN
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,THIN_MIPI,
                                          FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif
#else
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif

#if !LVCMOS_EN
    /* update DEVICE_FLAG_INFO_ADDRESS */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_FLAG_INFO_ADDRESS,NEW_FRAME_START,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif


#if THREAD_INTERLEAVE_EN
/*
 * In Wide link mode, actual socket number each thread is connected to is
 * derived from SSAD[kkk] control byte sent by FPGA.Actual socket number =
 * (3'bkkk x 2) + tt[0] on adapter tt[1], where tt is the thread number sent in
 * STAD[tt] if FPGA sets kkk as 0 for Thread 0, actual socket number connected
 * to Thread 0 is 0 if FPGA sets kkk as 0 for Thread 1, actual socket number
 * connected to Thread 1 is 1
 */
    /* Inform active threads*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_ACTIVE_TREAD_INFO_ADDRESS,2,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    #if PORT1_EN
    /* PORT1 Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_2,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /* inform active Thread */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_3,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 1 socket information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_SOCKET_INFO_ADDRESS,00,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,00,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    #else
    /* PORT0 Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_0,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /* inform active Thread */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_1,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 1 socket information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_SOCKET_INFO_ADDRESS,00,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,00,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    #endif
#else /* Single Thread */

     /* Inform active threads*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_ACTIVE_TREAD_INFO_ADDRESS,1,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,0,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 2 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,0,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

        /* Thread 1 socket information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_SOCKET_INFO_ADDRESS,0,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    #if PORT1_EN
    /* PORT1 Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_2,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    #else
    /* PORT0 Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,CY_LVDS_GPIF_THREAD_0,
                                        FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    #endif
#endif

    /* Update DMA buffer size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_MSB_ADDRESS,CY_USB_GET_MSB(FPGA_DMA_BUFFER_SIZE),
                                      FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_LSB_ADDRESS,CY_USB_GET_LSB(FPGA_DMA_BUFFER_SIZE),
                                      FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

   /* Update default resolution width size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_MSB_ADDRESS,CY_USB_GET_MSB(width),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_LSB_ADDRESS,CY_USB_GET_LSB(width),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Update default resolution hight size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_MSB_ADDRESS,CY_USB_GET_MSB(height),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_LSB_ADDRESS,CY_USB_GET_LSB(height),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    /*currently FPGA uses 64 bit bus width to generate the colorbar data or read from the image sensor (SONY, Thine)*/
    /* ___________          _____             ______        _________         ______
    *|           |        |     |           |      |      |         |       |      |
    *| FV LV Gen |--64b-->| FiFo|--W(AXI)-->| DDR  |--R-->|SlaveFiFo|--64-->| FX20 |
    *|___________|        |_____|           |______|      |________ |       |______|
    *
    * Using 64 bit generator in FPGA can able to achieve max 4K frame (YUY2) @ 64-65fps
    * No meaning to stream more than 65fps in narrow link mode
    */
    if ((FPS_DEFAULT >= 66) && (LVCMOS_EN || (WL_EN == 0)))
    {
        LOG_ERROR("Check the FPGA and FX3G2 Narrow Link max capability");
    }
    /*User should modify or use max throughput FPGA config, to work with 20Gbps (128/256 bit generator) */
    if (FPS_DEFAULT >= 75)
    {
        LOG_COLOR("Make sure you are using max throughput FPGA config file, otherwise no stream");
    }
    /* default fps*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_FPS_ADDRESS,FPS_DEFAULT,
                                      FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    return status;
} /* End of Cy_ConfigFpgaRegister() */

/******************************************************************************
* Function Name: Cy_UVC_SendVideoResolutionToFPGA(uint16_t width, uint16_t height)
*******************************************************************************
* Summary:
*
*
* Parameters:
* \param width
* Video Resolution width
*
* \param Height
* Video Resolution Height
*
* Return:
*  0 for success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_UVC_SendVideoResolutionToFPGA (uint16_t width, uint16_t height)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_MSB_ADDRESS,CY_USB_GET_MSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_LSB_ADDRESS,CY_USB_GET_LSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    DBG_APP_INFO("i2c status = 0x%x IMAGE_WIDTH :%d \n\r",status, width);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_MSB_ADDRESS,CY_USB_GET_MSB(height),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);


    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_LSB_ADDRESS,CY_USB_GET_LSB(height),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    DBG_APP_INFO("i2c status = 0x%x IMAGE_HIGHT :%d \n\r",status, height);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_MODE_ADDRESS,NO_CONVERSION,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_FPS_ADDRESS,FPS_DEFAULT,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x FPS :%d \n\r",status, FPS_DEFAULT);
    return status;
} /* End of Cy_UVC_SendVideoResolutionToFPGA() */


/******************************************************************************
* Function Name: Cy_FPGAPhyLnkTraining()
*******************************************************************************
* Summary:
*  I2C wRites to FPGA to set up Phy & Link trianing patterns
*
* Parameters:
*
*
* Return:
*  0 for success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_FPGAPhyLnkTraining (void)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;

    /* write FPGA register to Disable format conversion */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS,CAMERA_APP_DISABLE,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs(1000);
#if LINK_TRAINING_EN
    uint8_t lvds_phy_gear_ratio = cy_usb_get_lvds_phy_gear_ratio();
    /* Inform Gearing ratio to the FPGA */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_GEAR_RATIO_ADDRESS,
                            (uint8_t)pow(2.0, (double) lvds_phy_gear_ratio),
                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if LVCMOS_DDR_EN
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,FPGA_LINK_CONTROL,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs (1000);

#else
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_PHY_LINK_CONTROL_ADDRESS, FPGA_LINK_CONTROL | FPGA_PHY_CONTROL,
                           FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);

#endif
    Cy_SysLib_DelayUs(1000);

    /* Send PHY Training patter byte to the FPGA */
    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_PHY_TRAINING_ADDRESS, PHY_TRAINING_PATTERN_BYTE,
                           FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(1000);

    /* Send LINK Training patter byte to the FPGA */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS,CY_USB_DWORD_GET_BYTE0(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs (1000);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS,CY_USB_DWORD_GET_BYTE1(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs (1000);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS,CY_USB_DWORD_GET_BYTE2(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs (1000);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS,CY_USB_DWORD_GET_BYTE3(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    DBG_APP_INFO("i2c status = 0x%x\n\r",status);
    Cy_SysLib_DelayUs (1000);
#endif

    return status;
} /* End of Cy_FPGAPhyLnkTraining() */


/******************************************************************************
* Function Name: Cy_UVC_SetVideoResolution(uint8_t format_index,
*                                   uint8_t resolution_index,
                                    uint8_t device_offset, uint8_t devSpeed)
*******************************************************************************
* Summary:
*
*
* Parameters:
* \param format_index
* Video format Index
*
* \param resolution_index
* Video resolution Index
*
* \param device_offset
* device number (fpga implementation specific)
*
* \param devSpeed
* device USB speed
*
* Return:
*  0 for read success, error code for unsuccess.
******************************************************************************/
cy_en_scb_i2c_status_t Cy_UVC_SetVideoResolution (uint32_t format_index, uint32_t resolution_index, cy_en_usb_speed_t devSpeed)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    uint8_t regValue = 0;

    DBG_APP_INFO("format_index %d resolution_index %d devSpeed = %d\r\n",  format_index,resolution_index, devSpeed);
    if(devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
        switch(resolution_index)
        {
        case CY_USB_UVC_SS_4K_FRAME_INDEX:
            status = Cy_I2C_Read(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,&regValue, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            if(format_index == YUY2_FORMAT_INDEX)
            {
                CLR_BIT(regValue, HDMI_COLOR_FORMAT_YUV444);
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,regValue, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_PIXEL_WIDTH_ADDRESS,_16BIT_PIXEL, FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            }
            if (glIsSensorConfigured)
            {
                /*Stream video from MIPI Source*/
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS, MIPI_SOURCE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                #if SONY_IMX715_EN
                status = Cy_ImageSensorSetResolution(3840,  2160);
                if(status)
                    DBG_APP_ERR("Error I2C write with Sensor\r\n");
                else
                    DBG_APP_INFO("Success I2C write with Sensor\r\n");
                #elif THINE_EN
                cy_ImageSensorInitTHine(SENSOR_SLAVE_ADDR);
                #endif
            }
            else
            {
                /*Stream colorbar in case image sensor is not configured */
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

            }
            status = Cy_UVC_SendVideoResolutionToFPGA (3840,  2160);
            break;

        case CY_USB_UVC_SS_1080P_FRAME_INDEX:
            if (glIsSensorConfigured)
            {
                /*Stream video from MIPI Source*/
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,MIPI_SOURCE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                #if SONY_IMX715_EN
                status = Cy_ImageSensorSetResolution(1920,  1080); /* Sensor configuration defined in cy_imagesensir.c */
                ASSERT_NON_BLOCK(0 == status,status);
                #endif
            }
            else
            {
                /*Stream colorbar in case image sensor is not configured */
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            }
            status = Cy_UVC_SendVideoResolutionToFPGA (1920,  1080);
            break;

        case CY_USB_UVC_SS_720P_FRAME_INDEX:
            /* status = Cy_ImageSensorSetResolution(1280,  720); */
            /*if(status)
                DBG_APP_ERR("Error I2C write with Sensor\r\n");*/
                /*Stream colorbar in case image sensor is not configured */
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                status = Cy_UVC_SendVideoResolutionToFPGA (1280,  720);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            break;

        case CY_USB_UVC_SS_VGA_FRAME_INDEX:
            /* CyImageSensorSetResolution(640,  480); */
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                status = Cy_UVC_SendVideoResolutionToFPGA (640,  480);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            break;

        default:
            break;
        }
    }
    else /* HS or FS */
    {
        switch(resolution_index)
        {
            case CY_USB_UVC_HS_VGA_FRAME_INDEX:
                /*Stream colorbar in case image sensor is not configured */
                status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
                status = Cy_UVC_SendVideoResolutionToFPGA (640,  480);
                ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            break;
        }
    }
    return status;
} /* End of Cy_UVC_SetVideoResolution() */

/******************************************************************************
* Function Name: Cy_UVC_FpgaStatusCheck(cy_stc_usb_app_ctxt_t *pAppCtxt)
*******************************************************************************
* Summary: Reads FPGA Common and Device status. This function called every
* One second once and status will be printed on debug port if change in status
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* Return:
* void
******************************************************************************/
void Cy_UVC_FpgaStatusCheck(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    static uint8_t fpgaDeviceRegStatusPrev = 0;
    static uint8_t fpgaCommonRegStatusPrev = 0;
    uint8_t fpgaStatusCurrent;
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;
    BaseType_t pdStatus;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    if(pAppCtxt->glFrameCount)
    {
        pAppCtxt->glfps = (pAppCtxt->glFrameCount * 1000) / (APP_ONE_SEC_INTERVAL_MS);
        pAppCtxt->glFrameCount = 0;
        DBG_APP_INFO("FPS = %d\r\n",pAppCtxt->glfps);
        pAppCtxt->glfps = 0;
    }
#if LINK_TRAINING_EN
    if(!glIsLVDSPhyTrainingDone)
    {
        LOG_COLOR("LVDS Phy Training Failed..Reset the board and try again\r\n");
    }
    #if (!PORT1_EN && !LVCMOS_EN)
    if(!glIsLVDSLink0TrainingDone)
    {
        LOG_ERROR("LVDS Link 0 Training Failed..Reset the board and try again\r\n");
        Cy_UVC_PrintDMAReg(0,0);
    }
    #endif
    #if (WL_EN) || (PORT1_EN && !LVCMOS_EN)
    if(!glIsLVDSLink1TrainingDone)
    {
        LOG_ERROR("LVDS Link 1 Training Failed..Reset the board and try again\r\n");
        Cy_UVC_PrintDMAReg(0,0);
    }
    #endif
#endif
    if (pAppCtxt->printStartupMessage)
    {
        xMsg.type = CY_USB_STARTUP_PRINT_EVENT;
        pdStatus = xQueueSendFromISR(pAppCtxt->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
        ASSERT_NON_BLOCK(pdTRUE == pdStatus, status);
    }


    if(glIsFPGAConfigured)
    {
        /*Read Device 0 FPGA status*/
        status = Cy_I2C_Read (FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_FLAG_STATUS_ADDRESS,
                               &fpgaStatusCurrent,FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
        ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        if((CY_SCB_I2C_SUCCESS == status)  &&  (fpgaStatusCurrent != fpgaDeviceRegStatusPrev))
        {
            LOG_COLOR("FPGA Device Status = 0x%x\r\n",fpgaStatusCurrent);
            fpgaDeviceRegStatusPrev = fpgaStatusCurrent;
            if(fpgaStatusCurrent & SLAVE_FIFO_ALMOST_EMPTY)
            DBG_APP_INFO("FPGA Device_Status: Slave FIFO Empty\r\n");

            if(fpgaStatusCurrent & INTER_MED_FIFO_EMPTY)
            DBG_APP_INFO("FPGA Device_Status: Intermediate FIFO Empty\r\n");

            if(fpgaStatusCurrent & INTER_MED_FIFO_FULL)
            DBG_APP_INFO("FPGA Device_Status: Intermediate FIFO Full\r\n");

            if(fpgaStatusCurrent & DDR_FULL_FRAME_WRITE_COMPLETE)
            DBG_APP_INFO("FPGA Device_Status: DDR Full frame write complete\r\n");

            if(fpgaStatusCurrent & DDR_FULL_FRAME_READ_COMPLETE)
            DBG_APP_INFO("FPGA Device_Status: DDR Full frame read complete\r\n");
        }
            /*Read common status FPGA register*/
            status = Cy_I2C_Read (FPGASLAVE_ADDR,FPGA_EXT_CONTROLLER_STATUS_ADDRESS,
                                    &fpgaStatusCurrent,FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
        if((CY_SCB_I2C_SUCCESS == status) &&  (fpgaStatusCurrent != fpgaCommonRegStatusPrev))
        {
            fpgaCommonRegStatusPrev = fpgaStatusCurrent;
            if(fpgaStatusCurrent & DMA_READY_STATUS)
            DBG_APP_INFO("FPGA Common_Status: DMA Ready\r\n");

            if(fpgaStatusCurrent & INTER_MED_FIFO_EMPTY)
            DBG_APP_INFO("FPGA Common_Status: DDR configuration\r\n");

            if(fpgaStatusCurrent & INTER_MED_FIFO_FULL)
            DBG_APP_INFO("FPGA Common_Status: DDR Controller busy\r\n");

            if(fpgaStatusCurrent & DDR_FULL_FRAME_WRITE_COMPLETE)
            DBG_APP_INFO("FPGA Common_Status: DDR Command queue full\r\n");

            if(fpgaStatusCurrent & DDR_FULL_FRAME_READ_COMPLETE)
            DBG_APP_INFO("FPGA Common_Status: Datapath is idle\r\n");
        }
    }
    else
    {
        LOG_ERROR("FPGA Configuration failed..Check troubleshooting section of user guide\r\n")
    }
} /* End of Cy_UVC_FpgaStatusCheck() */

/******************************************************************************
* Function Name: Cy_UVC_AppStop(cy_stc_usb_app_ctxt_t *pAppCtxt,
                cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint16_t wIndex)
*******************************************************************************
* Summary:
* Stop the data stream channels
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param wIndex
* USB streaming endpoint numbder
*
* Return:
* void
******************************************************************************/
static void Cy_UVC_AppStop(cy_stc_usb_app_ctxt_t *pAppCtxt,
                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint16_t wIndex)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;
    cy_stc_hbdma_sock_t sckStat;
    uint8_t index = 0;
    uint32_t pollCnt = 0;
    uint32_t epNumber = ((uint32_t)wIndex & 0x7FUL);
    DBG_APP_INFO("App Stop:windex=0x%x\r\n",wIndex);
    if((cy_uvc_IsApplnActive == true) && (pAppCtxt->hbBulkInChannel != NULL))
    {
        /* Wait for DMA sockets to stall */
        for(index = 0; index < pAppCtxt->hbBulkInChannel->prodSckCount; index++)
        {
            do {
                Cy_SysLib_DelayUs(10);
                Cy_HBDma_GetSocketStatus(pAppCtxt->pHbDmaMgrCtxt->pDrvContext, pAppCtxt->hbBulkInChannel->prodSckId[index], &sckStat);
            } while(
                    ((_FLD2VAL(LVDSSS_LVDS_ADAPTER_DMA_SCK_STATUS_STATE,sckStat.status)) != 0x1) &&
                    (pollCnt++ < 500));

            DBG_APP_INFO("DMA Socket %x is stalled  status = 0x%x\r\n", pAppCtxt->hbBulkInChannel->prodSckId[index], sckStat.status);
        }
    }
    Cy_UVC_StreamStartStop(STOP);
    /* Reset the DMA channel through which data is received from the LVDS side. */
    status = Cy_HBDma_Channel_Reset(pAppCtxt->hbBulkInChannel);
    ASSERT_NON_BLOCK(CY_HBDMA_MGR_SUCCESS == status,status);

    cy_uvc_IsApplnActive = false;
    DBG_APP_INFO("UVC DMA Reset and Variable Cleared\r\n");

    /* On USB 2.0 connection, reset the DataWire channel used to send data to the EPM. */
    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
    {
        Cy_USBHS_App_ResetEpDma(&(pAppCtxt->endpInDma[epNumber]));
    }
    else if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, true);
    }

    Cy_USBD_FlushEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN);
    Cy_USBD_ResetEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, false);
    }
} /* End of Cy_UVC_AppStop() */

/******************************************************************************
* Function Name: Cy_UVC_AppStart(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint32_t format_index,
                                            uint32_t frame_index)
*******************************************************************************
* Summary:
* Start the data stream channel/s
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param format_index
* Video format index
*
* \param frame_index
* Video resolution index
*
* Return:
* 0 for read success, error code for unsuccess
******************************************************************************/
static void Cy_UVC_AppStart(cy_stc_usb_app_ctxt_t *pAppCtxt,
                            uint32_t format_index, uint32_t frame_index)
{
    cy_en_hbdma_mgr_status_t mgrStatus;
    DBG_APP_TRACE("AppStart cy_uvc_IsApplnActive %d\r\n",cy_uvc_IsApplnActive);

    if (!cy_uvc_IsApplnActive)
    {
        mgrStatus = Cy_HBDma_Channel_Enable(pAppCtxt->hbBulkInChannel, 0);
        ASSERT_NON_BLOCK(mgrStatus == CY_HBDMA_MGR_SUCCESS, mgrStatus);
        if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS && AUTO_DMA_CHANNEL)
            DBG_APP_ERR("Switch to SuperSpeed USB Port and Cable, or use manual DMA configuration in HS \r\n");
    }

    pAppCtxt->glfps                  = 0;
    pAppCtxt->glDmaBufCnt            = 0;
    pAppCtxt->glDmaBufCnt_prv        = 0;
    pAppCtxt->glProd                 = 0;
    pAppCtxt->glCons                 = 0;
    pAppCtxt->glProdCount            = 0;
    pAppCtxt->glConsCount            = 0;
    pAppCtxt->glFrameSizeTransferred = 0;
    pAppCtxt->glFrameSize            = 0;
    pAppCtxt->glPrintFlag            = 0;
    pAppCtxt->glFrameCount           = 0;
    pAppCtxt->glPartialBufSize       = 0;

    Cy_UVC_StreamStartStop(STOP);
    if (Cy_UVC_SetVideoResolution(format_index, frame_index, pAppCtxt->devSpeed) != CY_SCB_I2C_SUCCESS)
    {
        DBG_APP_ERR("Error Sending Resolution\r\n");
    }
    if (!cy_uvc_IsApplnActive)
    {
        #if LVCMOS_EN
        Cy_LVDS_GpifSMSwitch(LVDSSS_LVDS, 0, 255, 0, 0, 12, 2 );
        #else
        Cy_LVDS_GpifSMSwitch(LVDSSS_LVDS, 0, 255, 0, 0, 0, 2 );
        #endif

        DBG_APP_INFO("Cy_LVDS_GpifSMSwitch line %d\r\n", __LINE__);
    }
    Cy_UVC_StreamStartStop(START);
} /* End of Cy_UVC_AppStart */

/*
 * Function: Cy_USB_AppDisableEndpDma()
 * Description: This function de-inits all active USB DMA channels as part of USB disconnect process.
 * Parameter: cy_stc_usb_app_ctxt_t *
 * return: void
 */
void Cy_USB_AppDisableEndpDma (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    /* Disable and destroy the High BandWidth DMA channels. */
    if (pAppCtxt->hbBulkInChannel != NULL) {
        Cy_HBDma_Channel_Disable(pAppCtxt->hbBulkInChannel);
        Cy_HBDma_Channel_Destroy(pAppCtxt->hbBulkInChannel);
        pAppCtxt->hbBulkInChannel = NULL;

        /* Flush the EPM in case of USB 3.x connection. */
        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN);
        }
    }

#if AUDIO_IF_EN
    if (pAppCtxt->pPDMToUsbChn != NULL) {
        Cy_HBDma_Channel_Disable(pAppCtxt->pPDMToUsbChn);
        Cy_HBDma_Channel_Destroy(pAppCtxt->pPDMToUsbChn);
        pAppCtxt->pPDMToUsbChn = NULL;

        /* Flush the EPM in case of USB 3.x connection. */
        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, UAC_IN_ENDPOINT, CY_USB_ENDP_DIR_IN);
        }
    }
#endif /* AUDIO_IF_EN */

} /* End of Cy_USB_AppDisableEndpDma() */

/******************************************************************************
* Function Name: Cy_UVC_AppSetCurRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                            uint16_t wLength, uint16_t wValue)
*******************************************************************************
* Summary:
* This function handles the SET_CUR request addresses to the UVC video streaming interface.
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param wLength
* wLength field of control request
*
* \param wValue
* wValue field of control request
*
* Return:
* void
******************************************************************************/
void Cy_UVC_AppSetCurRqtHandler (cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength, uint16_t wValue)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    uint32_t status;
    uint16_t loopCnt = 250u;

    DBG_APP_TRACE("UVC_SET_CUR_REQ wValue = 0x%x wIndex = 0x%x\r\n",wValue, pAppCtxt->wIndex);

    /* Read the data out commitctrl buffer */
    status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, cy_uvc_commitctrl, wLength);
    if (status == CY_USBD_STATUS_SUCCESS)
    {
        /* Wait until receive DMA transfer has been completed. */
        while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
            Cy_SysLib_DelayUs(10);
        }
        if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
            DBG_APP_ERR("SET_CUR data timed out\r\n");
            Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            return;
        }

        cy_uvc_currentFrameIndex = cy_uvc_commitctrl[3];
        /* Reset the Counter */
        cy_uvc_buffer_counter = 1;
        if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS)
        {
            /* Copy the relevant settings from the host provided data into the
               active data structure. */
            if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_4K_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_4K[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_3840_2160;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_3840_2160;
            }
            else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_1080P_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_1080p[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_1920_1080;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_1920_1080;
            }
            else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_720P_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_720p[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_1280_720;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_1280_720;
            }
            else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_VGA_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_VGA[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_640_480;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_640_480;
            }
            else
            {
                DBG_APP_ERR("Error FrameIndex\r\n");
                cy_uvc_currentFrameIndex = CY_USB_UVC_SS_4K_FRAME_INDEX;
            }
            DBG_APP_TRACE("cy_usb_full_buffer_no%d\r\n",cy_usb_full_buffer_no);
            DBG_APP_TRACE("cy_usb_no_bytes_last_buffer%d \r\n",cy_usb_no_bytes_last_buffer);
        }
        else /* HS speed */
        {
            if (cy_uvc_currentFrameIndex == CY_USB_UVC_HS_VGA_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_HS_VGA[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_640_480;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_640_480;

                #if (AUTO_DMA_CHANNEL)
                cy_uvc_probectrl_HS_VGA[22] = CY_USB_DWORD_GET_BYTE0(CY_USB_UVC_STREAM_BUF_SIZE_HS);
                cy_uvc_probectrl_HS_VGA[23] = CY_USB_DWORD_GET_BYTE1(CY_USB_UVC_STREAM_BUF_SIZE_HS);
                cy_uvc_probectrl_HS_VGA[24] = CY_USB_DWORD_GET_BYTE2(CY_USB_UVC_STREAM_BUF_SIZE_HS);
                cy_uvc_probectrl_HS_VGA[25] = CY_USB_DWORD_GET_BYTE3(CY_USB_UVC_STREAM_BUF_SIZE_HS);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_640_480_HS;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_640_480_HS;
                #endif
            }
            else
            {
                DBG_APP_ERR("Error FrameIndex\r\n");
                cy_uvc_currentFrameIndex = CY_USB_UVC_HS_VGA_FRAME_INDEX;
            }
        }

        if (wValue == CY_USB_UVC_VS_COMMIT_CONTROL)
        {
            DBG_APP_INFO("SET_CUR Commit cy_uvc_currentFrameIndex:%x\r\n", cy_uvc_currentFrameIndex);
            /* Reset the Counter */
            cy_uvc_buffer_counter = 1;
            Cy_UVC_AppStart(pAppCtxt, cy_uvc_commitctrl[2],cy_uvc_commitctrl[3]);
        }
    }
    else
    {
        DBG_APP_ERR("Error RecvEndp0Data\r\n");
    }
} /* End of Cy_UVC_AppSetCurRqtHandler() */

/*****************************************************************************
* Function Name: Cy_UVC_AppGetCurRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
* This function handles the GET_CUR request addresses to the UVC video streaming interface.
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* Return:
* void
*****************************************************************************/
void Cy_UVC_AppGetCurRqtHandler (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;

    if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS)
    {
        switch (cy_uvc_currentFrameIndex)
        {
            case CY_USB_UVC_SS_4K_FRAME_INDEX:
                retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                        (uint8_t *)cy_uvc_probectrl_4K, CY_USB_UVC_MAX_PROBE_SETTING);
                break;

            case CY_USB_UVC_SS_720P_FRAME_INDEX:
                retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                        (uint8_t *)cy_uvc_probectrl_720p, CY_USB_UVC_MAX_PROBE_SETTING);
                break;

            case CY_USB_UVC_SS_1080P_FRAME_INDEX:
                retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                        (uint8_t *)cy_uvc_probectrl_1080p, CY_USB_UVC_MAX_PROBE_SETTING);
                break;

            case CY_USB_UVC_SS_VGA_FRAME_INDEX:
                retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                        (uint8_t *)cy_uvc_probectrl_VGA, CY_USB_UVC_MAX_PROBE_SETTING);
                break;

            default:
                DBG_APP_ERR("currentFrameIndex\r\n");
                Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
                break;
        }
    }
    else
    {
        if (cy_uvc_currentFrameIndex == CY_USB_UVC_HS_VGA_FRAME_INDEX)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                    (uint8_t *)cy_uvc_probectrl_HS_VGA, CY_USB_UVC_MAX_PROBE_SETTING);
        }
        else
        {
            DBG_APP_ERR("currentFrameIndex\r\n");
            Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
        }
    }

    if (retStatus != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_ERR("Error SendEndp0Data\r\n");
    }
}

/******************************************************************************
* Function Name: Cy_UVC_AppDeviceTaskHandler(void *pTaskParam)
*******************************************************************************
* Summary:
* Application task handler
*
* Parameters:
* \param pTaskParam
* task param
*
* Return:
* void
******************************************************************************/
void Cy_UVC_AppDeviceTaskHandler(void *pTaskParam)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    cy_stc_usbd_app_msg_t queueMsg;
    cy_en_scb_i2c_status_t i2cStatus;
    BaseType_t xStatus;
    uint32_t *pUsbEvtLogBuf;
    uint16_t wIndex;
    uint32_t dpslpcnt;
    bool IsDeepSlpExit;

    (void)IsDeepSlpExit;

    /* Reset Frame Id in UVC Header */
    cy_uvc_header_[1] = CY_USB_UVC_HEADER_DEFAULT_BFH;
    DBG_APP_INFO("UvcDeviceThreadCreated\r\n");

#if FPGA_ENABLE
    Cy_FPGAConfigPins(pAppCtxt,FPGA_CONFIG_MODE);
    Cy_QSPI_Start(pAppCtxt,&HBW_BufMgr);
    Cy_SPI_FlashInit(SPI_FLASH_0, true, false);;

    Cy_FPGAConfigure(pAppCtxt,FPGA_CONFIG_MODE);
    Cy_FPGAPhyLnkTraining();
    Cy_I2C_FPGAGetVersion(pAppCtxt);

    if(!glIsFPGARegConfigured)
    {
        if(0 == Cy_ConfigFpgaRegister())
        {
            glIsFPGARegConfigured = true;
            DBG_APP_INFO("Successfuly configured Ti180 FPGA via I2C \n\r");
        }
        else
        {
            LOG_ERROR("Failed to configure FPGA via I2C \r\n");
        }
    }
#endif /* FPGA_ENABLE */
    vTaskDelay(10);
    /* Initialize the LVDS interface. */
    Cy_LVDS_LVCMOS_Init();
    /* Register for USB events to be stored in buffer so that all LTSSM state change interrupts are enabled. */
    pUsbEvtLogBuf = (uint32_t *)Cy_HBDma_BufMgr_Alloc(pAppCtxt->pHbDmaMgrCtxt->pBufMgr, 512u * sizeof(uint32_t));
    Cy_USBSS_Cal_InitEventLog(pAppCtxt->pUsbdCtxt->pSsCalCtxt, pUsbEvtLogBuf, 512u);

    /* TBD: */
    memcpy (cy_uvc_probectrl_4K, cy_uvc_probectrl_4K_, sizeof(cy_uvc_probectrl_4K_));
    memcpy (cy_uvc_probectrl_720p, cy_uvc_probectrl_720p_, sizeof(cy_uvc_probectrl_720p_));
    memcpy (cy_uvc_probectrl_1080p, cy_uvc_probectrl_1080p_, sizeof(cy_uvc_probectrl_1080p_));
    memcpy (cy_uvc_probectrl_VGA, cy_uvc_probectrl_VGA_, sizeof(cy_uvc_probectrl_VGA_));
    memcpy (cy_uvc_probectrl_HS_VGA, cy_uvc_probectrl_HS_VGA_, sizeof(cy_uvc_probectrl_HS_VGA_));
    memcpy (cy_uvc_header, cy_uvc_header_, sizeof(cy_uvc_header_));

    /* If VBus is present, enable the USB connection. */
    pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);
    if (pAppCtxt->vbusPresent) {
        Cy_USB_SSConnectionEnable(pAppCtxt);
    }


#if SONY_IMX715_EN
    if(Cy_ImageSensorSetResolution(3840, 2160) != 0) /* configure sensor with 4K as default resolution */
        DBG_APP_INFO("I2C fails with Sony Sensor1 0x1A \n\r");

    if(glIsSensorConfigured)
    {
        DBG_APP_INFO("Successfuly configured Sony Image senor via I2C \n\r");
        Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,SONY_MIPI,
                                          FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    }
    else
    {
        DBG_APP_INFO("Failed to configured Sony Image senor via I2C \n\r");
        DBG_APP_INFO("Streaming Colorbar \n\r");
        Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    }

    for(uint32_t count = 0; count < 200; count++)
        Cy_SysLib_DelayUs(1000);/* Delay 200ms needed for Sony IMX715 */
#elif THINE_EN
    cy_ImageSensorInitTHine(SENSOR_SLAVE_ADDR);
    if(glIsSensorConfigured)
        DBG_APP_INFO("Successfuly configured Thine1 Image senor via I2C \n\r");
    else
        DBG_APP_INFO("Failed to configured Thine Image senor via I2C \n\r");
#endif
    xTimerStart( pAppCtxt->FpgaStatusTimer, 0 );
    appCtxt.printStartupMessage = true;
    for (;;)
    {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 50 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->uvcMsgQueue, &queueMsg, 50);
        if (xStatus != pdPASS) {
            continue;
        }

        switch (queueMsg.type)
        {
            case CY_USB_UVC_VBUS_CHANGE_INTR:
                /* Start the debounce timer. */
                xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
                break;

            case CY_USB_UVC_VBUS_CHANGE_DEBOUNCED:
                /* Check whether VBus state has changed. */
                pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);

                if (pAppCtxt->vbusPresent)
                {
                    if (!pAppCtxt->usbConnected)
                    {
                        DBG_APP_INFO("Enabling USB connection due to VBus detect\r\n");
                        Cy_USB_SSConnectionEnable(pAppCtxt);
                    }
                }
                else
                {
                    if (pAppCtxt->usbConnected)
                    {
                        Cy_USB_AppDisableEndpDma(pAppCtxt);
                        DBG_APP_INFO("Disabling USB connection due to VBus removal\r\n");
                        Cy_USB_SSConnectionDisable(pAppCtxt);
                    }
                }
                break;

            case FX_PRINT_FRAME_INFO_EVENT:
                pAppCtxt->glPrintFlag = 0;
                DBG_APP_INFO("Prod=%d Cons=%d Prtl_Sz=%d Frm_Sz=%dB\n\r",
                pAppCtxt->glProdCount, pAppCtxt->glConsCount, pAppCtxt->glPartialBufSize, pAppCtxt->glFrameSize);
                break;

            case FX_PRINT_FPS_EVENT :
                if(pAppCtxt->glfps)
                {
                    Cy_Debug_AddToLog(2, "UVC fps = %d\n\r",pAppCtxt->glfps);
                    pAppCtxt->glfps = 0;
                }
                break;

            case CY_USB_UVC_VIDEO_STREAM_START_EVENT:
                break;

            case CY_USB_UVC_DEVICE_VIDEO_CNTRL_RQT_EVENT:
                DBG_APP_INFO("queueMsg.data[0] = %d\r\n",queueMsg.data[0]);
                xStatus  = Cy_VideoControlHandleVCInterfaceRequest(pAppCtxt, queueMsg.data[0]);
                ASSERT_NON_BLOCK(xStatus == CY_USBD_STATUS_SUCCESS, xStatus);
                break;

            case CY_USB_UVC_DEVICE_SET_CUR_RQT:
                DBG_APP_TRACE("CY_USB_UVC_DEVICE_SET_CUR_RQT\r\n");
                DBG_APP_TRACE("queueMsg.data[0] = %d\r\n",queueMsg.data[0]);
                Cy_UVC_AppSetCurRqtHandler(pAppCtxt, queueMsg.data[0], queueMsg.data[1]);
                break;

            case CY_USB_UVC_DEVICE_GET_CUR_RQT:
                Cy_UVC_AppGetCurRqtHandler(pAppCtxt);
                break;

            case CY_USB_UVC_VIDEO_STREAM_STOP_EVENT:
                DBG_APP_INFO("CY_USB_UVC_VIDEO_STREAM_STOP_EVENT\r\n");
                wIndex = (uint16_t)queueMsg.data[0];
                Cy_UVC_AppStop(pAppCtxt,pAppCtxt->pUsbdCtxt, wIndex);
                break;

            case CY_USB_STARTUP_PRINT_EVENT:
                    appCtxt.printStartupMessage = false;
                    Cy_AppPrintStartupLogs();
                break;

            case CY_USB_UVC_FPGA_STATUS_POLL_EVENT:
                xTimerReset(pAppCtxt->FpgaStatusTimer,0);
                Cy_UVC_FpgaStatusCheck(pAppCtxt);
                break;

            case FX_DMA_RESET_EVENT:
                /* No action taken */
                break;

#if DEEPSLEEP_EN
            case CY_USB_ENTER_DEEPSLEEP:
                if (
                        (pAppCtxt->devState == CY_USB_DEVICE_STATE_SUSPEND) &&
                        (pAppCtxt->prevDevState==CY_USB_DEVICE_STATE_CONFIGURED))
                {
                    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
                    {
                        dpslpcnt = 50000;
                        do {
                            Cy_SysLib_DelayUs(10);
                            dpslpcnt--;
                        } while ((pAppCtxt->devState == CY_USB_DEVICE_STATE_SUSPEND) && (dpslpcnt > 0));
                    }
                    else
                    {
                        if (Cy_USBD_GetUSBPowerState(pAppCtxt->pUsbdCtxt) == CY_USBD_USBSS_LINK_U3)
                        {
                            dpslpcnt = 50000;
                            do {
                                Cy_SysLib_DelayUs(10);
                                dpslpcnt--;
                            } while ((Cy_USBD_GetUSBPowerState(pAppCtxt->pUsbdCtxt) == CY_USBD_USBSS_LINK_U3) && (dpslpcnt > 0));
                        }

                        if (Cy_USBD_GetUSBPowerState(pAppCtxt->pUsbdCtxt) != CY_USBD_USBSS_LINK_U3)
                        {
                            pAppCtxt->devState = pAppCtxt->prevDevState;
                        }
                    }

                    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_SUSPEND)
                    {
                        DBG_APP_INFO("Entering DeepDleep\r\n");
                        Cy_UVC_StreamStartStop(STOP);
                        Cy_SysLib_DelayUs(1500);

#if LVDS_LOW_POWER_EN
                        /* Signal the FPGA to initiate L3 Entry */
                        i2cStatus = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LOW_PWR_MODE_ADDRESS, LVDS_L3_MODE_ENTER,
                                FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
                        ASSERT_NON_BLOCK(i2cStatus == CY_SCB_I2C_SUCCESS, i2cStatus);
#endif /* LVDS_LOW_POWER_EN */

                        IsDeepSlpExit = false;
                        if (Cy_USBD_EnterDeepsleep(pAppCtxt->pUsbdCtxt, pAppCtxt->pCpuDw1Base) == CY_USBD_STATUS_SUCCESS)
                        {
#if USBFS_LOGS_ENABLE
                            Cy_Debug_LogDeInit();
                            Cy_Debug_LogInit(&dbgCfg);
#endif /* USBFS_LOGS_ENABLE */

#if LVCMOS_EN
                            DBG_APP_INFO("Woke up from deep sleep\r\n");
#else
                            DBG_APP_INFO("Woke up from deep sleep, initiating phy/link re-training\r\n");
#endif /* LVCMOS_EN */

                            pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
                            pAppCtxt->devState = CY_USB_DEVICE_STATE_CONFIGURED;
                            IsDeepSlpExit = true;
                        }
                        else
                        {
                            LOG_ERROR("Failed to enter deep sleep mode\r\n");
                        }

#if LVDS_LOW_POWER_EN
                        /* Clear training status before initiating low power exit to make the FPGA ready for retraining
                         * Request for phy and link training
                         */
                        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
                        i2cStatus = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_PHY_LINK_CONTROL_ADDRESS, FPGA_PHY_CONTROL|FPGA_LINK_CONTROL,
                                FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
                        ASSERT_NON_BLOCK(i2cStatus == CY_SCB_I2C_SUCCESS, i2cStatus);

                        i2cStatus = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LOW_PWR_MODE_ADDRESS, LVDS_EXIT_LOWPOWER_MODE,
                                FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
                        ASSERT_NON_BLOCK(i2cStatus == CY_SCB_I2C_SUCCESS, i2cStatus);
#endif /* LVDS_LOW_POWER_EN */

#if LVCMOS_EN
                        if (IsDeepSlpExit)
                        {
                            Cy_LVDS_LVCMOS_Init();
                            Cy_SysLib_Delay(1);
                        }
#endif /* LVCMOS_EN */
                    }
                }
#endif /* DEEPSLEEP_EN */
                break;

#if LVDS_LOW_POWER_EN
            case CY_LVDS_RETRAIN:
                /* Device has exited a LVDS Low power mode, rerun LVDS training.
                 * At this stage, device applies the stored settings and reruns the training */
                Cy_App_LvdsRetraining(pAppCtxt);
                break;
#endif /* LVDS_LOW_POWER_EN */

            default:
                break;
        } /* switch (queueMsg.type) */
    } /* for(;;) */
} /* End of Cy_UVC_AppDeviceTaskHandler() */

/******************************************************************************
* Function Name: Cy_USB_VbusDebounceTimerCb(TimerHandle_t xTimer)
*******************************************************************************
* Summary:
* Timer used to do debounce on VBus changed interrupt notification.
*
* \param xTimer
* Timer Handle
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_VbusDebounceTimerCb (TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    DBG_APP_INFO("VbusDebounce_CB\r\n");
    if (pAppCtxt->vbusChangeIntr) {
        /* Notify the VCOM task that VBus debounce is complete. */
        xMsg.type = CY_USB_UVC_VBUS_CHANGE_DEBOUNCED;
        xQueueSendFromISR(pAppCtxt->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);
    }
} /* End of function Cy_USB_VbusDebounceTimerCb() */

/***********************************************************************************************
* Function Name: Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
**************************************************************************************************
* Summary:
* This Function will be called when timer expires.This function print event log.
*
* Parameters:
* \param xTimer
* timer handle
*
* Return:
* void
************************************************************************************************/
void
Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    /* retrieve pAppCtxt */
    pAppCtxt = ( cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED) {
        /*
         * Print out the contents of the USB event log buffer which has
         * logged event related to enumeration and line training.
         */
        Cy_USB_AppPrintUsbEventLog(pAppCtxt, pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    }
}   /* End of Cy_USB_PrintEvtLogTimerCb() */

/******************************************************************************
* Function Name: Cy_USB_FpgaStatusReadTimerCb(TimerHandle_t xTimer)
*******************************************************************************
* Summary:
* Timer used to read FPGA status at every second
*
* \param xTimer
* Timer Handle
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_FpgaStatusReadTimerCb (TimerHandle_t xTimer)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t status;

    xMsg.type = CY_USB_UVC_FPGA_STATUS_POLL_EVENT;
    status = xQueueSendFromISR(appCtxt.uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
    ASSERT_NON_BLOCK(pdTRUE == status,status);
}   /* End of Cy_USB_FpgaStatusReadTimerCb() */

/******************************************************************************
* Function Name: Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                               cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base,
                                DW_Type *pCpuDw1Base,
                                cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
*******************************************************************************
* Summary:
* This function Initializes application related data structures, register
* callback, creates software timer, queue and task for device function.
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
 *****************************************************************************/
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
                    DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    uint32_t index;
    BaseType_t status = pdFALSE;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;

    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->enumMethod = CY_USB_ENUM_METHOD_FAST;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++)
    {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;
    pAppCtxt->glDmaBufCnt_prv = 0;

    /*
     * Callbacks registered with USBD layer. These callbacks will be called
     * based on appropriate event.
     */
    Cy_USB_AppRegisterCallback(pAppCtxt);

    if (!(pAppCtxt->firstInitDone))
    {
        pAppCtxt->vbusChangeIntr = false;
        pAppCtxt->vbusPresent    = false;
        pAppCtxt->usbConnected   = false;

        /* Create the message queue and register it with the kernel. */
        pAppCtxt->uvcMsgQueue = xQueueCreate(CY_USB_UVC_DEVICE_MSG_QUEUE_SIZE,
                CY_USB_UVC_DEVICE_MSG_SIZE);
        if (pAppCtxt->uvcMsgQueue == NULL) {
            DBG_APP_ERR("QueuecreateFail\r\n");
            return;
        }

        vQueueAddToRegistry(pAppCtxt->uvcMsgQueue, "UVCDeviceMsgQueue");

        /* Create task and check status to confirm task created properly. */
        status = xTaskCreate(Cy_UVC_AppDeviceTaskHandler, "UvcDeviceTask", 2048,
                             (void *)pAppCtxt, 5, &(pAppCtxt->uvcDevicetaskHandle));
        if (status != pdPASS)
        {
            DBG_APP_ERR("TaskcreateFail\r\n");
            return;
        }

#if AUDIO_IF_EN
        Cy_UAC_AppInit(pAppCtxt);
#endif /* AUDIO_IF_EN */

        pAppCtxt->vbusDebounceTimer = xTimerCreate("VbusDebounceTimer", 200, pdFALSE,
                (void *)pAppCtxt, Cy_USB_VbusDebounceTimerCb);
        if (pAppCtxt->vbusDebounceTimer == NULL) {
            DBG_APP_ERR("TimerCreateFail\r\n");
            return;
        }

        pAppCtxt->evtLogTimer = xTimerCreate("EventLogTimer", APP_ONE_SEC_INTERVAL_MS, pdTRUE,
                                             (void *)pAppCtxt,
                                             Cy_USB_PrintEvtLogTimerCb);
        if (pAppCtxt->evtLogTimer == NULL) {
            DBG_APP_ERR("evtLogTimer Create Fail\r\n");
            return;
        } else {
            /* Start the USB Event logger timer. */
            DBG_APP_TRACE("evtLogTimer Start\r\n");
            xTimerStart(pAppCtxt->evtLogTimer, 0);
        }

        pAppCtxt->FpgaStatusTimer = xTimerCreate("FpgaStatusTimer", APP_ONE_SEC_INTERVAL_MS, pdFALSE,
        (void *)pAppCtxt, Cy_USB_FpgaStatusReadTimerCb);
        if (pAppCtxt->FpgaStatusTimer == NULL) {
            DBG_APP_ERR("FPGA StatusTimerCreateFail\r\n");
            return;
        }
        DBG_APP_INFO("xTimer Created\r\n");
        pAppCtxt->firstInitDone = 0x01;
    }

    /* Zero out the EP0 transfer buffer. */
    memset((uint8_t *)Ep0XferBuffer, 0, sizeof(Ep0XferBuffer));
} /* End of Cy_USB_AppInit() */


/******************************************************************************
* Function Name: Cy_USB_AppSetAddressCallback(void *pUsbApp,
*                                           cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
*                                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
* Summary:
* This Function will be called by USBD layer when  a USB address has been
* assigned to the device.
*
* \param pAppCtxt
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
 *****************************************************************************/
void Cy_USB_AppSetAddressCallback (void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Update the state variables. */
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr      = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed     = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Check the type of USB connection and register appropriate descriptors. */
    CyApp_RegisterUsbDescriptors(pAppCtxt, pAppCtxt->devSpeed);
} /* End of Cy_USB_AppSetAddressCallback() */


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
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET,
                             Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE,
                             Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED,
                             Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP,
                             Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND,
                             Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME,
                             Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG,
                             Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF,
                             Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP,
                             Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME,
                             Cy_USB_AppL1ResumeCallback);
    // Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP,
    //                         Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP,
                             Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR,
                             Cy_USB_AppSetAddressCallback);
} /* End of Cy_USB_AppRegisterCallback() */

/******************************************************************************
* Function Name: Cy_UVC_AppHandleProduceEvent(cy_stc_usb_app_ctxt_t  *pUsbApp,
                                        cy_stc_hbdma_channel_t *pChHandle)
*******************************************************************************
* Summary:
*  Function that handles a produce event indicating receipt of data through
*  the LVDS ingress socket.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pChHandle
* Pointer to DMA channel structure.
*
* \return
* None
*
 *****************************************************************************/
static void Cy_UVC_AppHandleProduceEvent (
        cy_stc_usb_app_ctxt_t  *pUsbApp,
        cy_stc_hbdma_channel_t *pChHandle)
{
    cy_en_hbdma_mgr_status_t   status;
    cy_stc_hbdma_buff_status_t buffStat;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t pdStatus, xHigherPriorityTaskWoken = pdFALSE;
    /* Wait for a free buffer. */
    status = Cy_HBDma_Channel_GetBuffer(pChHandle, &buffStat);
    if (status != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("HB-DMA GetBuffer Error\r\n");
        return;
    }

    pUsbApp->glDmaBufCnt = buffStat.count;
    pUsbApp->glFrameSizeTransferred += pUsbApp->glDmaBufCnt;
    cy_uvc_commitLength = buffStat.count + CY_USB_UVC_MAX_HEADER;


#if PARTIAL_FRAME_EN
    if (((buffStat.count == pUsbApp->glDmaProdBufSize) && (pUsbApp->devSpeed > CY_USBD_USB_DEV_HS)) || \
        (cy_uvc_buffer_counter <= cy_usb_full_buffer_no)) /* Not End of frame, add UVC header and send it UIB */
#else
    if(cy_uvc_buffer_counter <= cy_usb_full_buffer_no) /* Not End of frame, add UVC header and send it UIB */
#endif
    {
        /* Not the end of frame. */
        if (UVC_HEADER_BY_FX20 || (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS))
            Cy_UVC_AddHeader(buffStat.pBuffer - CY_USB_UVC_MAX_HEADER, CY_USB_UVC_HEADER_FRAME);

        cy_uvc_buffer_counter++;
    }
    else /* End of Frame detected ? */
    {
        /* Short packet: End of frame. */
        if (UVC_HEADER_BY_FX20 || pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
            Cy_UVC_AddHeader(buffStat.pBuffer - CY_USB_UVC_MAX_HEADER, CY_USB_UVC_HEADER_EOF);

        cy_uvc_buffer_counter = 1;
        pUsbApp->glFrameSize = pUsbApp->glFrameSizeTransferred;
        pUsbApp->glFrameSizeTransferred = 0;
        pUsbApp->glFrameCount++;
        pUsbApp->glPartialBufSize = buffStat.count;
        pUsbApp->glConsCount = pUsbApp->glCons;
        pUsbApp->glProdCount = pUsbApp->glProd;
        pUsbApp->glPrintFlag = 1;
        pUsbApp->glCons = 0;
        pUsbApp->glProd = 0;
        xMsg.type = FX_PRINT_FRAME_INFO_EVENT;
        pdStatus = xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
        ASSERT_NON_BLOCK(pdTRUE == pdStatus, pdStatus);
    }

    if(!buffStat.count)
    {
        status = Cy_HBDma_Channel_DiscardBuffer(pChHandle, &buffStat);
        return;
    }
    if (UVC_HEADER_BY_FX20 || (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS))
    {
        /* Commit the buffer for transfer */
        buffStat.count = cy_uvc_commitLength;
        buffStat.size = (cy_uvc_commitLength + 15)&0xFFF0;
        buffStat.pBuffer = buffStat.pBuffer - CY_USB_UVC_MAX_HEADER;
    }

    /* Commit the buffer for transfer */
    status = Cy_HBDma_Channel_CommitBuffer(pChHandle, &buffStat);
    if (status != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("HB-DMA CommitBuffer Error\r\n");
        return;
    }
    Cy_USB_App_KeepLinkActive(pUsbApp);
} /* End of CyUvcAppHandleProduceEvent() */

/******************************************************************************
* Function Name: Cy_UVC_HbDmaCb(cy_stc_hbdma_channel_t *handle,
*                                   cy_en_hbdma_cb_type_t type,
*                                   cy_stc_hbdma_buff_status_t *pbufStat,
*                                   void *userCtx)
*******************************************************************************
* Summary:
* HBDMA callback function to add the UVC header to the frame/buffer coming
* from LVDS and commits to USB.
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
 *****************************************************************************/
void Cy_UVC_HbDmaCb(
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        pAppCtxt->glProd++;
        /* Video streamer application enable */
        if ((cy_uvc_IsApplnActive == true) && (cy_uvc_devconfigured == true))
        {
            Cy_UVC_AppHandleProduceEvent(pAppCtxt, handle);
        }
    }
    if (type == CY_HBDMA_CB_CONS_EVENT)
    {
        pAppCtxt->glCons++;
    }
} /* End of Cy_UVC_HbDmaCb() */

#if AUDIO_IF_EN
/******************************************************************************
 * Function Name: Cy_UAC_HbDmaCb
 ******************************************************************************
 * Summary:
 *  Callback function to manage data transfers on the USB Audio Class endpoint
 *  in USB 3.x connections.
 *
 * Parameters:
 *  \param handle
 *  Pointer to the DMA channel associated with the endpoint.
 *
 *  \param type
 *  Type of callback event
 *
 *  \param pbufStat
 *  Event specific data
 *
 *  \param userCtx
 * User context data to be de-referenced to obtain the application context.
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_UAC_HbDmaCb (cy_stc_hbdma_channel_t *handle,
                          cy_en_hbdma_cb_type_t type,
                          cy_stc_hbdma_buff_status_t *pbufStat,
                          void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    /* As this is an IN endpoint, only consume events are expected. */
    if (type == CY_HBDMA_CB_CONS_EVENT)
    {
        /* Data has been consumed on USB side and buffer is free now. If all buffers
         * were full previously, we can queue a read operation using the newly freed
         * buffer.
         */
        if (pAppCtxt->pdmRxFreeBufCount == 0)
        {
            /* Queue read from the PDM RX FIFO into the next DMA buffer. */
            Cy_PDM_AppQueueRead(pAppCtxt, UAC_EP_MAX_PKT_SIZE);
        }

        pAppCtxt->pdmRxFreeBufCount++;
    }
} /* End of Cy_UAC_HbDmaCb() */
#endif /* AUDIO_IF_EN */

/*****************************************************************************
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
static void Cy_USB_AppSetupEndpDmaParamsSs (cy_stc_usb_app_ctxt_t *pUsbApp,
                                uint8_t *pEndpDscr)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    cy_en_hbdma_mgr_status_t mgrStat;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);
    DBG_APP_INFO("AppSetupEndpDmaParamsSs: endpNum:0x%x maxPktSize:0x%x dir:0x%x\r\n",
                 endpNumber, maxPktSize, dir);

    /* Handling UVC streaming endpoint. */
    if ((endpNumber == UVC_STREAM_ENDPOINT) && (dir != 0)) {
        pUsbApp->uvcInEpNum = (uint8_t)endpNumber;

        if (pUsbApp->hbBulkInChannel != NULL)
        {
            /* If channel was created earlier, then destroy the channel and create a fresh. */
            Cy_HBDma_Channel_Disable(pUsbApp->hbBulkInChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbBulkInChannel);
            pUsbApp->hbBulkInChannel = NULL;
        }

        dmaConfig.chType        = CY_HBDMA_TYPE_IP_TO_IP;    /* DMA Channel type: from LVDS to USB EG IP. */
        dmaConfig.usbMaxPktSize = maxPktSize;
        dmaConfig.consSckCount  = 1;                         /* Only one consumer socket per channel. */
        dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1]    = (cy_hbdma_socket_id_t)0;
        dmaConfig.count         = CY_USB_UVC_STREAM_BUF_COUNT; /* DMA Buffer Count */
        dmaConfig.prodBufSize   = FPGA_DMA_BUFFER_SIZE;        /* Receive depends on uvc header adds by FX10 or FPGA*/
        dmaConfig.size          = DMA_BUFFER_SIZE;             /* DMA Buffer size in bytes */
        dmaConfig.bufferMode    = true;                        /* DMA buffer mode enabled */
        dmaConfig.userCtx       = (void *)(pUsbApp);           /* Pass the application context as user context. */

#if MANUAL_DMA_CHANNEL
        #if UVC_HEADER_BY_FX20
        dmaConfig.prodHdrSize  = CY_USB_UVC_MAX_HEADER;
        #elif UVC_HEADER_BY_FPGA
        dmaConfig.prodHdrSize  = 0;
        #endif
        dmaConfig.eventEnable  = 0;          /* Manual channel: Disable event signalling between sockets. */
        dmaConfig.intrEnable   = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                 LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.cb           = Cy_UVC_HbDmaCb;   /* HB-DMA callback */
#elif AUTO_DMA_CHANNEL
        dmaConfig.prodHdrSize  = 0;
        dmaConfig.eventEnable  = 1;          /* Auto channel: Enable event signalling between sockets. */
        dmaConfig.intrEnable   = 0;          /* No interrupts required in this case. */
        dmaConfig.cb           = NULL;       /* Call back function is not required for AUTO DMA */

#endif

#if PORT1_EN /* PORT1 */
    #if THREAD_INTERLEAVE_EN /* PORT1 Dual Thread */
        dmaConfig.prodSckCount = 2; /* No. of producer sockets */
        dmaConfig.prodSck[0]   = CY_HBDMA_LVDS_SOCKET_16;
        dmaConfig.prodSck[1]   = CY_HBDMA_LVDS_SOCKET_17;
    #else /* PORT1 Single Thread */
        dmaConfig.prodSckCount = 1; /* No. of producer sockets */
        dmaConfig.prodSck[0]   = CY_HBDMA_LVDS_SOCKET_16;
        dmaConfig.prodSck[1]   = (cy_hbdma_socket_id_t)0; /* Producer Socket ID: None */
    #endif
#else /* PORT0 */
    #if THREAD_INTERLEAVE_EN /* PORT0 Dual Thread */
        dmaConfig.prodSckCount = 2; /* No. of producer sockets */
        dmaConfig.prodSck[0]   = CY_HBDMA_LVDS_SOCKET_00;
        dmaConfig.prodSck[1]   = CY_HBDMA_LVDS_SOCKET_01;
    #else  /* PORT0 Single Thread */
        dmaConfig.prodSckCount = 1; /* No. of producer sockets */
        dmaConfig.prodSck[0]   = CY_HBDMA_LVDS_SOCKET_00;
        dmaConfig.prodSck[1]   = (cy_hbdma_socket_id_t)0; /* Producer Socket ID: None */
    #endif /* THREAD_INTERLEAVE_EN */
#endif /* PORT1_EN */

        if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
        {
            #if (!LVCMOS_EN && WL_EN) /* DMA count=1 for LVDS WL mode only*/
            {
                dmaConfig.count      = 1; /* DMA Buffer Count */
            }
            #endif
            dmaConfig.consSck[0]  = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
            dmaConfig.eventEnable = 0;  /* Manual channel: Disable event signalling between sockets. */
            dmaConfig.intrEnable  = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                    LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
            #if AUTO_DMA_CHANNEL
            DBG_APP_INFO("Use manual DMA configuration for USB 2.0 due to architectural limitation \r\n");
            cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;
            /* In case of HS is detected , Manual DMA channel should use */
            dmaConfig.size        = CY_USB_UVC_STREAM_BUF_SIZE_HS;             /* DMA Buffer size in bytes */
            dmaConfig.prodBufSize = FPGA_DMA_BUFFER_SIZE_HS;
            dmaConfig.prodHdrSize = CY_USB_UVC_MAX_HEADER;
            dmaConfig.cb          = Cy_UVC_HbDmaCb;   /* HB-DMA callback */

            /* Update DMA buffer size used by Firmware */
            status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_MSB_ADDRESS,CY_USB_GET_MSB(FPGA_DMA_BUFFER_SIZE_HS),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

            status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_LSB_ADDRESS,CY_USB_GET_LSB(FPGA_DMA_BUFFER_SIZE_HS),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

            status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_DISABLE,
                                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            #endif
        }
        /* Copy DMA producer Buffer size into global variable */
        pUsbApp->glDmaProdBufSize = dmaConfig.prodBufSize;
        Cy_USBD_SetEpBurstMode(pUsbApp->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_IN, true);

        LOG_COLOR("DMA Channel Config - Manual Channel enable: %d UVC Header Addition by FPGA/INMD: %d \n\r", MANUAL_DMA_CHANNEL,UVC_HEADER_BY_FPGA);
        LOG_COLOR("DMA Channel Config - DMA buffer Size: %d, Producer buffer size: %d \n\r",dmaConfig.size,dmaConfig.prodBufSize);

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                &dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("BulkIn channel create failed 0x%x\r\n", mgrStat);
            if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS && AUTO_DMA_CHANNEL)
                DBG_APP_ERR("Switch to SuperSpeed USB Port and Cable, or Use Manual DMA configuration in HS \r\n");

            return;
        }

        /* Store the DMA channel pointer. */
        pUsbApp->hbBulkInChannel = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);
    }

#if AUDIO_IF_EN
    /* Handle UAC streaming endpoint. */
    if ((endpNumber == UAC_IN_ENDPOINT) && (dir != 0)) {
        if (pUsbApp->pPDMToUsbChn != NULL) {
            DBG_APP_ERR("ISOC-IN channel already created\r\n");
            return;
        }

        dmaConfig.usbMaxPktSize = maxPktSize;
        dmaConfig.size          = UAC_XFER_BUF_SIZE;         /* DMA Buffer size in bytes */
        dmaConfig.count         = PDM_APP_BUFFER_CNT;        /* DMA Buffer Count */
        dmaConfig.prodHdrSize   = 0;                         /* No header being added. */
        dmaConfig.prodBufSize   = UAC_XFER_BUF_SIZE;
        dmaConfig.eventEnable   = 0;
        dmaConfig.intrEnable    = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                  LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.bufferMode    = false;                     /* DMA buffer mode disabled */
        dmaConfig.chType        = CY_HBDMA_TYPE_MEM_TO_IP;   /* DMA Channel type: from MEM to USB EG IP. */
        dmaConfig.prodSckCount  = 1;                         /* No. of producer sockets */
        dmaConfig.prodSck[0]    = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig.prodSck[1]    = (cy_hbdma_socket_id_t)0;   /* Producer Socket ID: None */
        dmaConfig.consSckCount  = 1;                         /* No. of consumer Sockets */
        dmaConfig.consSck[0]    = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1]    = (cy_hbdma_socket_id_t)0;   /* Consumer Socket ID: None */
        dmaConfig.cb            = Cy_UAC_HbDmaCb;              /* HB-DMA callback */
        dmaConfig.userCtx       = (void *)(pUsbApp);         /* Pass the application context as user context. */

        /* Choose the correct consumer in case of USB-HS connection. */
        if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS) {
            dmaConfig.consSck[0] = (cy_hbdma_socket_id_t)(CY_HBDMA_USBHS_IN_EP_00 + endpNumber);
        }

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                &dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS) {
            DBG_APP_ERR("ISOCIN channel create failed 0x%x\r\n", mgrStat);
            return;
        }

        /* Store the pointer to the DMA channel. */
        pUsbApp->pPDMToUsbChn = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);

        /* Get the list of DMA buffers to be used for PDM to USB transfers. */
        mgrStat = Cy_HBDma_Channel_GetBufferInfo(pUsbApp->pPDMToUsbChn, pUsbApp->pPDMRxBuffer, PDM_APP_BUFFER_CNT);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("Get Buffer INFO failed 0x%x\r\n", mgrStat);
            return;
        }

#if ((!LVCMOS_EN) && (FPGA_ENABLE))
        /* In LVDS WL to USB 2.x case, use System RAM buffers for the Audio data transfer. */
        if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS) {
            uint8_t idx;
            for (idx = 0; idx < PDM_APP_BUFFER_CNT; idx++) {
                pUsbApp->pPDMRxBuffer[idx] = (uint8_t *)glAudioDataBuf[idx];
            }
        }
#endif /* ((!LVCMOS_EN) && (FPGA_ENABLE)) */

        pUsbApp->pdmRxBufIndex      = 0;
        pUsbApp->pdmRxFreeBufCount  = PDM_APP_BUFFER_CNT;

        DBG_APP_INFO("HBDMA ISOCIN endpNum:%x ChnCreate status: %x\r\n", endpNumber, mgrStat);

        /* Enable the channel for data transfer. */
        mgrStat = Cy_HBDma_Channel_Enable(pUsbApp->pPDMToUsbChn, 0);
        DBG_APP_INFO("HBDMA PDM to USB channel enable status: %x\r\n", mgrStat);
    }
#endif /* AUDIO_IF_EN */
} /* End of Cy_USB_AppSetupEndpDmaParamsSs() */

/******************************************************************************
* Function Name: Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp,
                                                uint8_t *pEndpDscr)
*******************************************************************************
* Summary:
*  Configure and enable DMA channels.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pEndpDscr
* Endpoint descriptor pointer
*
* \return
* None
*
 *****************************************************************************/
void Cy_USB_AppSetupEndpDmaParams (cy_stc_usb_app_ctxt_t *pUsbApp,
                                   uint8_t *pEndpDscr)
{
    /* Create/configure the DMA data path. */
    Cy_USB_AppSetupEndpDmaParamsSs(pUsbApp, pEndpDscr);
} /* End of Cy_USB_AppSetupEndpDmaParams() */

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
void Cy_USB_AppConfigureEndp (cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint32_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t *pCompDscr = NULL;
    uint8_t interval = 0x00;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr))
    {
        return;
    }
    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType))
    {
        /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
        isoPkts = ((*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK) >> CY_USB_ENDP_ADDL_XN_POS) + 1;
    }

    valid = 0x01;
    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS)
    {
        /* Get companion descriptor and from there get burstSize. */
        pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbdCtxt, pEndpDscr);
        Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);
        Cy_USBD_GetEndpCompnMaxStream(pCompDscr, &maxStream);
        Cy_USBD_GetEndpInterval(pEndpDscr, &interval);
    }

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.allowNakTillDmaRdy = false;
    endpConfig.interval = interval;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_INFO("#ENDPCFG: %d, %d\r\n", endpNumber, usbdRetCode);
} /* End of Cy_USB_AppConfigureEndp() */

/******************************************************************************
* Function Name: Cy_USB_AppSetCfgCallback(void *pAppCtxt,
                                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                        cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
* Summary:
* Callback function will be invoked by USBD when set configuration is received
*
* \param pAppCtxt
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
 *****************************************************************************/
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{

    cy_uvc_devconfigured = true;
    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
    cy_en_usb_speed_t devSpeed;

    DBG_APP_INFO("AppSetCfgCbStart\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    devSpeed = pUsbApp->devSpeed;

    /*
     * Print out the contents of the USB event log buffer which has
     * logged event related to enumeration and line training.
     */
    Cy_USB_AppPrintUsbEventLog(pAppCtxt, pUsbdCtxt->pSsCalCtxt);

    /* Disable optional Low Power Mode transitions. */
    Cy_USBD_LpmDisable(pUsbdCtxt);

    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pUsbApp->pCpuDw0Base);
    Cy_DMA_Enable(pUsbApp->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg)
    {
        /* Set config should be called when active config value > 0x00. */
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00)
    {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++)
    {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL)
        {
            DBG_APP_INFO("pIntfDscrNull\r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00)
        {
            DBG_APP_INFO("numOfEndp 0\r\n");
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;

            if (devSpeed > CY_USBD_USB_DEV_HS)
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            }
            else
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }
    }

    if (devSpeed <= CY_USBD_USB_DEV_HS)
    {
        /* Enable the interrupt for the DataWire channel used for video streaming to USBHS BULK-IN endpoint. */
        Cy_USB_AppInitDmaIntr(pUsbApp->uvcInEpNum, CY_USB_ENDP_DIR_IN, Cy_UVC_DataWire_ISR);
    }

    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pUsbApp->devState = CY_USB_DEVICE_STATE_CONFIGURED;

#if USB3_LPM_ENABLE
    /* Schedule LPM enable after 1 second. */
    DBG_APP_INFO("Schedule LPM enable\r\n");
    pUsbApp->isLpmEnabled = false;
    pUsbApp->lpmEnableTime = Cy_USBD_GetTimerTick() + 1000;
#endif /* USB3_LPM_ENABLE */

    DBG_APP_INFO("AppSetCfgCbEnd Done\r\n");
} /* End of Cy_USB_AppSetCfgCallback()*/

/******************************************************************************
* Function Name: Cy_USB_AppBusResetCallback(void *pUserCtxt,cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
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
 *****************************************************************************/
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    DBG_APP_INFO("AppBusResetCallback\r\n");

    /* Stop and destroy the high bandwidth DMA channel if present. To be done before AppInit is called. */
    if (pUsbApp->hbBulkInChannel != NULL)
    {
        DBG_APP_TRACE("HBDMA destroy\r\n");
        Cy_HBDma_Channel_Disable(pUsbApp->hbBulkInChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbBulkInChannel);
        pUsbApp->hbBulkInChannel = NULL;
    }

    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pUsbApp, pUsbdCtxt, pUsbApp->pCpuDmacBase,
                   pUsbApp->pCpuDw0Base, pUsbApp->pCpuDw1Base,
                   pUsbApp->pHbDmaMgrCtxt);
    pUsbApp->devState = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_RESET;

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
#if USB3_LPM_ENABLE
        DBG_APP_INFO("Disabling LPM\r\n");
        Cy_USBSS_Cal_LPMDisable(pUsbdCtxt->pSsCalCtxt);
#endif /* USB3_LPM_ENABLE */
    }

    cy_uvc_devconfigured = false;
    cy_uvc_IsApplnActive = false;
} /* End of Cy_USB_AppBusResetCallback() */

/******************************************************************************
* Function Name: Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
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
 *****************************************************************************/
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    DBG_APP_INFO("ppBusResetDoneCallback\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->prevDevState = pUsbApp->devState;
} /* End of Cy_USB_AppBusResetDoneCallback(). */

extern uint8_t CyFxUSB20DeviceDscr[];

/******************************************************************************
* Function Name: Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
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
 *****************************************************************************/
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    Cy_USBD_SetDscr(pUsbApp->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
} /* End of Cy_USB_AppBusSpeedCallback() */

/******************************************************************************
* Function Name: Cy_USB_AppSetupCallback(void *pAppCtxt,
*                                   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
*                                   cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
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
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType, bTarget;
    uint16_t wValue, wIndex, wLength;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t pdStatus = 0;

    DBG_APP_TRACE("AppSetupCallback\r\n\r\n");

    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function. */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bType = ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    pUsbApp->wValue = wValue;
    pUsbApp->wIndex = wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;

    if (bType == CY_USB_CTRL_REQ_STD)
    {
        DBG_APP_TRACE("CY_USB_CTRL_REQ_STD\r\n");
        if (bRequest == CY_USB_SC_SET_FEATURE)
        {
            DBG_APP_TRACE("CY_USB_SC_SET_FEATURE\r\n");
            /* Function Suspend: ACK the request if the device is in configured state in an USB 3.x connection. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) &&
                (wValue == CY_USB_FEATURE_FUNC_SUSPEND))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF\r\n");
                DBG_APP_INFO("CY_USB_FEATURE_FUNC_SUSPEND\r\n");
                DBG_APP_INFO("cy_uvc_devconfigured = %d\r\n",cy_uvc_devconfigured);
                if ((pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) && (cy_uvc_devconfigured))
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                else
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            /* SET-FEATURE(EP-HALT) is only supported to facilitate Chapter 9 compliance tests. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_ENDP\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                        epDir, true);

                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE))) {
                /* Set U1/U2 enable. Just ACK the request. */
#if USB3_LPM_ENABLE
                if (pUsbApp->isLpmEnabled == false) {
                DBG_APP_INFO("Enabling LPM\r\n");
                pUsbApp->isLpmEnabled = true;
                Cy_USBD_LpmEnable(pUsbdCtxt);
            }
#endif /* USB3_LPM_ENABLE */
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if (bRequest == CY_USB_SC_CLEAR_FEATURE)
        {
            DBG_APP_INFO("CY_USB_SC_CLEAR_FEATURE\r\n");
            /* Function Suspend: ACK the request if the device is in configured state in an USB 3.x connection. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF\r\n");
                if ((pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) && (cy_uvc_devconfigured))
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                else
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_ENDP && CY_USB_FEATURE_ENDP_HALT\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                if ((epDir == CY_USB_ENDP_DIR_OUT) || ((wIndex & 0x7F) != pUsbApp->uvcInEpNum))
                {
                    /* For any EP other than the UVC streaming endpoint, just clear the STALL bit. */
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                            epDir, false);
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                }
                else
                {

                    xMsg.type = CY_USB_UVC_VIDEO_STREAM_STOP_EVENT;
                    xMsg.data[0] = wIndex;
                    pdStatus = xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    ASSERT_NON_BLOCK(pdTRUE == pdStatus, pdStatus);
                    Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                }

                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE)))
            {
                /* Set U1/U2 disable. Just ACK the request. */
#if USB3_LPM_ENABLE
                DBG_APP_INFO("Disabling LPM\r\n");
                pUsbApp->isLpmEnabled  = false;
                pUsbApp->lpmEnableTime = 0;
                Cy_USBD_LpmDisable(pUsbdCtxt);
#endif /* USB3_LPM_ENABLE */
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }
    }

    if (bType == CY_USB_CTRL_REQ_VENDOR)
    {
        /* user-defined code section */
        isReqHandled = true;
    }

    /* Check for UVC Class Requests */
    if (bType == CY_USB_CTRL_REQ_CLASS)
    {

        /* In USBSS connection, disable LPM entry for the next 100 ms. */
        if (pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
        {
            Cy_USB_App_KeepLinkActive(pUsbApp);
        }

        DBG_APP_TRACE("UVC Class Requests\r\n");
        /* Handle requests addressed to the Video Control interface. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) &&
            (CY_USB_GET_LSB(wIndex) == CY_USB_UVC_INTERFACE_VC))
        {
            DBG_APP_TRACE("CY_USB_CTRL_REQ_RECIPENT_INTF && CY_USB_UVC_INTERFACE_VC\r\n");
            /* Respond to CY_USB_UVC_INTERFACE_VC  */
            xMsg.type    = CY_USB_UVC_DEVICE_VIDEO_CNTRL_RQT_EVENT;
            xMsg.data[0] = wLength;
            pdStatus = xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
            ASSERT_NON_BLOCK(pdTRUE == pdStatus, pdStatus);
            isReqHandled = true;
        }

        /* Handle requests addressed to the Video Streaming interface. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (CY_USB_GET_LSB(wIndex) == CY_USB_UVC_INTERFACE_VS))
        {
            isReqHandled = true;
            DBG_APP_TRACE("CY_USB_CTRL_REQ_RECIPENT_INTF && CY_USB_UVC_INTERFACE_VS = 0x%x\r\n",CY_GET_LSB(wIndex));
            switch (wValue)
            {

            case CY_USB_UVC_VS_PROBE_CONTROL:
            case CY_USB_UVC_VS_COMMIT_CONTROL:
            {
                switch (bRequest)
                {
                case CY_USB_UVC_GET_INFO_REQ:
                    DBG_APP_TRACE("UVC_GET_INFO_REQ\r\n");
                    Ep0XferBuffer[0] = 3; /* GET/SET requests are supported. */
                    retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)Ep0XferBuffer, 0x01);
                    ASSERT_NON_BLOCK(CY_USBD_STATUS_SUCCESS == retStatus,retStatus);
                    break;

                case CY_USB_UVC_GET_LEN_REQ:
                    DBG_APP_TRACE("UVC_GET_LEN_REQ\r\n");
                    Ep0XferBuffer[0] = CY_USB_UVC_MAX_PROBE_SETTING;
                    retStatus = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)Ep0XferBuffer, 0x01);
                    ASSERT_NON_BLOCK(CY_USBD_STATUS_SUCCESS == retStatus,retStatus);
                    break;

                /* We only have one functional setting. Keep returning the same as current, default
                 * minimum and maximum. */
                case CY_USB_UVC_GET_CUR_REQ:
                case CY_USB_UVC_GET_DEF_REQ:
                case CY_USB_UVC_GET_MIN_REQ:
                case CY_USB_UVC_GET_MAX_REQ:
                    DBG_APP_TRACE("UVC_GET_CUR_DEF_MIN_MAX_REQ:%x \r\n", bRequest);

                    /*
                     * Notify the UVC task to handle the GET_CUR request. We need to ensure that the SET_CUR handling
                     * is complete before GET_CUR response is sent.
                     */
                    xMsg.type    = CY_USB_UVC_DEVICE_GET_CUR_RQT;
                    xMsg.data[0] = wLength;
                    xMsg.data[1] = wValue;
                    xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    isReqHandled = true;
                    break;

                case CY_USB_UVC_SET_CUR_REQ:
                    /* Since this request has OUT data, it should be handled in task context. */
                    DBG_APP_TRACE("CY_USB_UVC_SET_CUR_REQ\r\n");
                    xMsg.type    = CY_USB_UVC_DEVICE_SET_CUR_RQT;
                    xMsg.data[0] = wLength;
                    xMsg.data[1] = wValue;
                    pdStatus = xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    ASSERT_NON_BLOCK(pdTRUE == pdStatus, pdStatus);
                    isReqHandled = true;
                    break;

                default:
                    isReqHandled = false;
                    break;
                }
            }
            break;

            default:
                isReqHandled = false;
                break;
            }
        }

        /* Don't try to stall the endpoint if we have already attempted data transfer. */
    }

    /* SET_SEL request is supposed to have an OUT data phase of 6 bytes. */
    if ((bRequest == CY_USB_SC_SET_SEL) && (wLength == 6))
    {
        /* SET_SEL request is only received in USBSS case and the Cy_USB_USBD_RecvEndp0Data is blocking. */
        retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)SetSelDataBuffer, wLength);
        DBG_APP_INFO("SET_SEL: EP0 recv stat = %d, Data=%x:%x\r\n",
                retStatus, SetSelDataBuffer[0], SetSelDataBuffer[1]);
        isReqHandled = true;
    }

    /*
     * If Request is not handled by the callback, Stall the command.
     */
    if (!isReqHandled)
    {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00,
                                      CY_USB_ENDP_DIR_IN, TRUE);
    }
    DBG_APP_INFO(" isReqHandled  = %d\r\n",isReqHandled);
} /* End of Cy_USB_AppSetupCallback() */

/******************************************************************************
* Function name: Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
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
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
#if DEEPSLEEP_EN
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
#endif /* DEEPSLEEP_EN */

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->prevDevState = pUsbApp->devState;
    pUsbApp->devState = CY_USB_DEVICE_STATE_SUSPEND;

#if DEEPSLEEP_EN
    xMsg.type = CY_USB_ENTER_DEEPSLEEP;
    xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
#endif /* DEEPSLEEP_EN */
} /* End of Cy_USB_AppSuspendCallback() */

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
void Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    cy_en_usb_device_state_t tempState;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    tempState = pUsbApp->devState;
    pUsbApp->devState = pUsbApp->prevDevState;
    pUsbApp->prevDevState = tempState;
} /* End of Cy_USB_AppResumeCallback() */

/******************************************************************************
* Function name: Cy_USB_AppSetIntfCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* Callback function will be invoked by USBD when SET_INTERFACE is  received
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
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_setup_req_t *pSetupReq;
    uint8_t intfNum, altSetting;

    DBG_APP_INFO("AppSetIntfCallback Start\r\n");
    pSetupReq  = &(pUsbdCtxt->setupReq);
    intfNum    = pSetupReq->wIndex;
    altSetting = pSetupReq->wValue;

#if AUDIO_IF_EN
    if (intfNum == UAC_STREAM_INTF_NUM) {
        Cy_UAC_AppSetIntfHandler((cy_stc_usb_app_ctxt_t *)pAppCtxt, altSetting);
        return;
    }
#endif /* AUDIO_IF_EN */

    /* Interface does not support multiple alternate settings: Stall the SET_INTERFACE request. */
    DBG_APP_INFO("SetIntf(%d) on interface %d: Stalled\r\n", altSetting, intfNum);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
    DBG_APP_INFO("AppSetIntfCallback done\r\n");
} /* End of Cy_USB_AppSetIntfCallback() */

/******************************************************************************
* Function name: Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when ZLP message comes
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
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    if (pMsg->type == CY_USB_CAL_MSG_OUT_ZLP) {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), 0);
    }
} /* end of function. */

/******************************************************************************
* Function name: Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when L1 Sleep message comes.
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
void Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1SleepCb\r\n");
} /* end of function. */

/******************************************************************************
* Function name: Cy_USB_AppL1ResumeCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
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
void Cy_USB_AppL1ResumeCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1ResumeCb\r\n");
} /* End of Cy_USB_AppL1ResumeCallback() */

/******************************************************************************
* Function name: Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when SLP message comes.
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
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    if (pMsg->type == CY_USB_CAL_MSG_OUT_SLP) {
        Cy_HBDma_Mgr_HandleUsbShortInterrupt(pAppCtxt->pHbDmaMgrCtxt, (pMsg->data[0] & 0x7FU), pMsg->data[1]);
    }
} /* End of Cy_USB_AppSlpCallback() */

/******************************************************************************
* Function name: Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when set feature message comes.
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
void Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppSetFeatureCb\r\n");
} /* End of function. */

/******************************************************************************
* Function name: Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
*******************************************************************************
*
* This Function will be called by USBD layer when clear feature message comes.
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
void Cy_USB_AppClearFeatureCallback(void *pUsbApp,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppClearFeatureCb\r\n");
} /* end of function. */

/******************************************************************************
* Function name: Cy_USB_AppInitDmaIntr(uint32_t endpNumber,,
                    cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr)
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
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                           cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;
    if ((endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER))
    {
#if (!CY_CPU_CORTEX_M4)
        intrCfg.intrPriority = 3;
        intrCfg.intrSrc = NvicMux7_IRQn;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#else
        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#endif /* (!CY_CPU_CORTEX_M4) */

        if (userIsr != NULL)
        {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        }
        else
        {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
} /* End of Cy_USB_AppInitDmaIntr() */


/*[]*/

