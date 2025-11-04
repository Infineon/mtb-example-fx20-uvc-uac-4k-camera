/******************************************************************************
* \file cy_usb_videocontrol.h
* \version 1.0
*
* Defines the interfaces for video control application
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
#ifndef _CY_VIDEOCONTROL_H_
#define _CY_VIDEOCONTROL_H_

#include "cy_usb_app.h"
//#define Video_Control_Interface

#define VIDEO_CONTROL_INTERFACE                       (1)
#define ENABLE_PAN_TILT_CONTROL                       (0)

    #define CY_UVC_VC_CAMERA_TERMINAL_ID              (0x01)   /* Control Interface: Camera Terminal ID */
    #define CY_UVC_VC_PROCESSING_UNIT_ID              (0x02)   /* Control Interface: Processing Unit ID */

    /*****Camera Terminal Controls******/
    #define CT_CONTROL_UNDEFINED                      0x00
    #define CT_SCANNING_MODE_CONTROL                  0x01
    #define CT_AE_MODE_CONTROL                        0x02
    #define CT_AE_PRIORITY_CONTROL                    0x03
    #define CT_EXPOSURE_TIME_ABSOLUTE_CONTROL         0x04
    #define CT_EXPOSURE_TIME_RELATIVE_CONTROL         0x05
    #define CT_FOCUS_ABSOLUTE_CONTROL                 0x06
    #define CT_FOCUS_RELATIVE_CONTROL                 0x07
    #define CT_FOCUS_AUTO_CONTROL                     0x08
    #define CT_IRIS_ABSOLUTE_CONTROL                  0x09
    #define CT_IRIS_RELATIVE_CONTROL                  0x0A
    #define CT_ZOOM_ABSOLUTE_CONTROL                  0x0B
    #define CT_ZOOM_RELATIVE_CONTROL                  0x0C
    #define CT_PANTILT_ABSOLUTE_CONTROL               0x0D
    #define CT_PANTILT_RELATIVE_CONTROL               0x0E
    #define CT_ROLL_ABSOLUTE_CONTROL                  0x0F
    #define CT_ROLL_RELATIVE_CONTROL                  0x10
    #define CT_PRIVACY_CONTROL                        0x11

    /****Processing Unit Controls*********/
    #define PU_CONTROL_UNDEFINED                      0x00
    #define PU_BACKLIGHT_COMPENSATION_CONTROL         0x01
    #define PU_BRIGHTNESS_CONTROL                     0x02
    #define PU_CONTRAST_CONTROL                       0x03
    #define PU_GAIN_CONTROL                           0x04
    #define PU_POWER_LINE_FREQUENCY_CONTROL           0x05
    #define PU_HUE_CONTROL                            0x06
    #define PU_SATURATION_CONTROL                     0x07
    #define PU_SHARPNESS_CONTROL                      0x08
    #define PU_GAMMA_CONTROL                          0x09
    #define PU_WHITE_BALANCE_TEMPERATURE_CONTROL      0x0A
    #define PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL 0x0B
    #define PU_WHITE_BALANCE_COMPONENT_CONTROL        0x0C
    #define PU_WHITE_BALANCE_COMPONENT_AUTO_CONTROL   0x0D


/*******************************Video Control Functions***************/

/******************************************************************************
* Function Name: Cy_VideoControlHandleVCInterfaceRequest(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint16_t wLength)
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
uint32_t Cy_VideoControlHandleVCInterfaceRequest(cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleAEControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                   uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleAEControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleAEPriorityControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleAEPriorityControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleExpAbsControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for  Manual Exposure Control
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
uint32_t Cy_VideoControlHandleExpAbsControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleManualFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for  Manual Focus Control
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
uint32_t Cy_VideoControlHandleManualFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleAutoFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
*******************************************************************************
* Summary:
* Function to handle UVC Requests for  Auto Focus Mode Selection Control
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
uint32_t Cy_VideoControlHandleAutoFocusControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandlePanTiltControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandlePanTiltControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleRollControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleRollControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleBacklightCompensationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
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
uint32_t Cy_VideoControlHandleBacklightCompensationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);


/******************************************************************************
* Function Name: Cy_VideoControlHandleBrightnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleBrightnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);


/******************************************************************************
* Function Name: Cy_VideoControlHandleContrastControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleContrastControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleGainControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleGainControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);


/******************************************************************************
* Function Name: Cy_VideoControlHandleHueControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleHueControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);


/******************************************************************************
* Function Name: Cy_VideoControlHandleSaturationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleSaturationControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleSharpnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleSharpnessControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleGammaControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleGammaControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandlePowerLineFreqControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
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
uint32_t Cy_VideoControlHandlePowerLineFreqControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleAWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleAWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleAWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleAWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleMWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleMWBTempControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

/******************************************************************************
* Function Name: Cy_VideoControlHandleMWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt,
*                                           uint8_t bRequest, uint16_t wLength)
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
uint32_t Cy_VideoControlHandleMWBCompControlReq(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t bRequest, uint16_t wLength);

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
cy_en_scb_i2c_status_t Cy_VideoControlSony715BrightnessControl(uint8_t brightnessValue);

#endif /* _CY_VIDEOCONTROL_H_ */
