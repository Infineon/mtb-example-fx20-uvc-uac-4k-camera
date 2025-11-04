/***************************************************************************//**
* \file cy_usb_descriptors.c
* \version 1.0
*
* Defines the USB descriptors used in the FX10 USB Device Class implementation.
*
*******************************************************************************
* \copyright
* (c) (2021-2023), Cypress Semiconductor Corporation (an Infineon company) or
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
*******************************************************************************/
#ifndef _CY_USB_DESCRIPTOR_H_
#define _CY_USB_DESCRIPTOR_H_

#include "cy_usb_app.h"
#include "cy_usb_uvc_device.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define CY_USB_TTL_SIZE_CLASS_DSCR_VIDEO1   (0x0E + 0x1B + CY_USB_UYVY_STILL_DESCR_CNT + \
                                            (CY_USB_UYVY_TOTAL_NO_OF_RES * 0x1E))


#define CY_USB_TTL_SIZE_CLASS_DSCR_AUDIO    (0x08 + 0x09 + 0x09 + 0x0C + 0x09 + 0x09 + 0x09 + 0x07 + 0x0B + 0x09 + 0x06 + 0x07) //69
#define CY_USB_TTL_SIZE_CLASS_DSCR_CDC      (84)

#define CY_USB_TTL_SIZE_CONFIG_DSCR         (0x09 + 0x8A + CY_USB_TTL_SIZE_CLASS_DSCR_VIDEO1 + (AUDIO_IF_EN * CY_USB_TTL_SIZE_CLASS_DSCR_AUDIO ) + \
                                            (CY_VENDOR_IF_EN*0x09) + (CDC_INTERFACE_EN*CY_USB_TTL_SIZE_CLASS_DSCR_CDC))

#define CY_USB_TTL_SIZE_CONFIG_DSCR_L       (CY_USB_GET_LSB(CY_USB_TTL_SIZE_CONFIG_DSCR))
#define CY_USB_TTL_SIZE_CONFIG_DSCR_H       (CY_USB_GET_MSB(CY_USB_TTL_SIZE_CONFIG_DSCR))

/* Required interface number for USB classes */
#define NUM_INTERFACES_UVC                  (2)
#define NUM_INTERFACES_UAC                  (2)
#define NUM_INTERFACES_VENDOR               (1)
#define NUM_INTERFACES_CDC                  (2)

/* Calculate Number of interface in the descriptor */
#define USB3_NUM_INTERFACES                 (NUM_INTERFACES_UVC + \
                                            (AUDIO_IF_EN * NUM_INTERFACES_UAC) + \
                                            (CY_VENDOR_IF_EN * NUM_INTERFACES_VENDOR) + \
                                            (CDC_INTERFACE_EN * NUM_INTERFACES_CDC))

/* CDC interface should be kept afetr UVC UAC & VENDOR */
#define CDC_INTF_NUM                        (NUM_INTERFACES_UVC + (NUM_INTERFACES_UAC*AUDIO_IF_EN) + CY_VENDOR_IF_EN )

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_DESCRIPTOR_H_ */

/* End of File */
