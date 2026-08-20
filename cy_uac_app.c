/******************************************************************************
* \file cy_uac_app.c
* \version 1.0
*
* Implements the USB audio data handling in the FX10/FX20 integrated USB Video+Audio
* application.
*
*******************************************************************************
* \copyright
* (c) (2021-2026), Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cy_pdl.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_app.h"
#include "cy_debug.h"

#if AUDIO_IF_EN

/* Set to 1 to enable internal test data generation instead of connection to the PDM microphone. */
#define PDM_TEST_GEN_EN (0u)

const cy_stc_pdm_pcm_fir_coeff_t FIR0[] = {
    {-3, -9},{6, 49},{43, -105},{-238, 18},
    {581, 539},{-719, -1877},{-434, 4021},{8191,8191}
};

const cy_stc_pdm_pcm_fir_coeff_t FIR1[] = {
    {-1, -1},{4, 3},{-10, -6},{20, 11},
    {-38, -17},{65, 26},{-107, -36},{167, 48},
    {-255, -61},{383, 73},{-578, -84},{911, 93},
    {-1643, -99},{5126, 8191}
};

extern cy_stc_usb_app_ctxt_t appCtxt;
static cy_stc_dma_descriptor_t glPDMReadDmaDesc0;
#if STEREO_ENABLE
static cy_stc_dma_descriptor_t glPDMReadDmaDesc1;
#endif /* STEREO_ENABLE */

