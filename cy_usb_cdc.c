/******************************************************************************
* \file cy_usb_cdc.c
* \version 1.0
*
* Implementation for (Vcom) CDC class releated operation
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

#include "cy_usb_cdc.h"
#if CDC_INTERFACE_EN
/* These two data length will assist timer functionality. */
static uint32_t uartRxFifoDataLenCurr = 0x00, uartRxFifoDataLenPrev = 0x00;
uint8_t USB3_DESC_ATTRIBUTES glVcomCfgData[7U] = {0x00,0x10,0x0E,0x00,0x02,0x00,0x08};
uint32_t cy_timer_loop;

static cy_stc_dma_descriptor_t gUartWriteDmaDesc1, gUartWriteDmaDesc2;
static cy_stc_dma_descriptor_t gUartReadlDmaDesc;
extern cy_stc_usb_app_ctxt_t appCtxt;


/* Default SCB-UART configuration. */
cy_stc_scb_uart_config_t glUartConfig = {
    .uartMode                   = CY_SCB_UART_STANDARD,
    .oversample                 = 8,
    .dataWidth                  = 8,
    .enableMsbFirst             = false,
    .stopBits                   = CY_SCB_UART_STOP_BITS_1,
    .parity                     = CY_SCB_UART_PARITY_NONE,
    .enableInputFilter          = false,
    .dropOnParityError          = false,
    .dropOnFrameError           = false,
    .enableMutliProcessorMode   = false,
    .receiverAddress            = 0,
    .receiverAddressMask        = 0,
    .acceptAddrInFifo           = false,
    .irdaInvertRx               = false,
    .irdaEnableLowPowerReceiver = false,
    .smartCardRetryOnNack       = false,
    .enableCts                  = false,
    .ctsPolarity                = CY_SCB_UART_ACTIVE_LOW,
    .rtsRxFifoLevel             = 0,
    .rtsPolarity                = CY_SCB_UART_ACTIVE_LOW,
    .breakWidth                 = 8,
    .rxFifoTriggerLevel         = VCOM_UART_MAX_RD_SIZE,
    .rxFifoIntEnableMask        = 0,
    .txFifoTriggerLevel         = VCOM_UART_MAX_RD_SIZE,
    .txFifoIntEnableMask        = 0
};


void Cy_USB_ConfigureVcomTask(cy_stc_usb_app_ctxt_t *pAppCtxt)
{

    BaseType_t status;
    /* create queue and register it to kernel. */
    pAppCtxt->vcomDeviceQueue = xQueueCreate(CY_USB_VCOM_DEVICE_MSG_QUEUE_SIZE,
                                             CY_USB_VCOM_DEVICE_MSG_SIZE);

    if(NULL == pAppCtxt->vcomDeviceQueue)
    {
        LOG_ERROR("VcomQueue failed\r\n");
        return;
    }

    DBG_APP_INFO("createdVcomQueue\r\n");
    vQueueAddToRegistry(pAppCtxt->vcomDeviceQueue, "VCOMDeviceMsgQueue");

    /* Create task and check status to confirm task created properly. */
    status = xTaskCreate(Cy_USB_VcomDeviceTaskHandler, "VCOMDeviceTask", 1024,
                         (void *)pAppCtxt, 5, &(pAppCtxt->vcomDevicetaskHandle));

    ASSERT_NON_BLOCK(pdPASS == status, status);

    pAppCtxt->vcomtimerExpiry = VCOM_RESPONSE_TIME;
    pAppCtxt->vcomTimerHandle = xTimerCreate("VcomTimer", pAppCtxt->vcomtimerExpiry, pdFALSE,
                                             (void *)pAppCtxt, Cy_USB_AppVcomTimerCallback);
}



