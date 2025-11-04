/***************************************************************************//**
* \file cy_usb_uvc_device.h
* \version 1.0
*
* Defines the constants and messages used in the FX10 USB Video Class
* implementation.
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

#ifndef _CY_USB_UVC_DEVICE_H_
#define _CY_USB_UVC_DEVICE_H_

#include "cy_usb_app.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"

/* This header file comprises of the UVC application constants and
 * the video frame configurations */

/* Set UVC_PTZ_EN to '1' to enable PTZ */
#define UVC_PTZ_EN                               (0)

#define CY_USB_UVC_DEVICE_MSG_SETUP_DATA_XFER    (0x01)
#define CY_USB_UVC_DEVICE_MSG_START_DATA_XFER    (0x02)
#define CY_USB_UVC_DEVICE_MSG_STOP_DATA_XFER     (0x03)
#define CY_USB_UVC_DEVICE_MSG_READ_COMPLETE      (0x04)
#define CY_USB_UVC_DEVICE_MSG_WRITE_COMPLETE     (0x05)

#define CY_USB_UVC_DEVICE_MSG_ZLP_IN             (0x06)
#define CY_USB_UVC_DEVICE_MSG_SLP_IN             (0x07)
#define CY_USB_UVC_DEVICE_MSG_ZLP_OUT            (0x08)
#define CY_USB_UVC_DEVICE_MSG_SLP_OUT            (0x09)

#define CY_USB_UVC_DEVICE_MSG_L1_SLEEP           (0x0A)
#define CY_USB_UVC_DEVICE_MSG_L1_RESUME          (0x0B)

#define CY_USB_UVC_DEVICE_SET_FEATURE            (0x0C)
#define CY_USB_UVC_DEVICE_CLEAR_FEATURE          (0x0D)

#define CY_USB_UVC_VBUS_CHANGE_INTR              (0x0E)
#define CY_USB_UVC_VBUS_CHANGE_DEBOUNCED         (0x0F)
#define CY_USB_UVC_VIDEO_STREAMING_START         (0x10)
#define CY_USB_UVC_DEVICE_SET_CUR_RQT            (0x11)
#define CY_USB_UVC_DEVICE_VIDIO_CNTRL_RQT        (0x12)
#define CY_USB_UVC_FPGA_STATUS_POLL_EVENT        (0x13)
#define CY_USB_UVC_VIDEO_STREAM_START_EVENT      (0x14)
#define CY_USB_UVC_FLASH_PROGRAMMER_EVENT        (0x15)
#define CY_USB_UVC_VIDEO_STREAM_STOP_EVENT       (0x16)
#define CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER   (0x17)
#define CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE     (0x18)
#define CY_USB_VCOM_DEVICE_MSG_SLP_OUT           (0x19)
#define CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE    (0x1A)
#define CY_USB_UART_MSG_WRITE_COMPLETE           (0x1B)
#define CY_USB_UART_MSG_READ_COMPLETE            (0x1C)
#define CY_USB_VCOM_SETLINECODING_CMD            (0x1D)
#define CY_USB_FS_PORT_STATUS_EVENT              (0x1E)
#define USB_FS_DTE_PRESENT                       (0x01)
#define CY_USB_PDM_MSG_READ_COMPLETE             (0x20)
#define CY_USB_PDM_MSG_WRITE_COMPLETE            (0x21)
#define CY_USB_INTR_EVENT                        (0x22)
#define FX10_DMA_RESET_EVENT                     (0x31)     /* DMA Reset event */
#define FX10_PRINT_FRAME_INFO_EVENT              (0x32)     /* Print DMA buffer and FPS Event */
#define FX11_PRINT_FRAME_INFO_EVENT              (0x33)     /* Print DMA buffer and FPS Event */
#define FX10_PRINT_FPS_EVENT                     (0x34)     /* Print DMA buffer and FPS Event */
#define FX11_PRINT_FPS_EVENT                     (0x35)     /* Print DMA buffer and FPS Event */
#define FPGA_STATUS_PRINT_EVENT                  (0x36)     /* Print DMA buffer and FPS Event */
#define CY_USB_UVC_HDMI_ITE_EVENT                (0x40)
#define CY_USB_UVC_HDMI_POLL_EVENT               (0x41)


#define CY_USB_UVC_DEVICE_MSG_QUEUE_SIZE         (16)
#define CY_USB_UVC_DEVICE_MSG_SIZE               (sizeof (cy_stc_usbd_app_msg_t))