/******************************************************************************
 * Function Name: Cy_PDM_AppMxPdmInit
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
void Cy_PDM_AppMxPdmInit (void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_pdm_pcm_config_v2_t pdm_conf;
    cy_stc_pdm_pcm_channel_config_t chn_conf;

    /* Configure P4.1 and P4.2 as PDM signals. */
    memset ((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = P4_1_PDM0_PDM_CLK0;
    Cy_GPIO_Pin_Init(P4_1_PORT, P4_1_PIN, &pinCfg);

    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = P4_2_PDM0_PDM_DATA0;
    Cy_GPIO_Pin_Init(P4_2_PORT, P4_2_PIN, &pinCfg);

    memset ((uint8_t *)&pdm_conf, 0, sizeof(pdm_conf));

    /* Set divider for HFCLK[3] to 1 to make sure PDM block is getting input at 24 MHz. */
    Cy_SysClk_ClkHfSetDivider(3, CY_SYSCLK_CLKHF_NO_DIVIDE);

    /* clk_pdm runs at 1/8 the frequency of clk_if. */
    pdm_conf.clkDiv = 7;
    pdm_conf.clksel = CY_PDM_PCM_SEL_SRSS_CLOCK;
    pdm_conf.halverate = CY_PDM_PCM_RATE_FULL;
#if STEREO_ENABLE
    pdm_conf.route = 2;                 /* Stereo microphone: Single data bit used for both channels. */
#else
    pdm_conf.route = 0;
#endif /* STEREO_ENABLE */

    pdm_conf.fir0_coeff_user_value = 1;
    pdm_conf.fir1_coeff_user_value = 1;
    memcpy(pdm_conf.fir0_coeff, FIR0, sizeof(FIR0));
    memcpy(pdm_conf.fir1_coeff, FIR1, sizeof(FIR1));
    Cy_PDM_PCM_Init(PDM0, &pdm_conf);

    memset ((uint8_t *)&chn_conf, 0, sizeof(chn_conf));
    /* Sample the signal 2 clk_if cycles after rising edge of clk_pdm. */
    chn_conf.sampledelay = 1;
    chn_conf.wordSize = CY_PDM_PCM_WSIZE_16_BIT;
    chn_conf.signExtension = true;
    chn_conf.rxFifoTriggerLevel = 16;
    chn_conf.cic_decim_code = CY_PDM_PCM_CHAN_CIC_DECIM_32;
    chn_conf.fir0_decim_code = CY_PDM_PCM_CHAN_FIR0_DECIM_1;
#if PDM_TEST_GEN_EN
    chn_conf.fir0_scale = 16;
#else
    chn_conf.fir0_scale = 10;
#endif /* PDM_TEST_GEN_EN */
    chn_conf.fir0_enable = true;
    chn_conf.fir1_decim_code = CY_PDM_PCM_CHAN_FIR1_DECIM_2;
    chn_conf.fir1_scale = 13;
    chn_conf.dc_block_disable = false;
    chn_conf.dc_block_code = CY_PDM_PCM_CHAN_DCBLOCK_CODE_1;

#if PDM_TEST_GEN_EN
    cy_stc_test_config_t pdm_testCfg;
    memset((void *)&pdm_testCfg, 0, sizeof(pdm_testCfg));
    pdm_testCfg.drive_delay_hi = 0;             /* Drive high signal 1 clk_if cycle after rising edge of clk_pdm. */
    pdm_testCfg.drive_delay_lo = 4;             /* Drive low signal 5 clk_if cycles after rising edge of clk_pdm. */
    pdm_testCfg.mode_hi = 3;                    /* Choose sinusoidal pattern for high mode test signal. */
    pdm_testCfg.mode_lo = 3;                    /* Choose sinusoidal pattern for low mode test signal. */
    pdm_testCfg.audio_freq_div = 11;            /* Audio frequency = clk_pdm / 2*pi*(2^11) */
    pdm_testCfg.enable = 1;                     /* Enable test signal generation. */
#endif /* PDM_TEST_GEN_EN */

    /* Calling INIT enables the channel as well. */
    Cy_PDM_PCM_Channel_Init(PDM0, &chn_conf, 0);

    /* Configure connection from PDM0 RX Trigger to DW0 Channel 20 Input Trigger. */
    Cy_TrigMux_Select(TRIG_OUT_1TO1_3_PDM_RX0_TO_PDMA1_TR_IN20, false, TRIGGER_TYPE_LEVEL);

#if STEREO_ENABLE
    /* Sample the signal 6 clk_if cycles after rising edge of clk_pdm. */
    chn_conf.sampledelay = 5;
    Cy_PDM_PCM_Channel_Init(PDM0, &chn_conf, 1);

    /* Configure connection from PDM1 RX Trigger to DW0 Channel 21 Input Trigger. */
    Cy_TrigMux_Select(TRIG_OUT_1TO1_3_PDM_RX1_TO_PDMA1_TR_IN21, false, TRIGGER_TYPE_LEVEL);
#endif /* STEREO_ENABLE */

#if PDM_TEST_GEN_EN
    Cy_PDM_PCM_test_Init(PDM0, &pdm_conf, &pdm_testCfg);
#endif /* PDM_TEST_GEN_EN */
} /* End of Cy_PDM_AppMxPdmInit() */

/******************************************************************************
 * Function Name: Cy_PDM_AppMxPdmDeinit
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
void Cy_PDM_AppMxPdmDeinit (void)
{
    Cy_PDM_PCM_DeActivate_Channel(PDM0, 0);
    Cy_PDM_PCM_Channel_DeInit(PDM0, 0);
    Cy_PDM_PCM_Channel_Disable(PDM0, 0);

#if STEREO_ENABLE
    Cy_PDM_PCM_DeActivate_Channel(PDM0, 1);
    Cy_PDM_PCM_Channel_DeInit(PDM0, 1);
    Cy_PDM_PCM_Channel_Disable(PDM0, 1);
#endif /* STEREO_ENABLE */
} /* Cy_PDM_AppMxPdmDeinit() */

/******************************************************************************
 * Function Name: Cy_PDM_AppDmaReadCompletion
 ******************************************************************************
 * Summary
 *  Handle the completion of DMA transfer from PDM Receive FIFO into RAM
 *  buffers.
 *
 * Parameters:
 *  \param pAppCtxt
 * Pointer to application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_PDM_AppDmaReadCompletion (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    BaseType_t status;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    xMsg.type = CY_USB_PDM_MSG_READ_COMPLETE;
    status = xQueueSendFromISR(pAppCtxt->uacMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

    (void)status;
    (void)xHigherPriorityTaskWoken;
} /* End of Cy_PDM_AppDmaReadCompletion() */