void Cy_USB_VcomDeviceTaskHandler (void *pTaskParam)
{
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
#if (!FX3G2_TR_ASSIST)
    cy_stc_hbdma_buff_status_t buffStat;
#endif /* (!FX3G2_TR_ASSIST) */
    uint32_t intMask;

    DBG_APP_INFO("VCOMDeviceThreadCreated\r\n");

    Cy_USB_AppUartInitsDmaIntr(UART_WRITE,WriteUart_ISR);
    DBG_APP_INFO("Cy_USB_AppUartInitsDmaIntr DONE1\r\n");
    Cy_USB_AppUartInitsDmaIntr(UART_READ,ReadUart_ISR);
    DBG_APP_INFO("Cy_USB_AppUartInitsDmaIntr DONE2\r\n");
    /*Debug KEE*/
    // while(1)
    // {
    // vTaskDelay(10000);
    // }
    do {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 100 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->vcomDeviceQueue, &queueMsg, 20);
        if (xStatus != pdPASS) {
            continue;
        }

        switch (queueMsg.type) {
        case CY_USB_VCOM_DEVICE_MSG_START_DATA_XFER:
            /* Note: Message is sent only in case of USBHS. Not checking explicitly for speed here. */

            /* Enable DMA interrupts for the VCOM OUT and IN endpoints. */
            Cy_USB_AppInitDmaIntr(pAppCtxt->vcomOutEpNum,
                                  CY_USB_ENDP_DIR_OUT, OutEpDma_ISR);
            Cy_USB_AppInitDmaIntr(pAppCtxt->vcomInEpNum,
                                  CY_USB_ENDP_DIR_IN, InEpDma_ISR);

            /* Queue read on USB OUT endpoint for one MAX_PKT_SIZE worth of data. */
            pAppCtxt->usbOutPktLen = pAppCtxt->endpOutDma[pAppCtxt->vcomOutEpNum].maxPktSize;
            Cy_USB_AppQueueRead(pAppCtxt, pAppCtxt->vcomOutEpNum,
                                pAppCtxt->pUsbRxBuffer[pAppCtxt->nxtUsbRxBufIdx],
                                pAppCtxt->usbOutPktLen);

            /* Enable DataWire channel to read VCOM_UART_MAX_RD_SIZE bytes from the UART RX_FIFO. */
            Cy_VcomDev_QueueUartRead(pAppCtxt, VCOM_UART_MAX_RD_SIZE);
            break;

        case CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE:
            /* Note: Message is sent only in case of USBHS. Not checking explicitly for speed here. */

            intMask = Cy_SysLib_EnterCriticalSection();

            /* Increment the number of occupied data buffers and store
                 * the size of the data received.
                 */
            pAppCtxt->usbRxDataLen[pAppCtxt->nxtUsbRxBufIdx] = pAppCtxt->usbOutPktLen;
            pAppCtxt->nxtUsbRxBufIdx++;
            if (pAppCtxt->nxtUsbRxBufIdx >= VCOM_APP_BUFFER_CNT) {
                pAppCtxt->nxtUsbRxBufIdx = 0;
            }
            pAppCtxt->usbRxBusyBufCount++;

            /* If this is the first occupied buffer, we can queue UART write. */
            if (pAppCtxt->usbRxBusyBufCount == 1) {
                Cy_SysLib_ExitCriticalSection(intMask);

                /* Configure the DMA channel to write the received data to the SCB
                     * transmit FIFO. */
                Cy_USB_AppUARTWrite(pAppCtxt->pUsbRxBuffer[pAppCtxt->uartTxBufIndex],
                                    pAppCtxt->usbRxDataLen[pAppCtxt->uartTxBufIndex]);
            } else {
                Cy_SysLib_ExitCriticalSection(intMask);
            }

            /* If there is space, queue read on USB OUT endpoint for one MAX_PKT_SIZE worth of data. */
            if (pAppCtxt->usbRxBusyBufCount < VCOM_APP_BUFFER_CNT) {
                pAppCtxt->usbOutPktLen = pAppCtxt->endpOutDma[pAppCtxt->vcomOutEpNum].maxPktSize;
                Cy_USB_AppQueueRead(pAppCtxt, pAppCtxt->vcomOutEpNum,
                                    pAppCtxt->pUsbRxBuffer[pAppCtxt->nxtUsbRxBufIdx],
                                    pAppCtxt->usbOutPktLen);
            }
            break;

        case CY_USB_VCOM_DEVICE_MSG_SLP_OUT:
            /* Note: Message is sent only in case of USBHS. Not checking explicitly for speed here. */

            /* Queue fresh read to retrieve the correct amount of data from Ingress EPM. */
            pAppCtxt->usbOutPktLen = queueMsg.data[1];
            Cy_USB_AppQueueRead(pAppCtxt, pAppCtxt->vcomOutEpNum,
                                pAppCtxt->pUsbRxBuffer[pAppCtxt->nxtUsbRxBufIdx],
                                pAppCtxt->usbOutPktLen);

            /* Manually assert the trigger to the DMA channel. */
            Cy_TrigMux_SwTrigger(TRIG_IN_MUX_0_USBHSDEV_TR_OUT0 + pAppCtxt->vcomOutEpNum,
                                 CY_TRIGGER_TWO_CYCLES);
            break;

        case CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE:
            /* Note: Message is sent only in case of USBHS. Not checking explicitly for speed here. */

            /* Data has been consumed on USB side and buffer is free now. */
            if (pAppCtxt->uartRxFreeBufCount == 0) {
                /* Queue read from the UART RX FIFO into the next DMA buffer. */
                Cy_VcomDev_QueueUartRead(pAppCtxt, VCOM_UART_MAX_RD_SIZE);
            }
            pAppCtxt->uartRxFreeBufCount++;

            /* If we have at least one occupied buffer, queue the next write operation. */
            if (pAppCtxt->uartRxFreeBufCount < VCOM_APP_BUFFER_CNT) {
                Cy_USB_AppVcomSendUartData(pAppCtxt,
                                           pAppCtxt->pUartRxBuffer[pAppCtxt->nxtUsbTxBufIdx],
                                           pAppCtxt->uartRxDataLen[pAppCtxt->nxtUsbTxBufIdx]);
            }
            break;

        case CY_USB_UART_MSG_WRITE_COMPLETE:
            DBG_APP_TRACE("MEM_TO_UART_WRITE_COMPLETE\r\n");
#if (!FX3G2_TR_ASSIST)
            if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
                Cy_HBDma_Channel_DiscardBuffer(pAppCtxt->pUsbToUartChn, &buffStat);
            }
#endif /* (!FX3G2_TR_ASSIST) */

            intMask = Cy_SysLib_EnterCriticalSection();

            /* Move to the next TX buffer for the next transfer. */
            pAppCtxt->uartTxBufIndex++;
            if (pAppCtxt->uartTxBufIndex >= VCOM_APP_BUFFER_CNT) {
                pAppCtxt->uartTxBufIndex = 0;
            }

            /* Decrement the number of used DMA buffers on the USB RX side.
                 * If we still have an used buffer, we need to queue the
                 * corresponding data transfer on the UART interface.
                 */
            pAppCtxt->usbRxBusyBufCount--;
            if (pAppCtxt->usbRxBusyBufCount > 0) {
                Cy_SysLib_ExitCriticalSection(intMask);

                if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
#if FX3G2_TR_ASSIST
                    uint16_t xferCnt = pAppCtxt->usbRxDataLen[pAppCtxt->uartTxBufIndex];

                    MAIN_REG->TR_ASSIST_GR[0].TRA_CTRL[1] &=
                        (~MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_STREAM_EN_Msk);

                    if (xferCnt < 0x400U) {
                        /* If this is a short packet, configure the count accordingly. */
                        MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[1] =
                            MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_SHORT_CONFIG_Msk |
                            (xferCnt << MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_BYTE_COUNT_Pos);
                    } else {
                        MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[1] = 0;
                    }

                    MAIN_REG->TR_ASSIST_GR[0].TRA_CTRL[1] = pAppCtxt->pUsbToUartChn->nextProdDscr |
                                                            MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_STREAM_EN_Msk |
                                                            MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_PKT_MODE_Msk;
#endif /* FX3G2_TR_ASSIST */
                } else {
                    /* Queue read on USB OUT endpoint if it is not pending. */
                    if (pAppCtxt->usbRxBusyBufCount == (VCOM_APP_BUFFER_CNT - 1)) {
                        pAppCtxt->usbOutPktLen = pAppCtxt->endpOutDma[pAppCtxt->vcomOutEpNum].maxPktSize;
                        Cy_USB_AppQueueRead(pAppCtxt, pAppCtxt->vcomOutEpNum,
                                            pAppCtxt->pUsbRxBuffer[pAppCtxt->nxtUsbRxBufIdx],
                                            pAppCtxt->usbOutPktLen);
                    }
                }

                Cy_USB_AppUARTWrite(pAppCtxt->pUsbRxBuffer[pAppCtxt->uartTxBufIndex],
                                    pAppCtxt->usbRxDataLen[pAppCtxt->uartTxBufIndex]);
            } else {
                Cy_SysLib_ExitCriticalSection(intMask);
            }
            break;

        case CY_USB_UART_MSG_READ_COMPLETE:
            DBG_APP_TRACE("UART_MSG_READ_COMPLETE\r\n");
            if (pAppCtxt->blockUartReads) {
                DBG_APP_INFO("Block transfer on UART read complete\r\n");
                break;
            }
            xTimerReset(pAppCtxt->vcomTimerHandle, 0);

            /* FW based commit is not required in SS mode if FX3G2_TR_ASSIST is enabled. */
            if ((FX3G2_TR_ASSIST == 0) || (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)) {
                /* Send the data that has been received on the USB IN endpoint. */
                if (cy_timer_loop == 0x01){
                    Cy_USB_AppVcomSendUartData(pAppCtxt,
                                               pAppCtxt->pUartRxBuffer[pAppCtxt->uartRxBufIndex],
                                               uartRxFifoDataLenCurr);

                    /* Clear the data size tracking variables. */
                    cy_timer_loop = 0x00;
                    uartRxFifoDataLenCurr = 0x00;
                    uartRxFifoDataLenPrev = 0x00;
                }
                else{
                    Cy_USB_AppVcomSendUartData(pAppCtxt,
                                               pAppCtxt->pUartRxBuffer[pAppCtxt->uartRxBufIndex],
                                               VCOM_UART_MAX_RD_SIZE);
                }
            }

            intMask = Cy_SysLib_EnterCriticalSection();

            /* Move to the next RX buffer for the next read. */
            pAppCtxt->uartRxBufIndex++;
            if (pAppCtxt->uartRxBufIndex >= VCOM_APP_BUFFER_CNT) {
                pAppCtxt->uartRxBufIndex = 0;
            }

            pAppCtxt->uartRxFreeBufCount--;
            if (pAppCtxt->uartRxFreeBufCount != 0) {
                Cy_SysLib_ExitCriticalSection(intMask);

                /* Queue read from the UART RX FIFO into the next DMA buffer. */
                Cy_VcomDev_QueueUartRead(pAppCtxt, VCOM_UART_MAX_RD_SIZE);
            } else {
                Cy_SysLib_ExitCriticalSection(intMask);
            }
            break;

        case CY_USB_VCOM_SETLINECODING_CMD:
            Cy_USB_VcomDeviceSetLineCodingHandler(pAppCtxt, queueMsg.data[0]);
            break;

        default:
            DBG_APP_INFO("VcomMsgDefault\r\n");
            break;
        }/* end of switch() */

    } while (1);
}