#define CY_USB_MAX_DATA_BUFFER_SIZE              (4096)
#define CY_IFX_UVC_MAX_QUEUE_SIZE                (1)

/* UVC descriptor types */
#define CY_USB_INTF_ASSN_DSCR_TYPE               (11)        /* Interface association descriptor type. */
#define CY_USB_EP_BULK_VIDEO_PKT_SIZE            (0x400)     /* UVC video streaming endpoint packet Size */
#define CY_USB_EP_BULK_VIDEO_PKTS_COUNT          (0x01)      /* UVC video streaming endpoint packet Count */
#define CY_USB_UVC_MAX_VID_FRAMES                (2)         /* Maximum number of video frames (4) */


#define CY_USB_BULK_BURST                        (8)         /* Burst size for SS operation only. */
#define CY_USB_UVC_STREAM_BUF_COUNT              (6)         /* UVC Buffer count */
#define CY_USB_UVC_MAX_HEADER                    (32)        /* Maximum number of header bytes in UVC */
#define CY_USB_UVC_HEADER_DEFAULT_BFH            (0x8C)      /* Default BFH(Bit Field Header) for the UVC Header */

#define CY_USB_UVC_MAX_PROBE_SETTING             (34)       /* Maximum number of bytes in Probe Control */
#define CY_USB_UVC_MAX_PROBE_SETTING_ALIGNED     (64)       /* Maximum number of bytes in Probe Control aligned to 4 byte */
 
#define CY_USB_UVC_HEADER_FRAME                  (0)                     /* Normal frame indication */
#define CY_USB_UVC_HEADER_EOF                    (uint8_t)(1 << 1)       /* End of frame indication */
#define CY_USB_UVC_HEADER_FRAME_ID               (uint8_t)(1 << 0)       /* Frame ID toggle bit */
 
#define CY_USB_UVC_INTERFACE_VC                  (0)                     /* Video Control interface id. */
#define CY_USB_UVC_INTERFACE_VS                  (1)                     /* Video Streaming interface id. */
 
#define CY_USB_UVC_SET_REQ_TYPE                  (uint8_t)(0x21)         /* UVC interface SET request type */
#define CY_USB_UVC_GET_REQ_TYPE                  (uint8_t)(0xA1)         /* UVC Interface GET request type */
#define CY_USB_UVC_GET_CUR_REQ                   (uint8_t)(0x81)         /* UVC GET_CUR request */
#define CY_USB_UVC_SET_CUR_REQ                   (uint8_t)(0x01)         /* UVC SET_CUR request */
#define CY_USB_UVC_GET_MIN_REQ                   (uint8_t)(0x82)         /* UVC GET_MIN request */
#define CY_USB_UVC_GET_MAX_REQ                   (uint8_t)(0x83)         /* UVC GET_MAX request */
#define CY_USB_UVC_GET_RES_REQ                   (uint8_t)(0x84)         /* UVC GET_RES Request */
#define CY_USB_UVC_GET_LEN_REQ                   (uint8_t)(0x85)         /* UVC GET_LEN Request */
#define CY_USB_UVC_GET_INFO_REQ                  (uint8_t)(0x86)         /* UVC GET_INFO Request */
#define CY_USB_UVC_GET_DEF_REQ                   (uint8_t)(0x87)         /* UVC GET_DEF request */
 
#define CY_USB_UVC_VS_PROBE_CONTROL              (0x0100)                /* Control selector for VS_PROBE_CONTROL. */
#define CY_USB_UVC_VS_COMMIT_CONTROL             (0x0200)                /* Control selector for VS_COMMIT_CONTROL. */
 
#define CY_USB_UVC_VC_RQT_ERROR_CODE_CONTROL     (0x0200)
#define CY_USB_UVC_RQT_STAT_INVALID_CTRL         (0x06)

#if AUDIO_IF_EN
#define STEREO_ENABLE  (1)      /* Enable two PDM channels for stereo input. */
#endif

/* DMA buffer size used for video streaming. Different sizes are used in different cases.
 *
 * 1. Firmware addition of header on top of video recieved from FPGA. In this case, it is preferred to
 *    keep the DMA buffer size (inclusive of the 32 byte UVC header) as a multiple of the USB endpoint
 *    maximum packet size of 1024 bytes. If the size chosen is more than 32 KB (32768 bytes) and a
 *    multiple of 1024 bytes, streaming does not work with the UVC drivers on Linux. When using Bulk
 *    endpoints for streaming, the Linux driver requires dwMaxPayloadTransferSize to either be:
 *    a. Less than or equal to 32768 bytes
 *    b. Be a multiple of 32768 bytes
 *    c. End with a short packet
 *    Considering these constraints, the buffer size is chosen.
 *
 * 2. Pre-added UVC header by FPGA: For the best streaming throughput, the buffer size should be a
 *    multiple of 1024 bytes. The buffer size configured on the FPGA side is a 16-bit quantity. Hence,
 *    the buffer size is chosen as 63 KB (64512 bytes) in this case.
 */
