/******************************************************************************
* \file cy_usb_cdc.h
* \version 1.0
*
* Defines for (Vcom) CDC class releated operation
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
#ifndef _CY_USB_CDC_H_
#define _CY_USB_CDC_H_

#include "cy_usb_app.h"
#include "cy_usb_uvc_device.h"

#if defined(__cplusplus)
}
#endif

/* CDC Class specific requests to be handled by this application. */
#define SET_LINE_CODING                    (0x20)
#define GET_LINE_CODING                    (0x21)
#define SET_CONTROL_LINE_STATE             (0x22)

#define FX3G2_TR_ASSIST                    (1)

#define CY_USB_VCOM_DEVICE_MSG_QUEUE_SIZE  (24)
#define CY_IFX_VCOM_MAX_QUEUE_SIZE         (2)
#define CY_USB_VCOM_DEVICE_MSG_SIZE        (sizeof (cy_stc_usbd_app_msg_t))



#define VCOM_RESPONSE_TIME                 (250)  /** UART RX buffer polling duration in milliseconds. */
#define VCOM_UART_MAX_RD_SIZE              (64u)   /** Maximum size of data read from UART RX FIFO in one burst. */
 
#define UART_WRITE                         (1)
#define UART_READ                          (2)
#define UART_WRITE_DMA_CHANNEL             (18)
#define UART_READ_DMA_CHANNEL              (19)


void Cy_USB_ConfigureVcomTask(cy_stc_usb_app_ctxt_t *pAppCtxt);
void Cy_USB_VcomDeviceTaskHandler (void *pTaskParam);
void Cy_USB_AppVcomTimerCallback (TimerHandle_t xTimer);
void Cy_USB_AppUartInitsDmaIntr(uint8_t uart_read_write,cy_israddress userIsr);
void WriteUart_ISR(void);
void ReadUart_ISR(void);
void OutEpDma_ISR (void);
void InEpDma_ISR (void);
void Cy_VcomDev_QueueUartRead (cy_stc_usb_app_ctxt_t *pAppCtxt,
                                     uint32_t dataLength);
void Cy_USB_AppVcomSendUartData (cy_stc_usb_app_ctxt_t *pAppCtxt,
                           uint8_t *dataBuf_p, uint32_t dataLength);
void Cy_USB_AppUARTWrite (uint8_t *pBuffer, uint16_t dataSize);
void Cy_USB_VcomDeviceSetLineCodingHandler (cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength);
void Cy_USB_UARTDmaWriteCompletion(void *pApp);
void Cy_USB_UARTDmaReadCompletion(void *pApp);
void Cy_USB_AppClearUartDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t uart_read_write);

void Cy_USB_VcomDeviceDmaReadCompletion (void *pApp);
void Cy_USB_VcomDeviceDmaWriteCompletion (void *pApp);
void Cy_USB_AppUartRead (uint8_t *pHbSramBuffer, uint16_t dataSize);

void
Cy_USB_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber,
                    uint8_t *pBuffer, uint16_t dataSize);
void Cy_USB_AppSetupCDCDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp,
                                 uint8_t *pEndpDscr);
void
Cy_AppUartHbwDmaCallback (struct cy_stc_hbdma_channel *pHandle,
                         cy_en_hbdma_cb_type_t type,
                         cy_stc_hbdma_buff_status_t* pbufStat, void *userCtx);
bool Cy_USB_HandleCDCRequests(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t bRequest,
                              uint16_t wLength);
void ConfigureSCB1Clock(void);
void SCB1Init(void);
void SCB1_ISR(void);

#if defined(__cplusplus)
}
#endif

#endif // _CY_USB_CDC_H_
