/******************************************************************************
* \file cy_usb_videocontrol.c
* \version 1.0
*
* Implements function for video control interface
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

#include "cy_usb_videocontrol.h"
#include "cy_usb_uvc_device.h"
#include "cy_debug.h"
#include "cy_usb_i2c.h"

#if VIDEO_CONTROL_INTERFACE

typedef struct cy_stc_video_control_uint32
{
    uint32_t CurrentVal;    /* Current Value of the feature */
    uint32_t MinVal;        /* Minimum Value supported by the feature */
    uint32_t MaxVal;        /* Maximum Value supported by the feature */
    uint32_t Res;           /* Resolution supported by the feature */
    uint32_t DefaultVal;    /* Default Value of the feature */
    uint32_t SetVal;        /* Set Value sent by the host application */
} cy_stc_video_control_uint32_t;

typedef struct cy_stc_video_control_uint16
{
    uint16_t CurrentVal;    /* Current Value of the feature */
    uint16_t MinVal;        /* Minimum Value supported by the feature */
    uint16_t MaxVal;        /* Maximum Value supported by the feature */
    uint16_t Res;           /* Resolution supported by the feature */
    uint16_t DefaultVal;    /* Default Value of the feature */
    uint16_t SetVal;        /* Set Value sent by the host application */
} cy_stc_video_control_uint16_t;

typedef struct cy_stc_video_control_uint8
{
    uint8_t CurrentVal;     /* Current Value of the feature */
    uint8_t MinVal;         /* Minimum Value supported by the feature */
    uint8_t MaxVal;         /* Maximum Value supported by the feature */
    uint8_t Res;            /* Resolution supported by the feature */
    uint8_t DefaultVal;     /* Default Value of the feature */
    uint8_t SetVal;         /* Set Value sent by the host application */
} cy_stc_video_control_uint8_t;

typedef struct cy_stc_video_control_int32
{
    int32_t CurrentVal;     /* Current Value of the feature */
    int32_t MinVal;         /* Minimum Value supported by the feature */
    int32_t MaxVal;         /* Maximum Value supported by the feature */
    int32_t Res;            /* Resolution supported by the feature */
    int32_t DefaultVal;     /* Default Value of the feature */
    int32_t SetVal;         /* Set Value sent by the host application */
} cy_stc_video_control_int32_t;

typedef struct cy_stc_video_control_int16
{
    int16_t CurrentVal;     /* Current Value of the feature */
    int16_t MinVal;         /* Minimum Value supported by the feature */
    int16_t MaxVal;         /* Maximum Value supported by the feature */
    int16_t Res;            /* Resolution supported by the feature */
    int16_t DefaultVal;     /* Default Value of the feature */
    int16_t SetVal;         /* Set Value sent by the host application */
} cy_stc_video_control_int16_t;

typedef struct cy_stc_video_control_int8
{
    int8_t CurrentVal;      /* Current Value of the feature */
    int8_t MinVal;          /* Minimum Value supported by the feature */
    int8_t MaxVal;          /* Maximum Value supported by the feature */
    int8_t Res;             /* Resolution supported by the feature */
    int8_t DefaultVal;      /* Default Value of the feature */
    int8_t SetVal;          /* Set Value sent by the host application */
} cy_stc_video_control_int8_t;

USB3_DESC_ATTRIBUTES uint8_t EP0RxBuffer[CY_USB_UVC_MAX_PROBE_SETTING_ALIGNED];
USB3_DESC_ATTRIBUTES uint8_t Ep0TxBuffer[CY_USB_UVC_MAX_PROBE_SETTING_ALIGNED];

/* Video Control Features - Camera Terminal - Structure Definitions */
static cy_stc_video_control_uint8_t AE = {8, 0, 0, 9, 8, 1};    /* 1=Manual Mode, 8=Aperture Priority Mode */
static cy_stc_video_control_uint8_t AEPriority = {0, 0, 1, 1, 0, 1};
static cy_stc_video_control_uint32_t ExpAbs = {167, 2, 167, 1, 167, 1};
static cy_stc_video_control_uint16_t ManualFocus = {3000, 80, 3000, 1, 3000, 1};
static cy_stc_video_control_uint8_t AutoFocus = {1, 0, 1, 1, 1, 1};
static cy_stc_video_control_uint16_t Roll = {0, 0, 180, 180, 0, 0};

#if ENABLE_PAN_TILT_CONTROL
static cy_stc_video_control_int32_t Pan = {0, -90 * 3600, 90 * 3600, 3600, 0, 0};
static cy_stc_video_control_int32_t Tilt = {0, -90 * 3600, 90 * 3600, 3600, 0, 0};
#endif