bool Cy_USB_HandleCDCRequests(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t bRequest,
                              uint16_t wLength)
{
    bool isReqHandled = true;
    cy_en_usbd_ret_code_t retStatus;

    /* get_line_coding */
    if (bRequest == GET_LINE_CODING)
    {
        DBG_APP_INFO("GET_LINE_CODING\r\n");

        retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,(uint8_t *)glVcomCfgData,
                                              CY_USB_MIN(wLength,0x07));
        if (retStatus != CY_USBD_STATUS_SUCCESS)
        {
            isReqHandled = false;
        }
    }
    /* SET_CONTROL_LINE_STATE */
    else if (bRequest == SET_CONTROL_LINE_STATE)
    {
        DBG_APP_INFO("SET_CONTROL_LINE_STATE\r\n");
        if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED)
        {
            Cy_USBD_SendACkSetupDataStatusStage(pAppCtxt->pUsbdCtxt);
        }
        else
        {
            Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00,CY_USB_ENDP_DIR_IN, TRUE);
        }
    }
    else
    {
        DBG_APP_ERR("Error SetupCallback\r\n");
        isReqHandled = false;
    }
    return isReqHandled;
}



static void
Cy_USB_VcomDeviceSetLineCodingHandler (cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength)
{
    cy_stc_scb_uart_config_t uartCfg;
    cy_en_usbd_ret_code_t    retStatus = CY_USBD_STATUS_SUCCESS;
    uint16_t loopCnt = 250u;

    DBG_APP_INFO("SET_LINE_CODING\r\n");
    retStatus = Cy_USB_USBD_RecvEndp0Data(pAppCtxt->pUsbdCtxt, ((uint8_t *)glVcomCfgData), wLength);
    if (retStatus != CY_USBD_STATUS_SUCCESS)
    {
        Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
    }
    else
    {
        /* Wait until receive DMA transfer has been completed. */
        while ((!Cy_USBD_IsEp0ReceiveDone(pAppCtxt->pUsbdCtxt)) && (loopCnt--)) {
            Cy_SysLib_DelayUs(10);
        }

        if (!Cy_USBD_IsEp0ReceiveDone(pAppCtxt->pUsbdCtxt)) {
            Cy_USB_USBD_RetireRecvEndp0Data(pAppCtxt->pUsbdCtxt);
            Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);
            return;
        }

        /* TODO: The parameters passed from the host need to be applied on SCB1. */
        memset((uint8_t *)&uartCfg, 0, sizeof (cy_stc_scb_uart_config_t));
        uartCfg.uartMode            = CY_SCB_UART_STANDARD;
        uartCfg.oversample          = 15;
        uartCfg.stopBits            = CY_SCB_UART_STOP_BITS_1;
        uartCfg.parity              = CY_SCB_UART_PARITY_NONE;
        uartCfg.dataWidth           = 8;
        uartCfg.enableMsbFirst      = false;
        uartCfg.breakWidth          = 12;
        uartCfg.rxFifoTriggerLevel  = VCOM_UART_MAX_RD_SIZE;
        uartCfg.txFifoTriggerLevel  = VCOM_UART_MAX_RD_SIZE;
        uartCfg.rxFifoIntEnableMask = 0;

        Cy_SCB_UART_DeInit(SCB1);
        Cy_SCB_UART_Init(SCB1, &uartCfg, pAppCtxt->pUartCtxt);
        Cy_SCB_UART_Enable(SCB1);
        Cy_SCB_ClearTxFifo(SCB1);
        Cy_SCB_ClearRxFifo(SCB1);
        pAppCtxt->blockUartReads = false;

        /* Set the uart configuration */
        memcpy((uint8_t *)&glUartConfig,(uint8_t *)&uartCfg,sizeof (uartCfg));
    }
}



/*
 * Function: Cy_USB_AppVcomTimerCallback()
 * Description: DMA transfer from the UART RX FIFO will only happen when
 * the amount of data received crosses the threshold. To handle the cases
 * where lesser amount of data is received, a timer is being run. If the
 * RX FIFO has valid data which is below the threshold level, the UART DMA
 * will be re-configured to read it out.
 *
 * Parameter:
 *      xTimer: RTOS timer handle.
 * return: void
 */