/******************************************************************************
 * Function Name: Cy_PDM_CH1RX_ISR
 ******************************************************************************
 * Summary
 *  Interrupt service routine for the DataWire channel reading from PDM
 *  Channel-1 RX FIFO.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_CH1RX_ISR(void)
{
    /* Clear the interrupt. */
    Cy_DMA_Channel_ClearInterrupt(appCtxt.pCpuDw1Base, 21u);

    /* Check if the transfer on the Channel-0 has also been completed. Notify
     * application thread if all transfers are complete.
     */
    appCtxt.pdmPendingDmaFlag &= 0x01;
    if (appCtxt.pdmPendingDmaFlag == 0) {
        Cy_PDM_AppDmaReadCompletion(&appCtxt);
        portYIELD_FROM_ISR(true);
    }
} /* End of Cy_PDM_CH1RX_ISR() */

/******************************************************************************
 * Function Name: Cy_PDM_CH0RX_ISR
 ******************************************************************************
 * Summary
 *  Interrupt service routine for the DataWire channel reading from PDM
 *  Channel-0 RX FIFO.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_CH0RX_ISR (void)
{
    /* Clear the interrupt. */
    Cy_DMA_Channel_ClearInterrupt(appCtxt.pCpuDw1Base, 20u);

    /* Check if the transfer on the Channel-1 has also been completed. Notify
     * application thread if all transfers are complete.
     */
    appCtxt.pdmPendingDmaFlag &= 0x02;
    if (appCtxt.pdmPendingDmaFlag == 0) {
        Cy_PDM_AppDmaReadCompletion(&appCtxt);
        portYIELD_FROM_ISR(true);
    }
} /* End of Cy_PDM_CH0RX_ISR() */

/******************************************************************************
 * Function Name: Cy_PDM_InEpDma_ISR
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
void Cy_PDM_InEpDma_ISR (void)
{
    Cy_HBDma_Mgr_HandleDW1Interrupt(appCtxt.pHbDmaMgrCtxt);
    portYIELD_FROM_ISR(true);
} /* End of Cy_PDM_InEpDma_ISR() */

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
void Cy_UAC_AppInitDmaIntr (bool enable)
{
    cy_stc_sysint_t intrCfg;

    if (enable)
    {
        DBG_APP_INFO("Registering PDM DW ISRs\r\n");

        /* Interrupt for DataWire channel reading from PDM RX0 FIFO. */
        intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw1_20_IRQn);
        intrCfg.intrPriority = 5;

        Cy_SysInt_Init(&intrCfg, Cy_PDM_CH0RX_ISR);
        NVIC_EnableIRQ(intrCfg.intrSrc);

#if STEREO_ENABLE
        /* Interrupt for DataWire channel reading from PDM RX1 FIFO. */
        intrCfg.intrSrc = (IRQn_Type)(cpuss_interrupts_dw1_21_IRQn);
        intrCfg.intrPriority = 5;

        Cy_SysInt_Init(&intrCfg, Cy_PDM_CH1RX_ISR);
        NVIC_EnableIRQ(intrCfg.intrSrc);
#endif /* STEREO_ENABLE */
    }
    else
    {
        DBG_APP_INFO("Disabling PDM DW ISRs\r\n");

        NVIC_DisableIRQ(cpuss_interrupts_dw1_20_IRQn);
#if STEREO_ENABLE
        NVIC_DisableIRQ(cpuss_interrupts_dw1_21_IRQn);
#endif /* STEREO_ENABLE */
    }
} /* End of Cy_UAC_AppInitDmaIntr() */