/* Video Control Features - Processing Unit - Structure Definitions */
static cy_stc_video_control_uint16_t BacklightCompensation = {0, 0, 1, 1, 0, 1};
static cy_stc_video_control_uint16_t Contrast = {2, 0, 4, 1, 2, 1};
static cy_stc_video_control_uint16_t Saturation = {100, 0, 200, 1, 100, 1};
static cy_stc_video_control_uint8_t PLFreq = {1, 1, 2, 0, 1, 1};
static cy_stc_video_control_uint8_t AWB_Temp = {1, 0, 0, 1, 1, 1};
static cy_stc_video_control_uint8_t AWB_Comp = {1, 0, 12, 1, 1, 1};
static cy_stc_video_control_uint16_t Sharpness = {3, 0, 7, 1, 3, 1};
static cy_stc_video_control_uint16_t Gamma = {220, 100, 300, 10, 220, 1};
static cy_stc_video_control_uint16_t MWB_Temp = {5500, 2500, 15000, 100, 5500, 1};
static cy_stc_video_control_uint16_t MWB_Comp_Blue = {0, 0, 255, 1, 0, 1};
static cy_stc_video_control_uint16_t MWB_Comp_Red = {0, 0, 255, 1, 0, 1};
static cy_stc_video_control_int16_t Brightness = {0, -12, 12, 1, 0, 1};
static cy_stc_video_control_int16_t Hue = {0, -3000, 3000, 150, 0, 1};
static cy_stc_video_control_uint16_t Gain = {0x400, 0x100, 0xFB30, 1, 0x400, 1};
cy_stc_video_control_uint8_t LEDTorchLevel = {0, 0, 100, 10, 0, 1};

uint8_t LED_Status = 0;
static uint8_t VC_Get_Info = 0x03;

/* Camera Control Command to Ti180 */
#define ADDR_GAIN_MODE          0x1000
#define ADDR_MANUAL_GAIN        0x1004
#define ADDR_SHUTTER_MODE       0x1010
#define ADDR_MANUAL_SHUTTER     0x1014
#define ADDR_METERING_MODE      0x1030
#define ADDR_AE_COMPENSATION    0x1038
#define ADDR_WB_MODE            0x1100
#define ADDR_WB_PRESET_TEMP     0x111c
#define ADDR_DETAIL             0x1304
#define ADDR_COLOR_SATURATION   0x1358
#define ADDR_COLOR_ADJ_HUE      0x1360
#define ADDR_GAMMA              0x1504
#define ADDR_SENSOR_MIRROR      0x3030

static uint32_t Cy_VideoControlUnsupportedRequest(const char *funcName,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t bRequest)
{
    DBG_APP_ERR("%s: Unsupported request = 0x%X\n\r", funcName, bRequest);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0, CY_USB_ENDP_DIR_IN, true);
    return 1u;
}