void
Cy_USB_AppVcomTimerCallback (TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);

    (void)pAppCtxt;

    if (pAppCtxt->blockUartReads) {
        /* If the flag to block UART reads is set, do nothing. */
        return;
    }

    DBG_APP_TRACE("Timer Callback >>\r\n");

    if ((FX3G2_TR_ASSIST) && (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)) {
        /*
         * Just wait for two consicutive timer expire. Both the time if available
         * data is same then initiate transfer.
         * TBD: Do we need to check < 63 data? If control comes here means data
         * is  < 63 considering SBC interrupt has higher priority.
         */
        uartRxFifoDataLenPrev = uartRxFifoDataLenCurr;
        uartRxFifoDataLenCurr = Cy_SCB_GetNumInRxFifo(SCB1);

        DBG_APP_TRACE("uartRxFifoDataLenPrev =0x%x\r\n", uartRxFifoDataLenPrev);
        DBG_APP_TRACE("uartRxFifoDataLenCurr =0x%x\r\n", uartRxFifoDataLenCurr);

        if ((uartRxFifoDataLenCurr > 0x00) &&
            (uartRxFifoDataLenCurr == uartRxFifoDataLenPrev)) {

            DBG_APP_TRACE("Start RX_FIFO read len:%x\r\n", uartRxFifoDataLenCurr);

            /*
             * Just enable SHORT packet with HBWDMA before activating
             * CPUSS DMA. Later code wont get chance to enable short bit.
             */
            MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[0] =
                MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_SHORT_CONFIG_Msk |
                (uartRxFifoDataLenCurr <<
                 MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_BYTE_COUNT_Pos);

            /* Prepare the DataWire channel to read the available amount of data. */
            Cy_VcomDev_QueueUartRead(pAppCtxt, uartRxFifoDataLenCurr);

            /* SCB.RX_TRIGGER needs to be asserted high manually to get the
             * DMA transfer to start.
             */
            Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB1_RX_TO_PDMA0_TR_IN19,
                                 CY_TRIGGER_TWO_CYCLES);
            uartRxFifoDataLenPrev = 0x00;
            uartRxFifoDataLenCurr = 0x00;
        }
    } else {

        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_FS) {
            uartRxFifoDataLenPrev = uartRxFifoDataLenCurr;
            uartRxFifoDataLenCurr = Cy_SCB_GetNumInRxFifo(SCB1);

            if ((uartRxFifoDataLenCurr > 0x00) &&
                (uartRxFifoDataLenCurr == uartRxFifoDataLenPrev) && (cy_timer_loop == 0x00)) {
                DBG_APP_TRACE("Start RX_FIFO read len:%x\r\n", uartRxFifoDataLenCurr);

                /* Prepare the DataWire channel to read the available amount of data. */
                Cy_VcomDev_QueueUartRead(pAppCtxt, uartRxFifoDataLenCurr);

                /* SCB.RX_TRIGGER needs to be asserted high manually to get the
                 * DMA transfer to start.
                 */
                Cy_TrigMux_SwTrigger(TRIG_OUT_1TO1_0_SCB1_RX_TO_PDMA0_TR_IN19, CY_TRIGGER_TWO_CYCLES);

                /* Set flag to indicate timer based transfer has been initialized. */
                cy_timer_loop = 0x01;
            }
        }
    }

    xTimerReset(xTimer, 0);
}   /* end of function  */




/*
 * Function: Cy_USB_AppUartInitsDmaIntr()
 * Description: Function to register an ISR for the DMA channel associated
 *              with an UART Read/Write
 * Parameters:
 *      endpNumber: UART READ/WRITE
 *      userIsr: ISR function pointer. Can be NULL if interrupt is to be
 *               disabled.
 * return: void
 */
void
Cy_USB_AppUartInitsDmaIntr(uint8_t uart_read_write,cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;

    DBG_APP_INFO("Uart_Dma_Intr\r\n");

    intrCfg.intrPriority = 5;

    if (uart_read_write == UART_WRITE) {
        /* UART Write Interrupt Source */
        intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + UART_WRITE_DMA_CHANNEL);
    } else {
        /* UART Read Interrupt Source */
        intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + UART_READ_DMA_CHANNEL);
    }

    if (userIsr != NULL)  {
        /* If an ISR is provided, register it and enable the interrupt. */
        Cy_SysInt_Init(&intrCfg, userIsr);
        NVIC_EnableIRQ(intrCfg.intrSrc);
    } else {
        /* ISR is NULL. Disable the interrupt. */
        NVIC_DisableIRQ(intrCfg.intrSrc);
    }

    return;
} /* end of function. */



/*****************************************************************************
 * Function Name: WriteUart_ISR
 ******************************************************************************
 * Summary:
 *  Handler for DataWire transfer complete interrupts for transfers to the
 *  UART TX FIFO.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void WriteUart_ISR(void)
{
    DBG_APP_TRACE("WriteUart_ISR\r\n");
    Cy_USB_AppClearUartDmaInterrupt(&appCtxt, UART_WRITE);
    Cy_USB_UARTDmaWriteCompletion(&appCtxt);
}  /* end of function. */


/*****************************************************************************
 * Function Name: ReadUart_ISR
 ******************************************************************************
 * Summary:
 *  Handler for DataWire transfer complete interrupts for transfers which
 *  read from the UART RX FIFO.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void ReadUart_ISR(void)
{
    DBG_APP_TRACE("ReadUart_ISR >>\r\n");

    Cy_USB_AppClearUartDmaInterrupt(&appCtxt, UART_READ);
    Cy_USB_UARTDmaReadCompletion(&appCtxt);
    portYIELD_FROM_ISR(true);
}  /* end of function. */



/*****************************************************************************
 * Function Name: OutEpDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for DMA transfer completion on OUT endpoint.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void OutEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.vcomOutEpNum, CY_USB_ENDP_DIR_OUT);
    Cy_USB_VcomDeviceDmaReadCompletion(&appCtxt);
    portYIELD_FROM_ISR(true);
}  /* end of function. */


/*****************************************************************************
 * Function Name: InEpDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for DMA transfer completion on IN endpoint.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void InEpDma_ISR (void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.vcomInEpNum, CY_USB_ENDP_DIR_IN);
    Cy_USB_VcomDeviceDmaWriteCompletion(&appCtxt);
    portYIELD_FROM_ISR(true);
}  /* end of function. */



/*****************************************************************************
 * Function Name: SCB1_ISR
 *****************************************************************************
 * Summary
 *  Interrupt Service Routine for the SCB1 - UART block.
 *  The only interrupt enabled is the FIFO RX Trigger interrupt. The interrupt
 *  is marked when received and re-enabled later from task context.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 ****************************************************************************/