/******************************************************************************
 * Function Name: Cy_PDM_AppChannelActivateTimerCb
 ******************************************************************************
 * Summary:
 *  Activate the PDM receive channels for data transfer. This is done with
 *  a delay from the PDM interface selection so that the host application
 *  starts reading data out before the PDM receive FIFOs start getting filled.
 *
 * Parameters:
 *  \param xTimer
 * Handle of timer trigger the PDM channel activation.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_AppChannelActivateTimerCb (TimerHandle_t xTimer)
{
    Cy_PDM_PCM_Activate_Channel(PDM0, 0);
#if STEREO_ENABLE
    Cy_PDM_PCM_Activate_Channel(PDM0, 1);
#endif /* STEREO_ENABLE */
} /* End of Cy_PDM_AppChannelActivateTimerCb() */

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
void Cy_PDM_AppQueueRead (cy_stc_usb_app_ctxt_t *pAppCtxt,
                          uint32_t dataLength)
{
    cy_stc_dma_descriptor_config_t desc_cfg;
    cy_stc_dma_channel_config_t chan_cfg;
    cy_en_dma_status_t stat;
    uint8_t *rxBuf = pAppCtxt->pPDMRxBuffer[pAppCtxt->pdmRxBufIndex];

    /* Save the current data size. */
#if STEREO_ENABLE
    pAppCtxt->pdmRxDataLen[pAppCtxt->pdmRxBufIndex] = dataLength << 1u;
#else
    pAppCtxt->pdmRxDataLen[pAppCtxt->pdmRxBufIndex] = dataLength;
#endif /* STEREO_ENABLE */

    /* If the DMA channel is already enabled, disable it. */
#if STEREO_ENABLE
    pAppCtxt->pdmPendingDmaFlag = 3;
    Cy_DMA_Channel_Disable(DW1, 21);
#else
    pAppCtxt->pdmPendingDmaFlag = 1;
#endif /* STEREO_ENABLE */
    Cy_DMA_Channel_Disable(DW1, 20);

    /*
     * Each PCM sample read from the FIFO is of 16 bits.
     * The PCM FIFO depth is only 64 entries. We read 32 bytes of data (16 samples) out from the FIFO
     * whenever, the FIFO has at least 16 samples of data in it. Multiple such reads are required to fill
     * one packet of UAC_EP_MAX_PKT_SIZE bytes which will be sent to the USB host as one packet.
     *
     * In stereo mode, we start reading from both RX FIFO in parallel with two different DataWire
     * channels.
     */
    desc_cfg.retrigger       = CY_DMA_RETRIG_16CYC;
    desc_cfg.interruptType   = CY_DMA_DESCR_CHAIN;
    desc_cfg.triggerOutType  = CY_DMA_DESCR_CHAIN;
    desc_cfg.triggerInType   = CY_DMA_X_LOOP;
    desc_cfg.channelState    = CY_DMA_CHANNEL_DISABLED;
    desc_cfg.dataSize        = CY_DMA_HALFWORD;
    desc_cfg.srcTransferSize = CY_DMA_TRANSFER_SIZE_WORD;
    desc_cfg.dstTransferSize = CY_DMA_TRANSFER_SIZE_DATA;
    desc_cfg.descriptorType  = CY_DMA_2D_TRANSFER;
    desc_cfg.srcAddress      = (void *)(&(PDM0->CH[0].RX_FIFO_RD));
    desc_cfg.dstAddress      = (void *)rxBuf;
    desc_cfg.srcXincrement   = 0u;
#if STEREO_ENABLE
    desc_cfg.dstXincrement   = 2u;
#else
    desc_cfg.dstXincrement   = 1u;
#endif /* STEREO_ENABLE */
    desc_cfg.xCount          = (32 / 2);
    desc_cfg.srcYincrement   = 0;
#if STEREO_ENABLE
    desc_cfg.dstYincrement   = 32;
#else
    desc_cfg.dstYincrement   = (32 / 2);
#endif /* STEREO_ENABLE */
    desc_cfg.yCount          = (dataLength / 32);
    desc_cfg.nextDescriptor  = NULL;

    stat = Cy_DMA_Descriptor_Init(&glPDMReadDmaDesc0, &desc_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

#if STEREO_ENABLE
    desc_cfg.srcAddress = (void *)(&(PDM0->CH[1].RX_FIFO_RD));
    desc_cfg.dstAddress = (void *)(rxBuf + 2);

    stat = Cy_DMA_Descriptor_Init(&glPDMReadDmaDesc1, &desc_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);
#endif /* STEREO_ENABLE */

    chan_cfg.descriptor  = &glPDMReadDmaDesc0;
    chan_cfg.preemptable = false;
    chan_cfg.priority    = 0;
    chan_cfg.enable      = false;
    chan_cfg.bufferable  = false;

    stat = Cy_DMA_Channel_Init(DW1, 20, &chan_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);

#if STEREO_ENABLE
    chan_cfg.descriptor  = &glPDMReadDmaDesc1;
    chan_cfg.priority    = 1;

    stat = Cy_DMA_Channel_Init(DW1, 21, &chan_cfg);
    CY_ASSERT(stat == CY_DMA_SUCCESS);
#endif /* STEREO_ENABLE */

    Cy_DMA_Channel_SetInterruptMask(DW1, 20, CY_DMA_INTR_MASK);
#if STEREO_ENABLE
    Cy_DMA_Channel_SetInterruptMask(DW1, 21, CY_DMA_INTR_MASK);
#endif /* STEREO_ENABLE */

    /* Enable the DataWire channels. */
    Cy_DMA_Channel_Enable(DW1, 20);
#if STEREO_ENABLE
    Cy_DMA_Channel_Enable(DW1, 21);
#endif /* STEREO_ENABLE */

    DBG_APP_TRACE("Cy_PDM_AppQueueRead status %x \n\r",stat);
} /* End of Cy_PDM_AppQueueRead() */

/******************************************************************************
 * Function Name: Cy_UAC_AppPDMSendData
 ******************************************************************************
 * Summary:
 * Function to send the data received from the PDM channels to the USB host
 * through the IN endpoint.
 *
 * Parameters:
 *  \param pAppCtxt
 * Pointer to application context structure.
 *
 *  \param dataBuf_p
 *  Pointer to data buffer.
 *
 *  \param dataLength
 * Length of data to be sent.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppPDMSendData (cy_stc_usb_app_ctxt_t *pAppCtxt,
                       uint8_t *dataBuf_p, uint32_t dataLength)
{
    cy_stc_hbdma_buff_status_t dmaBufStat;
    cy_en_hbdma_mgr_status_t   hbdma_stat;

    dmaBufStat.pBuffer = dataBuf_p;
    dmaBufStat.size    = dataLength;
    dmaBufStat.count   = dataLength;
    dmaBufStat.status  = 0;

    /* Commit the buffer to initiate transfer to USB host. */
    hbdma_stat = Cy_HBDma_Channel_CommitBuffer(pAppCtxt->pPDMToUsbChn, &dmaBufStat);
    if (hbdma_stat != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("Cy_USB_AppPDMSendPDMData: DMA error=%x\r\n", hbdma_stat);
    }
} /* End of Cy_UAC_AppPDMSendData() */