/******************************************************************************
* Function Name: Cy_VideoControlGetInfo_0x03(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt)
*******************************************************************************
* Summary:
* Function that sends GetInfo command when request comes from host
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \return
* Zero on success, non-zero code on failure.
*
*****************************************************************************/
static uint32_t Cy_VideoControlGetInfo_0x03(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt)
{
    cy_en_usbd_ret_code_t status;

    Ep0TxBuffer[0] = 0x03;
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_ERR(" Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
}

/******************************************************************************
* Function Name: Cy_VideoControlHandleVCInterfaceRequest(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle all supported UVC Video Control requests
*
* \param pUsbdCtxt
* Application layer context pointer.
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
/* Function to handle all supported UVC Video Control requests */
uint32_t Cy_VideoControlHandleVCInterfaceRequest(cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength)
{
    uint32_t status = 1;
    uint16_t wIndex = pAppCtxt->wIndex;
    uint16_t wValue = pAppCtxt->wValue;
    uint8_t bRequest = pAppCtxt->bRequest;

    if (CY_GET_MSB(wIndex) == CY_UVC_VC_CAMERA_TERMINAL_ID)
    {
        /* Video Control - Camera Terminal */
        DBG_APP_INFO("VC:T ID 0x%x CS 0x%x Req 0x%x \n\r", CY_GET_MSB(wIndex), wValue, bRequest);

        switch (CY_GET_MSB(wValue))
        {
            case CT_AE_MODE_CONTROL:
                /* Auto-Exposure Mode Selection */
                status = Cy_VideoControlHandleAEControlReq(pAppCtxt, bRequest, wLength);
                break;

            case CT_AE_PRIORITY_CONTROL:
                /* Aperture Priority (Low light compensation) Control */
                status = Cy_VideoControlHandleAEPriorityControlReq(pAppCtxt, bRequest, wLength);
                break;

            case CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                /* Manual Exposure Control */
                status = Cy_VideoControlHandleExpAbsControlReq(pAppCtxt, bRequest, wLength);
                break;

            case CT_FOCUS_ABSOLUTE_CONTROL:
                /* Manual Focus Control */
                status = Cy_VideoControlHandleManualFocusControlReq(pAppCtxt, bRequest, wLength);
                break;

            case CT_FOCUS_AUTO_CONTROL:
                /* Auto Focus Mode Selection */
                status = Cy_VideoControlHandleAutoFocusControlReq(pAppCtxt, bRequest, wLength);
                break;

            case CT_PANTILT_ABSOLUTE_CONTROL:
                /* Pan-Tilt Control */
#if ENABLE_PAN_TILT_CONTROL
                status = Cy_VideoControlHandlePanTiltControlReq(pAppCtxt, bRequest, wLength);
#endif
                break;

            case CT_ROLL_ABSOLUTE_CONTROL:
                /* Roll Control */
                status = Cy_VideoControlHandleRollControlReq(pAppCtxt, bRequest, wLength);
                break;

            default:
                break;
        }
    }
    else if (CY_GET_MSB(wIndex) == CY_UVC_VC_PROCESSING_UNIT_ID)
    {
        /* Video Control - Processing Unit */
        DBG_APP_INFO("VC:T ID 0x%x CS 0x%x Req 0x%x \n\r", CY_GET_MSB(wIndex), wValue, bRequest);

        switch (CY_GET_MSB(wValue))
        {
            case PU_BACKLIGHT_COMPENSATION_CONTROL:
                /* Back Light Compensation */
                status = Cy_VideoControlHandleBacklightCompensationControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_BRIGHTNESS_CONTROL:
                /* Brightness Control */
                status = Cy_VideoControlHandleBrightnessControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_CONTRAST_CONTROL:
                /* Contrast Control */
                status = Cy_VideoControlHandleContrastControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_HUE_CONTROL:
                /* Hue Control */
                status = Cy_VideoControlHandleHueControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_GAIN_CONTROL:
                /* Gain Control */
                status = Cy_VideoControlHandleGainControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_SATURATION_CONTROL:
                /* Saturation Control */
                status = Cy_VideoControlHandleSaturationControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_SHARPNESS_CONTROL:
                /* Sharpness Control */
                status = Cy_VideoControlHandleSharpnessControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_GAMMA_CONTROL:
                /* Gamma Control */
                status = Cy_VideoControlHandleGammaControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_POWER_LINE_FREQUENCY_CONTROL:
                /* Power Line Frequency (Flicker) Control */
                status = Cy_VideoControlHandlePowerLineFreqControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
                /* Auto White Balance Mode Control */
                status = Cy_VideoControlHandleAWBTempControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
                /* White Balance Temperature Control */
                status = Cy_VideoControlHandleMWBTempControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_WHITE_BALANCE_COMPONENT_CONTROL:
                /* White Balance Component Control */
                status = Cy_VideoControlHandleMWBCompControlReq(pAppCtxt, bRequest, wLength);
                break;

            case PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL:
                /* Auto White Balance Component Control */
                status = Cy_VideoControlHandleAWBCompControlReq(pAppCtxt, bRequest, wLength);
                break;

            default:
                break;
        }
    }

    return status;
} /* End of Cy_VideoControlHandleVCInterfaceRequest() */

/* Requests to be supported for all the Video Control features
   CY_USB_UVC_GET_CUR_REQ
   CY_USB_UVC_GET_MIN_REQ
   CY_USB_UVC_GET_MAX_REQ
   CY_USB_UVC_GET.Res_REQ
   CY_USB_UVC_GET_DEF_REQ
   CY_USB_UVC_SET_CUR_REQ
*/

/******************************************************************************
* Function Name: Cy_VideoControlHandleAEControlReq(cy_stc_usb_app_ctxt_t
*                                   *pAppCtxt, uint8_t bRequest,
*                                   uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Auto-Exposure Mode Selection Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleAEControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            Ep0TxBuffer[0] = AE.CurrentVal;
            DBG_APP_INFO("AE Get Cur  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            Ep0TxBuffer[0] = AE.Res;
            DBG_APP_INFO("AE Get Res  = %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            Ep0TxBuffer[0] = AE.DefaultVal;
            DBG_APP_INFO("AE Get Def  = %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                AE.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("AE Set Cur  = %d\n\r", AE.SetVal);
                AE.CurrentVal = AE.SetVal;
                /* SendCmd(ADDR_SHUTTER_MODE, AE.SetVal == 1 ? 0 : 1); */
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleAEControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleAEPriorityControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Aperture Priority (Low light compensation)
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleAEPriorityControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            Ep0TxBuffer[0] = AEPriority.CurrentVal;
            DBG_APP_INFO("AEPriority Get Cur  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            Ep0TxBuffer[0] = AEPriority.MinVal;
            DBG_APP_INFO("AEPriority Get Min  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            Ep0TxBuffer[0] = AEPriority.MaxVal;
            DBG_APP_INFO("AEPriority Get Max  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            Ep0TxBuffer[0] = AEPriority.Res;
            DBG_APP_INFO("AEPriority Get Res  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("AEPriority Get Info  =  %d\n\r", Ep0TxBuffer[0]);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            Ep0TxBuffer[0] = AEPriority.DefaultVal;
            DBG_APP_INFO("AEPriority Get Def  =  %d\n\r", Ep0TxBuffer[0]);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                AEPriority.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("AEPriority Set Cur  = 0x%X\n\r", AEPriority.SetVal);

                if (AEPriority.CurrentVal != AEPriority.SetVal)
                {
                    AEPriority.CurrentVal = AEPriority.SetVal;

                    if (AEPriority.SetVal == 1)
                    {
                        /* Enable Low Light Compensation.
                           If enabled, frame rate will change depending on light condition.
                           If fixed frame rate is required, low light compensation should be disabled. */
                        /* TODO */
                    }
                    else
                    {
                        /* TODO */
                    }
                }
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleAEPriorityControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleExpAbsControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Manual Exposure Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleExpAbsControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint32_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = ExpAbs.CurrentVal;
            DBG_APP_INFO("ExpAbs Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = ExpAbs.MinVal;
            DBG_APP_INFO("ExpAbs Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = ExpAbs.MaxVal;
            DBG_APP_INFO("ExpAbs Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = ExpAbs.Res;
            DBG_APP_INFO("ExpAbs Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = ExpAbs.DefaultVal;
            DBG_APP_INFO("ExpAbs Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                int32_t shutterValue;

                ExpAbs.SetVal = *(uint32_t *)EP0RxBuffer;
                DBG_APP_INFO("ExpAbs Set Cur  = %d\n\r", ExpAbs.SetVal);
                ExpAbs.CurrentVal = ExpAbs.SetVal;

                shutterValue = ExpAbs.SetVal * (1 << 20) / 10000;

                if (shutterValue > 0x4444)
                {
                    shutterValue = 0x4444;
                    /* Max */
                }
                else if (shutterValue < 0xd2)
                {
                    shutterValue = 0xd2;
                    /* Min */
                }

                (void)shutterValue;
                /* SendCmd(ADDR_MANUAL_SHUTTER, shutterValue); */
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_DWORD_GET_BYTE0(wrVal);
    Ep0TxBuffer[1] = CY_USB_DWORD_GET_BYTE1(wrVal);
    Ep0TxBuffer[2] = CY_USB_DWORD_GET_BYTE2(wrVal);
    Ep0TxBuffer[3] = CY_USB_DWORD_GET_BYTE3(wrVal);

    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 4);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleExpAbsControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleManualFocusControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Manual Focus Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleManualFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = ManualFocus.CurrentVal;
            DBG_APP_INFO("ManualFocus Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = ManualFocus.MinVal;
            DBG_APP_INFO("ManualFocus Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = ManualFocus.MaxVal;
            DBG_APP_INFO("ManualFocus Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = ManualFocus.Res;
            DBG_APP_INFO("ManualFocus Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("ManualFocus Get Info  = %d\n\r", Ep0TxBuffer[0]);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = ManualFocus.DefaultVal;
            DBG_APP_INFO("ManualFocus Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                ManualFocus.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("ManualFocus Set Cur  = 0x%X\n\r", ManualFocus.SetVal);

                if (ManualFocus.CurrentVal != ManualFocus.SetVal)
                {
                    /* TODO: Function to control focus. */
                    /* TODO: Verify the focus status. */
                    ManualFocus.CurrentVal = ManualFocus.SetVal;
                }
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleManualFocusControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleAutoFocusControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Auto Focus Mode Selection Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleAutoFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint8_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = AutoFocus.CurrentVal;
            DBG_APP_INFO("AutoFocus Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = AutoFocus.MinVal;
            DBG_APP_INFO("AutoFocus Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = AutoFocus.MaxVal;
            DBG_APP_INFO("AutoFocus Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = AutoFocus.Res;
            DBG_APP_INFO("AutoFocus Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("AutoFocus Get Info  = %d\n\r", Ep0TxBuffer[0]);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = AutoFocus.DefaultVal;
            DBG_APP_INFO("AutoFocus Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                AutoFocus.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("AutoFocus Set Cur  = 0x%X\n\r", AutoFocus.SetVal);

                if (AutoFocus.CurrentVal != AutoFocus.SetVal)
                {
                    AutoFocus.CurrentVal = AutoFocus.SetVal;

                    if (AutoFocus.SetVal == 0x01)
                    {
                        /* Set Auto-Focus Mode */
                        /* TODO: Autofocus start. */
                        /* TODO: Verify status. */
                    }
                    else
                    {
                        /* Set Manual-Focus Mode */
                        /* TODO: Stop autofocus. */
                    }
                }
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = wrVal;
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleAutoFocusControlReq() */

#if ENABLE_PAN_TILT_CONTROL

/******************************************************************************
* Function Name: Cy_VideoControlHandlePanTiltControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for PAN-TILT Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandlePanTiltControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    uint16_t readCount = 0;
    int32_t PanVal;
    int32_t TiltVal;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint32_t Pan_Threshold;
    uint32_t Tilt_Threshold;

    (void)readCount;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            PanVal = Pan.CurrentVal;
            TiltVal = Tilt.CurrentVal;

            DBG_APP_INFO("Pan Get Cur  = %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            PanVal = Pan.MinVal;
            TiltVal = Tilt.MinVal;

            DBG_APP_INFO("Pan Get Min  = %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            PanVal = Pan.MaxVal;
            TiltVal = Tilt.MaxVal;

            DBG_APP_INFO("Pan Get Max  = %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            PanVal = Pan.Res;
            TiltVal = Tilt.Res;

            DBG_APP_INFO("Pan Get Res  =  %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            PanVal = VC_Get_Info;
            TiltVal = VC_Get_Info;

            DBG_APP_INFO("Pan Get Info  =  %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_GET_DEF_REQ:
            PanVal = Pan.DefaultVal;
            TiltVal = Tilt.DefaultVal;

            DBG_APP_INFO("Pan Get Def  =  %d, Tilt = %d\n\r", PanVal, TiltVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                Pan.SetVal = (
                        ((int32_t)EP0RxBuffer[3] << 24) | ((int32_t)EP0RxBuffer[2] << 16) |
                        ((int32_t)EP0RxBuffer[1] << 8) | ((int32_t)EP0RxBuffer[0] << 0));
                Tilt.SetVal = (
                        ((int32_t)EP0RxBuffer[7] << 24) | ((int32_t)EP0RxBuffer[6] << 16) |
                        ((int32_t)EP0RxBuffer[5] << 8) | ((int32_t)EP0RxBuffer[4] << 0));

                DBG_APP_INFO("Pan Set Cur  = %d, Tilt Set Cur = %d\n\r", Pan.SetVal, Tilt.SetVal);

                if (Pan.CurrentVal != Pan.SetVal)
                {
                    Pan.CurrentVal = Pan.SetVal;
                    Pan_Threshold = 7480 - (((Pan.SetVal / 3600 + 90) / 10) * 22);
                    /* Map the UVC supported scale for Pan to the duty cycle required by Stepper Motor. */
                    DBG_APP_INFO("Pan_Threshold = %d", Pan_Threshold);

                    /* TODO: Update the duty cycle of the PWM to rotate the stepper motor.
                       For this information, check the datasheet of Stepper Motor Module. */
                }

                if (Tilt.CurrentVal != Tilt.SetVal)
                {
                    Tilt.CurrentVal = Tilt.SetVal;
                    Tilt_Threshold = 7480 - (((Tilt.SetVal / 3600 + 90) / 10) * 22);
                    /* Map the UVC supported scale for Tilt to the duty cycle required by Stepper Motor. */

                    DBG_APP_INFO("Tilt_Threshold = %d\n\r", Tilt_Threshold);

                    /* TODO: Update the duty cycle of the PWM to rotate the stepper motor.
                       For this information, check the datasheet of Stepper Motor Module. */
                }
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_DWORD_GET_BYTE0(PanVal);
    Ep0TxBuffer[1] = CY_USB_DWORD_GET_BYTE1(PanVal);
    Ep0TxBuffer[2] = CY_USB_DWORD_GET_BYTE2(PanVal);
    Ep0TxBuffer[3] = CY_USB_DWORD_GET_BYTE3(PanVal);

    Ep0TxBuffer[4] = CY_USB_DWORD_GET_BYTE0(TiltVal);
    Ep0TxBuffer[5] = CY_USB_DWORD_GET_BYTE1(TiltVal);
    Ep0TxBuffer[6] = CY_USB_DWORD_GET_BYTE2(TiltVal);
    Ep0TxBuffer[7] = CY_USB_DWORD_GET_BYTE3(TiltVal);

    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 8);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandlePanTiltControlReq() */

#endif /* ENABLE_PAN_TILT_CONTROL */

/******************************************************************************
* Function Name: Cy_VideoControlHandleRollControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Roll Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleRollControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Roll.CurrentVal;
            DBG_APP_INFO("Roll Get Cur  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Roll.MinVal;
            DBG_APP_INFO("Roll Get Min  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Roll.MaxVal;
            DBG_APP_INFO("Roll Get Max  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Roll.Res;
            DBG_APP_INFO("Roll Get Res  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("Roll Get Info  =  %d\n\r", Ep0TxBuffer[0]);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Roll.DefaultVal;
            DBG_APP_INFO("Roll Get Def  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                Roll.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("Roll Set Cur  = 0x%X\n\r", Roll.SetVal);

                if (Roll.CurrentVal != Roll.SetVal)
                {
                    Roll.CurrentVal = Roll.SetVal;

                    /* if (Roll.SetVal == 0) */
                    /* TODO: flip */

                    /* else if (Roll.SetVal == 180) */
                    /* TODO: To achieve 1080 degree rotation, the video frame has to be flipped and rotated. */
                }
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleRollControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleBacklightCompensationControlReq(
*                                           cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Back Light Compensation Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleBacklightCompensationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = BacklightCompensation.CurrentVal;
            DBG_APP_INFO("BacklightCompensation Get Cur  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = BacklightCompensation.MinVal;
            DBG_APP_INFO("BacklightCompensation Get Min  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = BacklightCompensation.MaxVal;
            DBG_APP_INFO("BacklightCompensation Get Max  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = BacklightCompensation.Res;
            DBG_APP_INFO("BacklightCompensation Get Res  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = BacklightCompensation.DefaultVal;
            DBG_APP_INFO("BacklightCompensationB Get Def  =  %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                BacklightCompensation.SetVal = *(uint16_t *)EP0RxBuffer;
                DBG_APP_INFO("BacklightCompensation Set Cur  = 0x%X\n\r", BacklightCompensation.SetVal);
                BacklightCompensation.CurrentVal = BacklightCompensation.SetVal;
                /* SendCmd(ADDR_METERING_MODE, BacklightCompensation.SetVal == 0 ? 1 : 3); */
            }
            else
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleBacklightCompensationControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleBrightnessControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Brightness control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleBrightnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    int16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            /* SendCmd(ADDR_SENSOR_MIRROR, 0); */
            wrVal = Brightness.CurrentVal;
            DBG_APP_INFO("Brightness Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Brightness.MinVal;
            DBG_APP_INFO("Brightness Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Brightness.MaxVal;
            DBG_APP_INFO("Brightness Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Brightness.Res;
            DBG_APP_INFO("Brightness Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Brightness.DefaultVal;
            DBG_APP_INFO("BrightnessB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Brightness.SetVal = *(int16_t *)EP0RxBuffer;
                DBG_APP_INFO("Brightness Set Cur  = %d\n\r", Brightness.SetVal);
                Brightness.CurrentVal = Brightness.SetVal;
                /* SendCmd(ADDR_AE_COMPENSATION, Brightness.SetVal); */
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    memcpy(Ep0TxBuffer, (uint8_t *)(&wrVal), 2);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleBrightnessControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleContrastControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Contrast Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleContrastControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Contrast.CurrentVal;
            DBG_APP_INFO("Contrast Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Contrast.MinVal;
            DBG_APP_INFO("Contrast Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Contrast.MaxVal;
            DBG_APP_INFO("Contrast Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Contrast.Res;
            DBG_APP_INFO("Contrast Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Contrast.DefaultVal;
            DBG_APP_INFO("ContrastB Get Def  =%d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Contrast.SetVal = *(uint16_t *)EP0RxBuffer;
                DBG_APP_INFO("Contrast Set Cur  = %d\n\r", Contrast.SetVal);
                Contrast.CurrentVal = Contrast.SetVal;
                /* SendCmd(ADDR_GAMMA, Contrast.SetVal); */
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleContrastControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleGainControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Gain Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleGainControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Gain.CurrentVal;
            DBG_APP_INFO("Gain Get Cur  = %d\n\r", Gain.CurrentVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Gain.MinVal;
            DBG_APP_INFO("Gain Get Min  = %d\n\r", Gain.MinVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Gain.MaxVal;
            DBG_APP_INFO("Gain Get Max  = %d\n\r", Gain.MaxVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Gain.Res;
            DBG_APP_INFO("Gain Get Res  = %d\n\r", Gain.Res);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Gain.DefaultVal;
            DBG_APP_INFO("Gain Get Def  = %d\n\r", Gain.DefaultVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);

            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Gain.SetVal = *(uint16_t *)EP0RxBuffer;
                DBG_APP_INFO("Gain Set Cur  = %d\n\r", Gain.SetVal);
                Gain.CurrentVal = Gain.SetVal;
#if 0
                SendCmd(ADDR_MANUAL_GAIN, Gain.SetVal * 256);
                SendCmd(ADDR_GAIN_MODE, 0);
#endif
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleGainControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleHueControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Hue Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleHueControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    int16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Hue.CurrentVal;
            DBG_APP_INFO("Hue Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Hue.MinVal;
            DBG_APP_INFO("Hue Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Hue.MaxVal;
            DBG_APP_INFO("Hue Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Hue.Res;
            DBG_APP_INFO("Hue Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Hue.DefaultVal;
            DBG_APP_INFO("HueB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Hue.SetVal = *(int16_t *)EP0RxBuffer;
                DBG_APP_INFO("Hue Set Cur  = %d\n\r", Hue.SetVal);
                Hue.CurrentVal = Hue.SetVal;
                /* SendCmd(ADDR_COLOR_ADJ_HUE, Hue.CurrentVal / 150); */
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    memcpy(Ep0TxBuffer, (uint8_t *)&wrVal, 2);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleHueControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleSaturationControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Saturation Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleSaturationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Saturation.CurrentVal;
            DBG_APP_INFO("Saturation Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Saturation.MinVal;
            DBG_APP_INFO("Saturation Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Saturation.MaxVal;
            DBG_APP_INFO("Saturation Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Saturation.Res;
            DBG_APP_INFO("Saturation Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Saturation.DefaultVal;
            DBG_APP_INFO("SaturationB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Saturation.SetVal = *(uint16_t *)EP0RxBuffer;
                DBG_APP_INFO("Saturation Set Cur  = %d\n\r", Saturation.SetVal);
                Saturation.CurrentVal = Saturation.SetVal;
                /* SendCmd(ADDR_COLOR_SATURATION, Saturation.SetVal); */
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleSaturationControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleSharpnessControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Sharpness Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleSharpnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Sharpness.CurrentVal;
            DBG_APP_INFO("Sharpness Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Sharpness.MinVal;
            DBG_APP_INFO("Sharpness Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Sharpness.MaxVal;
            DBG_APP_INFO("Sharpness Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Sharpness.Res;
            DBG_APP_INFO("Sharpness Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Sharpness.DefaultVal;
            DBG_APP_INFO("SharpnessB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }
            else
            {
                Sharpness.SetVal = *((uint16_t *)EP0RxBuffer);
                DBG_APP_INFO("Sharpness Set Cur  = 0x%X\n\r", Sharpness.SetVal);

                if (Sharpness.CurrentVal != Sharpness.SetVal)
                {
                    /* TODO: Function to control focus. */
                    /* TODO: Verify the focus status. */
                    Sharpness.CurrentVal = Sharpness.SetVal;
                }
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleSharpnessControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleGammaControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Gamma Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleGammaControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = Gamma.CurrentVal;
            DBG_APP_INFO("Gamma Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = Gamma.MinVal;
            DBG_APP_INFO("Gamma Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = Gamma.MaxVal;
            DBG_APP_INFO("Gamma Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = Gamma.Res;
            DBG_APP_INFO("Gamma Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            DBG_APP_INFO("Gamma Get Info  = %d\n\r", VC_Get_Info);

            Ep0TxBuffer[0] = VC_Get_Info;
            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = Gamma.DefaultVal;
            DBG_APP_INFO("GammaB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                Gamma.SetVal = *((uint16_t *)EP0RxBuffer);
                DBG_APP_INFO("Gamma Set Cur  = 0x%X\n\r", Gamma.SetVal);

                if (Gamma.CurrentVal != Gamma.SetVal)
                {
                    Gamma.CurrentVal = Gamma.SetVal;
                    Gamma.SetVal = (Gamma.SetVal - 100) / 10;
                    /* TODO: Change the Gamma Control. */
                }
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleGammaControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandlePowerLineFreqControlReq(
*                                           cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Power Line Frequency (Flicker) Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandlePowerLineFreqControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint8_t wrVal = 0;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = PLFreq.CurrentVal;
            DBG_APP_INFO("PL Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = PLFreq.MinVal;
            DBG_APP_INFO("PL Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = PLFreq.MaxVal;
            DBG_APP_INFO("PL Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = PLFreq.Res;
            DBG_APP_INFO("PL Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("PL Get Info  = %d\n\r", VC_Get_Info);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = PLFreq.DefaultVal;
            DBG_APP_INFO("PL Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                PLFreq.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("PL Set Cur  = 0x%X\n\r", PLFreq.SetVal);

                if (PLFreq.CurrentVal != PLFreq.SetVal)
                {
                    PLFreq.CurrentVal = PLFreq.SetVal;

                    if (PLFreq.SetVal == 1)
                    {
                        /* TODO: Flicker cancel frequency is 50 Hz. */
                    }
                    else if (PLFreq.SetVal == 2)
                    {
                        /* TODO: Flicker cancel frequency is 60 Hz. */
                    }
                }
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = wrVal;
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandlePowerLineFreqControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleAWBTempControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Auto White-Balance Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleAWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint8_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = AWB_Temp.CurrentVal;
            DBG_APP_INFO("AWB Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = AWB_Temp.DefaultVal;
            DBG_APP_INFO("AWB Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                AWB_Temp.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("AWB Set Cur  = %d\n\r", AWB_Temp.SetVal);

                AWB_Temp.CurrentVal = AWB_Temp.SetVal;
                if (AWB_Temp.SetVal == 0x01)
                {
                    /* Set Auto-White Balance Mode */
                    /* SendCmd(ADDR_WB_MODE, 1); */
                }
                else
                {
                    /* Set Manual-White Balance Mode */
                    /* SendCmd(ADDR_WB_MODE, 2); */
                }
            }
            else
            {
                DBG_APP_ERR("Recv EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = wrVal;
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleAWBTempControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleAWBCompControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for Auto White-Balance Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleAWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint8_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = AWB_Comp.CurrentVal;
            DBG_APP_INFO("AWB Comp Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = AWB_Comp.MinVal;
            DBG_APP_INFO("AWB Comp Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = AWB_Comp.MaxVal;
            DBG_APP_INFO("AWB Comp Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = AWB_Comp.Res;
            DBG_APP_INFO("AWB Comp Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("AWB Comp Get Info  = %d\n\r", VC_Get_Info);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = AWB_Comp.DefaultVal;
            DBG_APP_INFO("AWB Comp Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                AWB_Comp.SetVal = EP0RxBuffer[0];
                DBG_APP_INFO("AWB Comp Set Cur  = 0x%X", AWB_Comp.SetVal);

                if (AWB_Comp.CurrentVal != AWB_Comp.SetVal)
                {
                    AWB_Comp.CurrentVal = AWB_Comp.SetVal;

                    if (AWB_Comp.SetVal == 0x01)
                    {
                        /* Set Auto-White Balance Mode */
                        /* TODO: AutoWhiteBalanceControl */
                    }
                    else
                    {
                        /* Set Manual-White Balance Mode */
                        /* TODO: Manual WhiteBalanceControl */
                    }
                }
            }
            else
            {
                DBG_APP_ERR("Error RecvEndp0Data\r\n");
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = wrVal;
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleAWBCompControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleMWBTempControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for White Balance Temperature Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleMWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint16_t wrVal;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = MWB_Temp.CurrentVal;
            DBG_APP_INFO("MWB Temp Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = MWB_Temp.MinVal;
            DBG_APP_INFO("MWB Temp Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = MWB_Temp.MaxVal;
            DBG_APP_INFO("MWB Temp Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = MWB_Temp.Res;
            DBG_APP_INFO("MWB Temp Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            return Cy_VideoControlGetInfo_0x03(pUsbdCtxt);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = MWB_Temp.DefaultVal;
            DBG_APP_INFO("MWBB Temp Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                MWB_Temp.SetVal = *(uint16_t *)EP0RxBuffer;
                DBG_APP_INFO("MWB Temp Set Cur  = %d\n\r", MWB_Temp.SetVal);
                MWB_Temp.CurrentVal = MWB_Temp.SetVal;
                /* SendCmd(ADDR_WB_PRESET_TEMP, MWB_Temp.SetVal); */
            }
            else
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_GET_LSB(wrVal);
    Ep0TxBuffer[1] = CY_USB_GET_MSB(wrVal);
    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 2);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO("Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleMWBTempControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlHandleMWBCompControlReq(cy_stc_usb_app_ctxt_t
*                                           *pAppCtxt, uint8_t bRequest,
*                                           uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for White Balance Component Control
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
uint32_t Cy_VideoControlHandleMWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength)
{
    /* uint16_t readCount = 0; */
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t status = CY_USBD_STATUS_FAILURE;
    uint32_t wrVal = 0;

    switch (bRequest)
    {
        case CY_USB_UVC_GET_CUR_REQ:
            wrVal = ((uint32_t)MWB_Comp_Red.CurrentVal << 8) |
                     ((uint32_t)MWB_Comp_Blue.CurrentVal << 0);
            DBG_APP_INFO("MWB Comp Get Cur  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MIN_REQ:
            wrVal = ((uint32_t)MWB_Comp_Red.MinVal << 8) |
                     ((uint32_t)MWB_Comp_Blue.MinVal << 0);
            DBG_APP_INFO("MWB Comp Get Min  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_MAX_REQ:
            wrVal = ((uint32_t)MWB_Comp_Red.MaxVal << 8) |
                     ((uint32_t)MWB_Comp_Blue.MaxVal << 0);
            DBG_APP_INFO("MWB Comp Get Max  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_RES_REQ:
            wrVal = ((uint32_t)MWB_Comp_Blue.Res << 0);
            DBG_APP_INFO("MWB Comp Get Res  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_GET_INFO_REQ:
            VC_Get_Info = 0x03;
            Ep0TxBuffer[0] = VC_Get_Info;
            DBG_APP_INFO("MWB Comp Get Info  = %d\n\r", VC_Get_Info);

            status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 1);
            if (status != CY_USBD_STATUS_SUCCESS)
            {
                DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
            }
            return (status != CY_USBD_STATUS_SUCCESS);

        case CY_USB_UVC_GET_DEF_REQ:
            wrVal = ((uint32_t)MWB_Comp_Red.DefaultVal << 8) |
                     ((uint32_t)MWB_Comp_Blue.DefaultVal << 0);
            DBG_APP_INFO("MWBB Comp Get Def  = %d\n\r", wrVal);
            break;

        case CY_USB_UVC_SET_CUR_REQ:
            status = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, EP0RxBuffer, wLength);
            if (status == CY_USBD_STATUS_SUCCESS)
            {
                MWB_Comp_Blue.SetVal = EP0RxBuffer[0];
                MWB_Comp_Red.SetVal  = EP0RxBuffer[1];
                DBG_APP_INFO("MWB Blue Set Cur  = 0x%X\n\r", MWB_Comp_Blue.SetVal);
                DBG_APP_INFO("MWB Red Cur  = 0x%X\n\r", MWB_Comp_Red.SetVal);

                if (MWB_Comp_Blue.CurrentVal != MWB_Comp_Blue.SetVal)
                {
                    MWB_Comp_Blue.CurrentVal = MWB_Comp_Blue.SetVal;
                    if ((MWB_Comp_Blue.SetVal >= 1) & (MWB_Comp_Blue.SetVal <= 255))
                    {
                        /* TODO: Adjust Manual-White Balance Comp - Blue. */
                    }
                }

                if (MWB_Comp_Red.CurrentVal != MWB_Comp_Red.SetVal)
                {
                    MWB_Comp_Red.CurrentVal = MWB_Comp_Red.SetVal;
                    if ((MWB_Comp_Red.SetVal >= 1) & (MWB_Comp_Red.SetVal <= 255))
                    {
                        /* TODO: Adjust Manual-White Balance Comp - Red. */
                    }
                }
            }
            else
            {
                DBG_APP_INFO("Recv EP0 Data failed = 0x%X\n\r", status);
            }

            return (status != CY_USBD_STATUS_SUCCESS);

        default:
            return Cy_VideoControlUnsupportedRequest(__func__, pUsbdCtxt, bRequest);
    }

    Ep0TxBuffer[0] = CY_USB_DWORD_GET_BYTE0(wrVal);
    Ep0TxBuffer[1] = CY_USB_DWORD_GET_BYTE1(wrVal);
    Ep0TxBuffer[2] = CY_USB_DWORD_GET_BYTE2(wrVal);
    Ep0TxBuffer[3] = CY_USB_DWORD_GET_BYTE3(wrVal);

    status = Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, Ep0TxBuffer, 4);
    if (status != CY_USBD_STATUS_SUCCESS)
    {
        DBG_APP_INFO(" Send EP0 Data failed = 0x%X\n\r", status);
    }

    return (status != CY_USBD_STATUS_SUCCESS);
} /* End of Cy_VideoControlHandleMWBCompControlReq() */

/******************************************************************************
* Function Name: Cy_VideoControlSony715BrightnessControl(uint8_t brightnessValue)
*******************************************************************************
* Summary:
* Function to control brighness of SonyIMX715 camera sensor
*
* \param pAppCtxt
* Application layer context pointer.
*
* \param bRequest
* bRequest Control request on endpoint 0
*
* \param wLength
* wLength Control request on endpoint 0
*
* \return
* Zero for success, Error code for non-success
*
******************************************************************************/
cy_en_scb_i2c_status_t Cy_VideoControlSony715BrightnessControl(uint8_t brightnessValue)
{
    cy_en_scb_i2c_status_t status = true;

    status = Cy_I2C_Write(FPGASLAVE_ADDR, DEVICE0_OFFSET + 0x18, brightnessValue,
        FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);

    return status;
} /* End of Cy_VideoControlSony715BrightnessControl() */

#endif