void SCB1_ISR(void)
{
    /******************************************************************
     * Disable the interrupt.
     * As CY_SCB_INTR_RX_FIFO_LEVEL interrupt source is not cleared when
     * the RX FIFO has more entries than the level, disable the interrupt
     * here and enable it after reading out data from the FIFO.
     ******************************************************************/
    /* TODO: Review the actual usage of the interrupt. */
    Cy_SCB_SetRxInterruptMask(SCB1, 0);
    Cy_SCB_ClearRxInterrupt(SCB1, 1);
}  /* end of function. */

/*
 * Function: Cy_VcomDev_QueueUartRead()
 * Description: Function which identifies the DMA buffer address where
 * the next data from UART RX_FIFO should be read and queues DataWire
 * transfer to read a specific amount of data into it.
 *
 * Parameter:
 *      pAppCtxt  : Pointer to the application context structure.
 *      dataLength: Length of data to be read from the SCB RX_FIFO.
 * return: void
 */
void Cy_VcomDev_QueueUartRead (cy_stc_usb_app_ctxt_t *pAppCtxt,
                                     uint32_t dataLength)
{
    uint8_t *rxBuf = pAppCtxt->pUartRxBuffer[pAppCtxt->uartRxBufIndex];

    /* Save the current data size. */
    pAppCtxt->uartRxDataLen[pAppCtxt->uartRxBufIndex] = dataLength;

    /* Configure the DataWire channel to read specified amount of data
     * into the next Rx buffer.
     */
    Cy_USB_AppUartRead(rxBuf, dataLength);
}





/*
 * Function: Cy_USB_AppUARTWrite()
 * Description: Function to write operation on an UART TX_FIFO.
 * Parameter: pBuffer, dataSize
 * return: void
 */
void
Cy_USB_AppUARTWrite (uint8_t *pBuffer, uint16_t dataSize)
{
    cy_stc_dma_descriptor_config_t desc_cfg;
    cy_stc_dma_channel_config_t    chan_cfg;
    cy_en_dma_status_t stat;

    /* If the DMA channel is already enabled, disable it. */
    Cy_DMA_Channel_Disable(DW0, UART_WRITE_DMA_CHANNEL);

    if (dataSize > VCOM_UART_MAX_RD_SIZE) {
        /* Data may not fit in the SCB TX FIFO. Use 2-D DMA
         * with wait for TRIGGER from SCB after each 64 bytes are
         * transferred. A second descriptor is used to send any
         * residual portion of data at the end.
         */
        if ((dataSize % VCOM_UART_MAX_RD_SIZE) != 0) {
            desc_cfg.retrigger          = CY_DMA_RETRIG_16CYC;
            desc_cfg.interruptType      = CY_DMA_DESCR_CHAIN;
            desc_cfg.triggerOutType     = CY_DMA_DESCR_CHAIN;
            desc_cfg.triggerInType      = CY_DMA_DESCR;
            desc_cfg.dataSize           = CY_DMA_BYTE;
            desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
            desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
            desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
            desc_cfg.srcAddress         = (void *)(pBuffer + (dataSize & (~(VCOM_UART_MAX_RD_SIZE - 1))));
            desc_cfg.dstAddress         = (void *)(&SCB1->TX_FIFO_WR);
            desc_cfg.srcXincrement      = 1;
            desc_cfg.dstXincrement      = 0;
            desc_cfg.xCount             = dataSize % VCOM_UART_MAX_RD_SIZE;
            desc_cfg.srcYincrement      = 0;
            desc_cfg.dstYincrement      = 0;
            desc_cfg.yCount             = 0;
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.nextDescriptor     = NULL;
            stat = Cy_DMA_Descriptor_Init (&gUartWriteDmaDesc2, &desc_cfg);
            CY_ASSERT(stat == CY_DMA_SUCCESS);

            /* Descriptor 1 needs to be linked to descriptor 2. */
            desc_cfg.channelState       = CY_DMA_CHANNEL_ENABLED;
            desc_cfg.nextDescriptor     = &gUartWriteDmaDesc2;
        } else {
            desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
            desc_cfg.nextDescriptor     = NULL;
        }

        desc_cfg.retrigger          = CY_DMA_RETRIG_16CYC;
        desc_cfg.interruptType      = CY_DMA_DESCR_CHAIN;
        desc_cfg.triggerOutType     = CY_DMA_DESCR_CHAIN;
        desc_cfg.triggerInType      = CY_DMA_X_LOOP;
        desc_cfg.dataSize           = CY_DMA_BYTE;
        desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
        desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
        desc_cfg.descriptorType     = CY_DMA_2D_TRANSFER;
        desc_cfg.srcAddress         = (void *)pBuffer;
        desc_cfg.dstAddress         = (void *)(&SCB1->TX_FIFO_WR);
        desc_cfg.srcXincrement      = 1;
        desc_cfg.dstXincrement      = 0;
        desc_cfg.xCount             = VCOM_UART_MAX_RD_SIZE;
        desc_cfg.srcYincrement      = VCOM_UART_MAX_RD_SIZE;
        desc_cfg.dstYincrement      = 0;
        desc_cfg.yCount             = (dataSize / VCOM_UART_MAX_RD_SIZE);
    } else {
        /* All the available data will fill in the SCB TX FIFO.
         * Use 1-D DMA to transfer the entire data in one burst.
         */
        desc_cfg.retrigger          = CY_DMA_RETRIG_4CYC;
        desc_cfg.interruptType      = CY_DMA_DESCR;
        desc_cfg.triggerOutType     = CY_DMA_DESCR;
        desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
        desc_cfg.triggerInType      = CY_DMA_DESCR;
        desc_cfg.dataSize           = CY_DMA_BYTE;
        desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
        desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
        desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
        desc_cfg.srcAddress         = (void *)pBuffer;
        desc_cfg.dstAddress         = (void *)(&SCB1->TX_FIFO_WR);
        desc_cfg.srcXincrement      = 1;
        desc_cfg.dstXincrement      = 0;
        desc_cfg.xCount             = dataSize;
        desc_cfg.srcYincrement      = 0;
        desc_cfg.dstYincrement      = 0;
        desc_cfg.yCount             = 1;
        desc_cfg.nextDescriptor     = NULL;
    }

    stat = Cy_DMA_Descriptor_Init (&gUartWriteDmaDesc1, &desc_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

    chan_cfg.descriptor  = &gUartWriteDmaDesc1;
    chan_cfg.preemptable = false;
    chan_cfg.priority    = 0;
    chan_cfg.enable      = false;
    chan_cfg.bufferable  = false;

    stat = Cy_DMA_Channel_Init (DW0, UART_WRITE_DMA_CHANNEL, &chan_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

    /* Make sure SLOW AHB read cache is evicted before new transfer is started. */
    MAIN_REG->CTRL |= MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk;
    while ((MAIN_REG->CTRL & MAIN_REG_CTRL_EVICT_SLOW_AHB_RD_CACHE_Msk) != 0);

    Cy_DMA_Channel_Enable(DW0, UART_WRITE_DMA_CHANNEL);
    Cy_DMA_Channel_SetInterruptMask(DW0,UART_WRITE_DMA_CHANNEL,CY_DMA_INTR_MASK);

    return;
}  /* end of function */



/*
 * Function: Cy_USB_AppVcomSendUartData
 * Description: Function to send the data received from UART to the USBSS
 * endpoint using High BandWidth DMA channel.
 * Parameter:
 *     pAppCtxt  : VCOM application context handle.
 *     dataBuf_p : Pointer to buffer containing data to be sent.
 *     dataLength: Length of the data in bytes.
 * Return: void
 */
void
Cy_USB_AppVcomSendUartData (cy_stc_usb_app_ctxt_t *pAppCtxt,
                           uint8_t *dataBuf_p, uint32_t dataLength)
{
    cy_stc_hbdma_buff_status_t dmaBufStat;
    cy_en_hbdma_mgr_status_t   hbdma_stat;

    if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
        dmaBufStat.pBuffer = dataBuf_p;
        dmaBufStat.size    = VCOM_UART_MAX_RD_SIZE;
        dmaBufStat.count   = dataLength;
        dmaBufStat.status  = 0;
        hbdma_stat = Cy_HBDma_Channel_CommitBuffer(pAppCtxt->pUartToUsbChn, &dmaBufStat);
        if (hbdma_stat != CY_HBDMA_MGR_SUCCESS) {
            DBG_APP_ERR("Cy_USB_AppVcomSendUartData: DMA error=%x\r\n", hbdma_stat);
        }
    } else {
        if (!pAppCtxt->usbInXferPending) {
            /* Send the data on the IN endpoint if it is currently idle. */
            Cy_USB_AppQueueWrite(pAppCtxt, pAppCtxt->vcomInEpNum, dataBuf_p, dataLength);

            if ((pAppCtxt->devSpeed == CY_USBD_USB_DEV_FS) && (dataLength == VCOM_UART_MAX_RD_SIZE)) {
                /* When sending a full packet (only applicable in FS case), send a ZLP after this. */
                Cy_USBD_SendEgressZLP(pAppCtxt->pUsbdCtxt, pAppCtxt->vcomInEpNum);
            }

            pAppCtxt->nxtUsbTxBufIdx++;
            if (pAppCtxt->nxtUsbTxBufIdx >= VCOM_APP_BUFFER_CNT) {
                pAppCtxt->nxtUsbTxBufIdx = 0;
            }
        }
    }
}



/*
 * Function: Cy_USB_UARTDmaWriteCompletion()
 * Description:  Handler for DMA transfer completion on UART TX_FIFO
 * Parameter: cy_stc_usb_app_ctxt_t
 * return: void
 */
void
Cy_USB_UARTDmaWriteCompletion(void *pApp)
{
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    xMsg.type = CY_USB_UART_MSG_WRITE_COMPLETE;
    status = xQueueSendFromISR(pAppCtxt->vcomDeviceQueue,  &(xMsg),
                               &(xHigherPriorityTaskWoken));
    (void)status;
    return;

}  /* end of function */




/*
 * Function: Cy_USB_AppClearUartDmaInterrupt()
 * Description: Function to clear the pending DMA interrupt associated with an
 *              UART.
 * Parameters:
 *      pAppCtxt: Pointer to USB application context structure.
 *      uart_read_write: UART Read/Write
 * return: void
 */
void
Cy_USB_AppClearUartDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,uint8_t uart_read_write)
{

    if ((pAppCtxt != NULL)) {
        if (uart_read_write == UART_WRITE) {
            Cy_DMA_Channel_ClearInterrupt(pAppCtxt->pCpuDw0Base, UART_WRITE_DMA_CHANNEL);
        } else if (uart_read_write == UART_READ) {
            Cy_DMA_Channel_ClearInterrupt(pAppCtxt->pCpuDw0Base, UART_READ_DMA_CHANNEL);
        } else {
            DBG_APP_ERR("ClearUartDmaInterrupt Err\r\n");
        }
    }

    return;
}  /* end of function. */