/******************************************************************************
 * Function Name: Cy_UAC_AppDeviceTaskHandler
 *******************************************************************************
 * Summary:
 *  Function that manages the PDM to USB data transfers for the USB Audio Class
 *  (UAC) interface.
 *
 * Parameters:
 *  \param pTaskParam
 * Opaque pointer which points to the application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppDeviceTaskHandler (void *pTaskParam)
{
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    uint32_t intMask;

    DBG_APP_INFO("PDM-UAC task started\r\n");

    /* Enable interrupts for the DW channels which read data from the PDM FIFOs. */
    Cy_UAC_AppInitDmaIntr(true);

    do
    {
        /* Wait for messages to be received from the ISRs. */
        xStatus = xQueueReceive(pAppCtxt->uacMsgQueue, &queueMsg, 100);
        if (xStatus != pdPASS)
        {
            continue;
        }

        switch (queueMsg.type)
        {
            case CY_USB_PDM_MSG_READ_COMPLETE:
                DBG_APP_TRACE("PDM_READ_COMPLETE\r\n");

                /* Commit the buffer to send the data on USB IN endpoint. */
                Cy_UAC_AppPDMSendData(pAppCtxt, pAppCtxt->pPDMRxBuffer[pAppCtxt->pdmRxBufIndex],
                        pAppCtxt->pdmRxDataLen[pAppCtxt->pdmRxBufIndex]);

                intMask = Cy_SysLib_EnterCriticalSection();
                /* Move to the next RX buffer for the next read. */
                pAppCtxt->pdmRxBufIndex++;
                if (pAppCtxt->pdmRxBufIndex >= PDM_APP_BUFFER_CNT) {
                    pAppCtxt->pdmRxBufIndex = 0;
                }

                pAppCtxt->pdmRxFreeBufCount--;
                if (pAppCtxt->pdmRxFreeBufCount != 0) {
                    Cy_SysLib_ExitCriticalSection(intMask);

                    /* Queue read from the UART RX FIFO into the next DMA buffer. */
                    Cy_PDM_AppQueueRead(pAppCtxt, UAC_EP_MAX_PKT_SIZE);
                } else {
                    Cy_SysLib_ExitCriticalSection(intMask);
                }
                break;

            default:
                DBG_APP_INFO("UACTask: Unknown message %x\r\n", queueMsg.type);
                break;
        } /* (queueMsg.type) */

    } while (1);
} /* End of Cy_UAC_AppPDMSendData() */

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
void Cy_UAC_AppSetIntfHandler (cy_stc_usb_app_ctxt_t *pAppCtxt,
                               uint8_t altSetting)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    uint32_t endpNumber;
    uint8_t *pIntfDscr, *pEndpDscr;
    int8_t numOfEndp;
    cy_en_usb_endp_dir_t endpDirection;

    DBG_APP_INFO("SetUACStreamingInterface start\r\n");
    if (altSetting == pAppCtxt->prevAltSetting)
    {
        DBG_APP_INFO("SameAltSetting\r\n");
        return;
    }

    /* New altSetting is different than previous one, so unconfigure previous. */
    DBG_APP_INFO("UnconfigPrevAltSet\r\n");
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, UAC_STREAM_INTF_NUM, pAppCtxt->prevAltSetting);
    if (pIntfDscr == NULL)
    {
        DBG_APP_INFO("pIntfDscrNull\r\n");
        return;
    }

    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        DBG_APP_INFO("SetIntf:prevNumEp 0\r\n");
    }
    else
    {
        /* Disable the DataWire channel and clear corresponding data structures. */
        Cy_DMA_Channel_Disable(pAppCtxt->pCpuDw1Base, 20);
#if STEREO_ENABLE
        Cy_DMA_Channel_Disable(pAppCtxt->pCpuDw1Base, 21);
#endif /* STEREO_ENABLE */

        pAppCtxt->pdmRxFreeBufCount = PDM_APP_BUFFER_CNT;
        pAppCtxt->pdmRxBufIndex     = 0;

        /* Free up the high bandwidth DMA channel if present. */
        if (pAppCtxt->pPDMToUsbChn != NULL) {
            Cy_HBDma_Channel_Disable(pAppCtxt->pPDMToUsbChn);
            Cy_HBDma_Channel_Destroy(pAppCtxt->pPDMToUsbChn);
            pAppCtxt->pPDMToUsbChn = NULL;
        }

        /* Run through all endpoints which were previously used and flush, reset, disable them. */
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80)
            {
                endpDirection = CY_USB_ENDP_DIR_IN;
            }
            else
            {
                endpDirection = CY_USB_ENDP_DIR_OUT;
            }
            endpNumber = (uint32_t)((*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);

            if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
                Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, true);
            }

            /* Flush, reset and then disable the endpoint. */
            Cy_USBD_FlushEndp(pUsbdCtxt, endpNumber, endpDirection);
            Cy_USBD_ResetEndp(pUsbdCtxt, endpNumber, endpDirection, false);
            Cy_USBD_EnableEndp(pUsbdCtxt, endpNumber, endpDirection, false);

            if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
                Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, false);
                Cy_SysLib_Delay(1);
            }

            numOfEndp--;

            /* Move to the next endpoint descriptor. */
            if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            } else {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }

        /* Deactivate the PDM channel and drain the FIFO. */
        Cy_PDM_PCM_DeActivate_Channel(PDM0, 0);
        while (Cy_PDM_PCM_Channel_GetNumInFifo(PDM0, 0) > 0) {
            (void)Cy_PDM_PCM_Channel_ReadFifo(PDM0, 0);
        }