#if PARTIAL_FRAME_EN
#define CY_USB_UVC_STREAM_BUF_SIZE               (63*1024)
#define CY_USB_UVC_FPGA_BUF_SIZE                 (CY_USB_UVC_STREAM_BUF_SIZE-CY_USB_UVC_MAX_HEADER) /*provide DMA buffer size with devisible by 3 for RGB24*/
#else /* Full Buffer method */      /* (Need to make common) */
#define CY_USB_UVC_STREAM_BUF_SIZE               (61472)     /* UVC Buffer size  */
#endif /* PARTIAL_FRAME_EN */


#define UAC_EP_MAX_PKT_SIZE                        (192)           /* Max. packet size for audio streaming EP. */
#if STEREO_ENABLE
#define UAC_XFER_BUF_SIZE                          (UAC_EP_MAX_PKT_SIZE * 2)
#else
#define UAC_XFER_BUF_SIZE                          (UAC_EP_MAX_PKT_SIZE)
#endif /* STEREO_ENABLE */

/* This macro can be used to check if the DMA buffer size is divisible by 3.
 * In the case where an RGB24-bit pixel is used, the DMA buffer size should 
 * be divisible by 3 */
#define IS_DEVISIBLE_3(n) (((n&0xF)+((n>>4)&0xF)+((n>>8)&0xF)+((n>>12)&0xF)) % 3)

/* Get the LS byte from a 16-bit number */
#define CY_GET_LSB(w)        ((uint8_t)((w) & UINT8_MAX))

/* Get the MS byte from a 16-bit number */
#define CY_GET_MSB(w)        ((uint8_t)((w) >> 8))

#define CY_USB_FULL_BUFFER_NO_640_480            ((640*480*2 )/((CY_USB_UVC_STREAM_BUF_SIZE)-32)-1)
#define CY_USB_NO_BYTE_LAST_BUFFER_640_480       ((CY_USB_UVC_STREAM_BUF_SIZE)-32)

#define CY_USB_FULL_BUFFER_NO_1280_720           ((1280*720*2 )/((CY_USB_UVC_STREAM_BUF_SIZE)-32)-1)
#define CY_USB_NO_BYTE_LAST_BUFFER_1280_720      (CY_USB_UVC_STREAM_BUF_SIZE-32)

#define CY_USB_FULL_BUFFER_NO_1920_1080          ((1920*1080*2 )/((CY_USB_UVC_STREAM_BUF_SIZE)-32))
#define CY_USB_NO_BYTE_LAST_BUFFER_1920_1080     ((1920*1080*2 )%((CY_USB_UVC_STREAM_BUF_SIZE)-32))

#define CY_USB_FULL_BUFFER_NO_3840_2160          ((3840*2160*2 )/((CY_USB_UVC_STREAM_BUF_SIZE)-32)-1)
#define CY_USB_NO_BYTE_LAST_BUFFER_3840_2160     ((CY_USB_UVC_STREAM_BUF_SIZE)-32)

#define FRAME_SIZE_BYTE_4K_YUY2                  (3840*2160*2)
#define FRAME_SIZE_BYTE_1080P_YUY2               (1920*1080*2)
#define FRAME_SIZE_BYTE_720P_YUY2                (1280*720*2)
#define FRAME_SIZE_BYTE_480P_YUY2                (640*480*2)

#define FRAME_SIZE_BYTE_4K_ARGB32                (3840*2160*4)
#define FRAME_SIZE_BYTE_4K_RGB24                 (3840*2160*3)
#define FRAME_SIZE_BYTE_1080P_RGB24              (1920*1080*3)
#define FRAME_SIZE_BYTE_720P_RGB24               (1280*720*3)
#define FRAME_SIZE_BYTE_480P_RGB24               (640*480*3)