/*
 * Function: Cy_USB_UARTDmaReadCompletion()
 * Description:  Handler for DMA transfer completion on UART RX_FIFO
 * Parameter: cy_stc_usb_app_ctxt_t
 * return: None
 */
void
Cy_USB_UARTDmaReadCompletion(void *pApp)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    xMsg.type = CY_USB_UART_MSG_READ_COMPLETE;
    status = xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, &(xMsg),
                               &(xHigherPriorityTaskWoken));
    (void)status;
    return;
}  /* end of function */



/*
 * Function: Cy_USB_VcomDeviceDmaReadCompletion()
 * Description:  Handler for DMA transfer completion on OUT endpoint
 * Parameter: cy_stc_usb_app_ctxt_t
 * return: void
 */
void Cy_USB_VcomDeviceDmaReadCompletion (void *pApp)
{
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    xMsg.type = CY_USB_VCOM_DEVICE_MSG_READ_COMPLETE;
    status = xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, &(xMsg),
                               &(xHigherPriorityTaskWoken));
    (void)status;
    return;
}   /* end of function */

/*
 * Function: Cy_USB_VcomDeviceDmaWriteCompletion()
 * Description:  Handler for DMA transfer completion on IN endpoint
 * Parameter: cy_stc_usb_app_ctxt_t
 * return: void
 */
void Cy_USB_VcomDeviceDmaWriteCompletion (void *pApp)
{
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t xMsg;
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;
    pAppCtxt->usbInXferPending = false;

    xMsg = CY_USB_VCOM_DEVICE_MSG_WRITE_COMPLETE;
    status = xQueueSendFromISR(pAppCtxt->vcomDeviceQueue, &(xMsg),
                               &(xHigherPriorityTaskWoken));
    (void)status;
    return;
}   /* end of function */



/*
 * Function: Cy_USB_AppUartRead()
 * Description: This function activate CPUSS DMA to read from UART RX_FIFO and
 *              put dataSize length of data into SRAM buffer.
 * Parameter: pHbSramBuffer, dataSize
 * return: void
 */