#if STEREO_ENABLE
        Cy_PDM_PCM_DeActivate_Channel(PDM0, 1);
        while (Cy_PDM_PCM_Channel_GetNumInFifo(PDM0, 1) > 0) {
            (void)Cy_PDM_PCM_Channel_ReadFifo(PDM0, 1);
        }
#endif /* STEREO_ENABLE */

        DBG_APP_INFO("PDM DMAStop done\r\n");
    }

    pAppCtxt->prevAltSetting = altSetting;

    /* Now take care of different config with new alt setting. */
    pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, UAC_STREAM_INTF_NUM, altSetting);
    if (pIntfDscr == NULL)
    {
        DBG_APP_INFO("pIntfDscrNull\r\n");
        return;
    }

    numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
    if (numOfEndp == 0x00)
    {
        DBG_APP_INFO("SetIntf:numEp 0\r\n");
    }
    else
    {
        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;

            /* Move to the next endpoint. */
            if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            } else {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }

        /* Register interrupt for the DW channel associated with the UAC IN endpoint. */
        Cy_USB_AppInitDmaIntr(UAC_IN_ENDPOINT, CY_USB_ENDP_DIR_IN, Cy_PDM_InEpDma_ISR);

        /* Queue the first read from the PDM receive FIFOs. */
        Cy_PDM_AppQueueRead(pAppCtxt, UAC_EP_MAX_PKT_SIZE);

        /* Start a timer which will activate the PDM receive channels after some time. */
        xTimerReset(pAppCtxt->pdmActivateTimer, 0);
    }

    DBG_APP_INFO("UAC SetIntf done\r\n");
} /* End of Cy_UAC_AppSetIntfHandler() */

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
void Cy_UAC_AppInit (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    BaseType_t status;

    if ((!pAppCtxt) || (pAppCtxt->firstInitDone)) {
        return;
    }

    pAppCtxt->pPDMToUsbChn = NULL;

    /* Create queue and register it to kernel. */
    pAppCtxt->uacMsgQueue = xQueueCreate(24, CY_USB_UVC_DEVICE_MSG_SIZE);
    if (pAppCtxt->uacMsgQueue == NULL)
    {
        DBG_APP_ERR("UAC MsgQueue created failed\r\n");
        return;
    }

    DBG_APP_INFO("Created PDM Queue\r\n");
    vQueueAddToRegistry(pAppCtxt->uacMsgQueue, "PDMDeviceMsgQueue");

    /* Create task and check status to confirm task created properly. */
    status = xTaskCreate(Cy_UAC_AppDeviceTaskHandler, "PDMTask", 1024,
            (void *)pAppCtxt, 6, &(pAppCtxt->uacAppTaskHandle));
    if (status != pdPASS)
    {
        DBG_APP_ERR("UAC TaskCreate failed\r\n");
        return;
    }

    pAppCtxt->pdmActivateTimer = xTimerCreate("PDMDelayTimer", 10, pdFALSE,
            (void *)pAppCtxt, Cy_PDM_AppChannelActivateTimerCb);
    DBG_APP_INFO("UAC AppInit done\r\n");
} /* End of Cy_UAC_AppInit() */

#endif /* AUDIO_IF_EN */

/*[]*/