#define FRAME_SIZE_BYTE_4K_WITH_HEADER           (FRAME_SIZE_BYTE_4K_YUY2+CY_USB_UVC_MAX_HEADER)
#define FRAME_SIZE_BYTE_1080P_WITH_HEADER        (FRAME_SIZE_BYTE_1080P_YUY2+CY_USB_UVC_MAX_HEADER)
#define FRAME_SIZE_BYTE_720P_WITH_HEADER         (FRAME_SIZE_BYTE_720P_YUY2+CY_USB_UVC_MAX_HEADER)
#define FRAME_SIZE_BYTE_480P_WITH_HEADER         (FRAME_SIZE_BYTE_480P_YUY2+CY_USB_UVC_MAX_HEADER)


#define USB3_DESC_ATTRIBUTES __attribute__ ((section(".descSection"), used)) __attribute__ ((aligned (32)))
#define HBDMA_BUF_ATTRIBUTES __attribute__ ((section(".hbBufSection"), used)) __attribute__ ((aligned (32)))

/* Endpoint definitions for the UVC-UAC application */
#define UVC_STREAM_ENDPOINT                      (1u)          /* IN endpoint used for UVC video stream. */
#define UVC_CONTROL_ENDPOINT                     (2u)          /* IN endpoint used for UVC control interface. */
#define UAC_IN_ENDPOINT                          (3u)          /* IN endpoint used for UAC audio stream. */
/* Endpoint definitions for the USB-UART application */
#define CDC_BULK_IN_ENDPOINT                     (7u)       /* Bulk endpoint used for UART to USB transfers: 2-IN. */
#define CDC_BULK_OUT_ENDPOINT                    (8u)       /* Bulk endpoint used for USB to UART transfers: 1-OUT. */
#define CDC_INTR_ENDPOINT                        (9u)       /* Interrupt endpoint used for VCOM notifications: 3-IN. */

/* Note: Interface number start with 0 */
#define UVC_CONTROL_INTF_NUM                     (0x00)          /* Index of UVC control interface. */
#define UVC_STREAM_INTF_NUM                      (0x01)          /* Index of UVC streaming interface. */
#define UAC_CONTROL_INTF_NUM                     (0x02)          /* Index of UAC control interface. */
#define UAC_STREAM_INTF_NUM                      (0x03)          /* Index of UAC stream interface. */
#define VENDOR_INTF_NUM                          (2+(2*AUDIO_IF_EN)) 
/*CDC interface should be kept afetr UVC UAC & VENDOR -> Check in descriptor.c*/

/* Do not change the index number */
#define CY_USB_UVC_SS_4K_FRAME_INDEX             (1)
#define CY_USB_UVC_SS_1080P_FRAME_INDEX          (2)
#define CY_USB_UVC_SS_720P_FRAME_INDEX           (3)
#define CY_USB_UVC_SS_VGA_FRAME_INDEX            (4)

#define CY_USB_UYVY_STILL_DESCR_CNT              (0) //
#define CY_USB_UYVY_TOTAL_NO_OF_RES              (4) //4K, 1080P,720P,480P
#define CY_USB_RGB24_TOTAL_NO_OF_RES             (1) //4K
#define CY_USB_ARGB32_TOTAL_NO_OF_RES            (1) //4K

#define CY_USB_UVC_HS_VGA_FRAME_INDEX            (1)

#define CY_USB_UVC_PROBE_FORMAT_INDEX_ADDRESS    (2) /* Format Index start address in probe control structure */
#define CY_USB_UVC_PROBE_FORMAT_INDEX_SIZE       (1) /* Format Index size */
#define CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE     (6) /* Format Index, Frame Index and Frame interval */
#define CY_USB_UVC_VIDEO_FRAME_SIZE_BYTE         (4) /* Video frame size */
#define CY_USB_UVC_VIDEO_FRAME_START_ADDRESS     (18) /* Video frame size info start in probectrl*/
#define CY_USB_UVC_RX_PAYLOAD_SIZE_BYTE          (4) /* Video frame size */
#define CY_USB_UVC_RX_PAYLOAD_START_ADDRESS      (22) /* RX payload size info start in probectrl*/


typedef enum CyUsbAppVideoFormats
{
    UNDEFINED_FORMAT = 0,
    YUY2_FORMAT_INDEX   ,
    RGB24_FORMAT_INDEX  ,
    ARGB32_FORMAT_INDEX ,
    NV12_FORMAT_INDEX   ,
    ARAW_FORMAT_INDEX   ,
    MJPEG_FORMAT_INDEX
} CyUsbAppVideoFormats_t;

#endif /* _CY_USB_ECO_DEVICE_H_ */

/* End of File */