void Cy_USB_AppUartRead (uint8_t *pHbSramBuffer, uint16_t dataSize)
{
    cy_stc_dma_descriptor_config_t desc_cfg;
    cy_stc_dma_channel_config_t    chan_cfg;
    cy_en_dma_status_t stat;

    /* If the DMA channel is already enabled, disable it. */
    Cy_DMA_Channel_Disable (DW0, UART_READ_DMA_CHANNEL);

    desc_cfg.retrigger          = CY_DMA_RETRIG_4CYC;
    desc_cfg.interruptType      = CY_DMA_DESCR;
    desc_cfg.triggerOutType     = CY_DMA_DESCR;
    desc_cfg.channelState       = CY_DMA_CHANNEL_DISABLED;
    desc_cfg.triggerInType      = CY_DMA_DESCR_CHAIN;
    desc_cfg.dataSize           = CY_DMA_BYTE;
    desc_cfg.srcTransferSize    = CY_DMA_TRANSFER_SIZE_WORD;
    desc_cfg.dstTransferSize    = CY_DMA_TRANSFER_SIZE_DATA;
    desc_cfg.descriptorType     = CY_DMA_1D_TRANSFER;
    desc_cfg.srcAddress         = (void *)(&SCB1->RX_FIFO_RD);
    desc_cfg.dstAddress         = (void *)pHbSramBuffer;
    desc_cfg.srcXincrement      = 0u;
    desc_cfg.dstXincrement      = 1u;
    desc_cfg.xCount             = dataSize;
    desc_cfg.srcYincrement      = 0;
    desc_cfg.dstYincrement      = 0;
    desc_cfg.yCount             = 1;
    desc_cfg.nextDescriptor     = NULL;

    stat = Cy_DMA_Descriptor_Init (&gUartReadlDmaDesc, &desc_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

    chan_cfg.descriptor  = &gUartReadlDmaDesc;
    chan_cfg.preemptable = false;
    chan_cfg.priority    = 0;
    chan_cfg.enable      = false;
    chan_cfg.bufferable  = false;

    stat = Cy_DMA_Channel_Init (DW0, UART_READ_DMA_CHANNEL, &chan_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

    Cy_DMA_Channel_SetInterruptMask(DW0,UART_READ_DMA_CHANNEL,CY_DMA_INTR_MASK);
    Cy_DMA_Channel_Enable (DW0, UART_READ_DMA_CHANNEL);

    return;
}  /* end of function */



/*
 * Function: Cy_USB_AppQueueRead()
 * Description: Function to queue read operation on an OUT endpoint.
 * Parameter: pAppCtxt, endpNumber, pBuffer, dataSize
 * return: void
 */
void
Cy_USB_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber,
                    uint8_t *pBuffer, uint16_t dataSize)
{
    cy_stc_app_endp_dma_set_t *dmaset_p;

    /* Null pointer checks. */
    if ((pAppCtxt == NULL) || (pAppCtxt->pUsbdCtxt == NULL) ||
        (pAppCtxt->pCpuDw0Base == NULL) || (pBuffer == NULL))
    {
        DBG_APP_ERR("QueueRead Err0\r\n");
        return;
    }

    /*
     * Verify that the selected endpoint is valid and
     * dataSize is non-zero.
     */
    dmaset_p = &(pAppCtxt->endpOutDma[endpNumber]);
    if ((dmaset_p->valid == 0) || (dataSize == 0))
    {
        DBG_APP_ERR("QueueRead Err1\r\n");
        return;
    }

    if (Cy_USBHS_App_QueueRead(dmaset_p, pBuffer, dataSize))
    {
        /* Set transfer size for the endpoint and clear NAK status to allow data to be received. */
        if (dataSize < dmaset_p->maxPktSize) {
            /* We are trying to read out data that has already been received. Force EP NAK. */
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, true);
        } else {
            /* Set the NAK bit so that the IP can see the bit transition from 1->0 after XFER_CNT is set. */
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, true);
            Cy_USBD_UpdateXferCount(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, dataSize);
            Cy_USB_USBD_EndpSetClearNakNrdy(pAppCtxt->pUsbdCtxt, endpNumber, CY_USB_ENDP_DIR_OUT, false);
        }
    }
} /* end of function */




void Cy_USB_AppSetupCDCDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp,
                                 uint8_t *pEndpDscr)
{

    if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
    {

        uint32_t endpNumber;
        cy_stc_app_endp_dma_set_t *pEndpDmaSet;
        DW_Type *pDW;
        uint16_t maxPktSize = 0x00;
        cy_en_usb_endp_dir_t endpDirection;
        bool stat;

        endpNumber = ((*(pEndpDscr+CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);
        Cy_USBD_GetEndpMaxPktSize(pEndpDscr, &maxPktSize);

        if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80) {
            endpDirection = CY_USB_ENDP_DIR_IN;
            pEndpDmaSet   = &(pUsbApp->endpInDma[endpNumber]);
            pDW           = pUsbApp->pCpuDw1Base;
        } else {
            endpDirection = CY_USB_ENDP_DIR_OUT;
            pEndpDmaSet   = &(pUsbApp->endpOutDma[endpNumber]);
            pDW           = pUsbApp->pCpuDw0Base;
        }

        DBG_APP_INFO("AppSetupEndpDmaParamsHs: endpNum:0x%x maxPktSize:0x%x "
                     "dir:0x%x \r\n", endpNumber, maxPktSize, endpDirection);

        stat = Cy_USBHS_App_EnableEpDmaSet(pEndpDmaSet, pDW, endpNumber, endpNumber, endpDirection, maxPktSize);
        DBG_APP_INFO("Enable EPDmaSet: endp=%x dir=%x stat=%x\r\n", endpNumber, endpDirection, stat);
    }


}



/*
 * Function: CY_AppUartHbwDmaCallback()
 * Description: HbwDMA callback whenever HBW-DMA transfer completes.
 * Parameter: cy_stc_hbdma_channel, cy_en_hbdma_cb_type_t,
 *            cy_stc_hbdma_buff_status_t, void* .
 * return: None.
 */
void
Cy_AppUartHbwDmaCallback (struct cy_stc_hbdma_channel *pHandle,
                         cy_en_hbdma_cb_type_t type,
                         cy_stc_hbdma_buff_status_t* pbufStat, void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    switch (type) {
    case CY_HBDMA_CB_CONS_EVENT:
        /*
             * This event means DATA is consumed and buffer is
             * available so next set of transfer can be initiated
             * from UART->HbwSRAM->USB
             */
        DBG_APP_INFO("CONS\r\n");

        /* Data has been consumed on USB side and buffer is free now. */
        if (pAppCtxt->uartRxFreeBufCount == 0) {
            /* Queue read from the UART RX FIFO into the next DMA buffer. */
            Cy_VcomDev_QueueUartRead(pAppCtxt, VCOM_UART_MAX_RD_SIZE);
        }
        pAppCtxt->uartRxFreeBufCount++;

#if FX3G2_TR_ASSIST
        /*
             * CONS_EVENT can come for SHORT as well as for FULL packet
             * so always reset short related settings in TR_ASSIST logic.
             */
        MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[0] &=
            (~MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_SHORT_CONFIG_Msk);
        MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[0] &=
            (~MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_BYTE_COUNT_Msk);
#endif /* FX3G2_TR_ASSIST */
        break;

    case CY_HBDMA_CB_PROD_EVENT:
        DBG_APP_INFO("PROD\r\n");

        /* Increment the number of occupied data buffers and store
             * the size of the data received.
             */
        pAppCtxt->usbRxDataLen[pAppCtxt->nxtUsbRxBufIdx] = pbufStat->count;
        pAppCtxt->nxtUsbRxBufIdx++;
        if (pAppCtxt->nxtUsbRxBufIdx >= VCOM_APP_BUFFER_CNT) {
            pAppCtxt->nxtUsbRxBufIdx = 0;
        }
        pAppCtxt->usbRxBusyBufCount++;

        /* If this is the first occupied buffer, we can queue UART write. */
        if (pAppCtxt->usbRxBusyBufCount == 1) {
#if FX3G2_TR_ASSIST
            MAIN_REG->TR_ASSIST_GR[0].TRA_CTRL[1] &=
                (~MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_STREAM_EN_Msk);

            if (pbufStat->count < 0x400U) {
                /* If this is a short packet, configure the count accordingly. */
                MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[1] =
                    MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_SHORT_CONFIG_Msk |
                    (pbufStat->count << MAIN_REG_TR_ASSIST_GR_TRA_SLP_CTRL_TRA_BYTE_COUNT_Pos);
            } else {
                MAIN_REG->TR_ASSIST_GR[0].TRA_SLP_CTRL[1] = 0;
            }

            MAIN_REG->TR_ASSIST_GR[0].TRA_CTRL[1] = pHandle->nextProdDscr |
                                                    MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_STREAM_EN_Msk |
                                                    MAIN_REG_TR_ASSIST_GR_TRA_CTRL_TRA_PKT_MODE_Msk;
#endif /* FX3G2_TR_ASSIST */

            /* Configure the DMA channel to write the received data to the SCB
                 * transmit FIFO. */
            Cy_USB_AppUARTWrite((uint8_t *)pbufStat->pBuffer, pbufStat->count);
        }
        break;

    default:
        break;
    }
    return;
}

/*****************************************************************************
 * Function Name: ConfigureSCB1Clock
 *****************************************************************************
 * Summary
 *  Configure the clock applied to SCB1.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 ****************************************************************************/
void ConfigureSCB1Clock()
{
    if (Cy_SysClk_ClkPeriGetFrequency() == 75000000UL) {
        /* Use PERI 16-bit divider #3 to divide 75 MHz HFCLK by 9 to get 8.333 MHz. */
        Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_16_BIT, 3, 500);
    } else {
        /* Use PERI 16-bit divider #3 to divide 50 MHz HFCLK by 6 to get 8.333 MHz. */
        Cy_SysClk_PeriphSetDivider (CY_SYSCLK_DIV_16_BIT, 3, 4);
    }

    Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 3);
    Cy_SysLib_DelayUs(10);

    /* Connect the PERI clock to the SCB input. */
    Cy_SysClk_PeriphAssignDivider(PCLK_SCB1_CLOCK, CY_SYSCLK_DIV_16_BIT, 3);
}  /* end of function. */


/*****************************************************************************
 * Function Name: SCB1Init
 *****************************************************************************
 * Summary
 *  Initialize SCB1 as UART.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 ****************************************************************************/
void SCB1Init(void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_scb_uart_config_t uartCfg;
    cy_stc_sysint_t intrCfg;

    /* Configure the clock input to the SCB block based on default
     * UART baud rate settings.
     */
    ConfigureSCB1Clock();

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure SCB1.UART_RX (P8.0) pin as input. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = P8_0_SCB1_UART_RX;
    Cy_GPIO_Pin_Init(P8_0_PORT, P8_0_PIN, &pinCfg);

    /* Configure the SCB1.UART_TX (P8.1) pin as strong drive output. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom = P8_1_SCB1_UART_TX;
    Cy_GPIO_Pin_Init(P8_1_PORT, P8_1_PIN, &pinCfg);

    memset((void *)&uartCfg, 0, sizeof(uartCfg));
    uartCfg.uartMode = CY_SCB_UART_STANDARD;

    uartCfg.oversample         = 15;
    uartCfg.dataWidth          = 8;
    uartCfg.enableMsbFirst     = false;
    uartCfg.stopBits           = CY_SCB_UART_STOP_BITS_1;
    uartCfg.parity             = CY_SCB_UART_PARITY_NONE;
    uartCfg.breakWidth         = 12;
    uartCfg.rxFifoTriggerLevel = VCOM_UART_MAX_RD_SIZE;
    uartCfg.txFifoTriggerLevel = VCOM_UART_MAX_RD_SIZE;

    /* Configure connection from SCB1 TX Trigger to DW0 Channel 18 Input Trigger. */
    Cy_TrigMux_Select(TRIG_OUT_1TO1_0_SCB1_TX_TO_PDMA0_TR_IN18, false, TRIGGER_TYPE_LEVEL);

    /* Configure connection from SCB1 RX Trigger to DW0 Channel 19 Input Trigger. */
    Cy_TrigMux_Select(TRIG_OUT_1TO1_0_SCB1_RX_TO_PDMA0_TR_IN19, false, TRIGGER_TYPE_LEVEL);

    /* Enable the RX FIFO trigger interrupt. */
    Cy_SCB_SetRxInterruptMask(SCB1, 1);

    /* Register the ISR for SCB1 and enable the interrupt vector.
     * TODO: Update code to work for CM0.
     */
    intrCfg.intrSrc = scb_1_interrupt_IRQn;
    intrCfg.intrPriority = 5;
    Cy_SysInt_Init(&intrCfg, SCB1_ISR);
    NVIC_EnableIRQ(scb_1_interrupt_IRQn);
} /* end of function. */

#endif
