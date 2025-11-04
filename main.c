/******************************************************************************
* \file main.c
* \version 1.0
*
* Main source file of the FX20 Integrated Audio+Video class application.
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
******************************************************************************/

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usbss_cal_drv.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_usbd_version.h"
#include "cy_hbdma_version.h"
#include "cy_lvds.h"
#include "app_version.h"
#include "timers.h"
#include "cy_usb_i2c.h"
#include "cy_usb_qspi.h"
#include "cy_usbfs_cdc.h"
#include "cybsp.h"
#include "cy_fx_common.h"


#if LVCMOS_EN
#include "cy_gpif_header_lvcmos.h"
#else
#include "cy_gpif_header_lvds.h"
#endif /* LVCMOS_EN */

/* Select SCB interface used for UART based logging. */
#define LOGGING_SCB                    (SCB1)
#define LOGGING_SCB_IDX                (1)
#define DEBUG_LEVEL                    (3u)
#if USBFS_LOGS_ENABLE
/* Select SCB interface used for UART based Command sending to FPGA. */
#define FPGA_UART_SCB                  (SCB1)
#define FPGA_UART_SCB_IDX              (1)
#define SCB1_RXBUF_SIZE                (64)
#endif /* USBFS_LOGS_ENABLE */

#if PTM_ENABLE
/* RTOS SW timer specific stuff */
/* 100 ms */
#define AUTO_RELOAD_TIMER_PERIOD (pdMS_TO_TICKS(2000UL))
TimerHandle_t xAutoReloadTimer;
BaseType_t xTimerStarted;
#endif /* PTM_ENABLE */

#if DEBUG_INFRA_EN
/* Debug log related initilization */

#define LOGBUF_SIZE (1024u)
uint8_t logBuff[LOGBUF_SIZE];
cy_stc_debug_config_t dbgCfg = {
    .pBuffer         = logBuff,
    .traceLvl        = DEBUG_LEVEL,
    .bufSize         = LOGBUF_SIZE,
#if USBFS_LOGS_ENABLE
    .dbgIntfce       = CY_DEBUG_INTFCE_USBFS_CDC,
#else
    .dbgIntfce       = CY_DEBUG_INTFCE_UART_SCB1,
#endif/* USBFS_LOGS_ENABLE */
    .printNow        = true
};

TaskHandle_t printLogTaskHandle;
#endif /* DEBUG_INFRA_EN */

/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t HBW_DrvCtxt;     /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t HBW_DscrList;  /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t HBW_BufMgr;      /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t HBW_MgrCtxt; /* High BandWidth DMA manager context. */
cy_stc_usb_usbd_ctxt_t usbdCtxt;
cy_stc_usb_app_ctxt_t appCtxt;
cy_stc_usbss_cal_ctxt_t ssCalCtxt;
cy_stc_usb_cal_ctxt_t hsCalCtxt;

/* SCB-UART context structure. */
static cy_stc_scb_uart_context_t gUartCtxt;

/* CPU DMA register pointers. */
DMAC_Type *pCpuDmacBase;
DW_Type   *pCpuDw0Base;
DW_Type   *pCpuDw1Base;

uint32_t hfclkFreq = BCLK__BUS_CLK__HZ;
static uint32_t g_UsbEvtLogBuf[512u];
static uint16_t gCurUsbEvtLogIndex = 0;

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

bool glIsLVDSPhyTrainingDone   = false;
bool glIsLVDSLink0TrainingDone = false;
bool glIsLVDSLink1TrainingDone = false;
uint8_t glPhyLinkTrainControl  = 0;

cy_stc_lvds_context_t lvdsContext;

volatile uint32_t           gpioPmodeStatus = 0;

/* Select SCB interface used for UART based Command sending to FPGA. */
#define FPGA_UART_SCB             (SCB1)
#define FPGA_UART_SCB_IDX         (1)
#define SCB1_RXBUF_SIZE           (64)
volatile uint8_t txDone;
uint8_t scb_rx_buffer[SCB1_RXBUF_SIZE];

void Cy_SysTickIntrWrapper (void)
{
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}

void vPortSetupTimerInterrupt(void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, Cy_SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(hfclkFreq / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

#if DEBUG_INFRA_EN
/******************************************************************************
* Function Name: Cy_DebugPrintTaskHandler(void *pTaskParam)
*******************************************************************************
* Summary:
* Debug print task handler
*
* Parameters:
* \param pTaskParam
* task param
*
* Return:
* void
******************************************************************************/
void Cy_DebugPrintTaskHandler(void *pTaskParam)
{
    while (1)
    {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
} /* End of Cy_DebugPrintTaskHandler() */
#endif /* DEBUG_INFRA_EN */

#if PTM_ENABLE
/******************************************************************************
* Function Name: ITP_EnableCB
*******************************************************************************
* Summary:
*  Callback function for software timer
*
* Parameters:
*  xTimer

* Return:
*  Does not return.
******************************************************************************/

void ITP_EnableCB(TimerHandle_t xTimer)
{
    USB32DEV->USB32DEV_PROT.PROT_INTR_MASK |= USB32DEV_PROT_INTR_ITP_EV_Msk;
}

/******************************************************************************
* Function Name: Software timer initialization
*******************************************************************************
* Summary:
*  Does some init for running rtos based auto reload software timer
*  which periodically enables ITP interrupt for LDM exchange mechanism
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
void Cy_SoftwareTimerInit(void)
{
    DBG_APP_INFO("Initializing software timer\r\n");
    xAutoReloadTimer = xTimerCreate("AutoReload",
                                    AUTO_RELOAD_TIMER_PERIOD,
                                    pdTRUE,
                                    0,
                                    ITP_EnableCB);

    xTimerStarted = xTimerStart(xAutoReloadTimer, 0);
} /* Cy_SoftwareTimerInit() */
#endif /* PTM_ENABLE */

/******************************************************************************
* Function Name: Cy_UVC_PrintLVDSConfigInfo(void) 
*******************************************************************************
* Summary:
* Prints LVDS configuration information on debug terminal Along with
* PHY and LINK Training pattern
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
void Cy_UVC_PrintLVDSConfigInfo(void)
{
    DBG_APP_INFO("LVDS PHY Train pattern = 0x%x\n\r",cy_lvds0_config.phyConfig->phyTrainingPattern);
    DBG_APP_INFO("LVDS LINK Train pattern = 0x%x\n\r",cy_lvds0_config.phyConfig->linkTrainingPattern);
    switch(cy_lvds0_config.phyConfig->interfaceClock)
    {
    case CY_LVDS_PHY_INTERFACE_CLK_625_MHZ:
        DBG_APP_INFO("LVDS Interface Clock Freq = 625 MHz\n\r");
        break;
    case CY_LVDS_PHY_INTERFACE_CLK_156_25_MHZ:
        DBG_APP_INFO("LVDS Interface Clock Freq = 156.25 MHz\n\r");
        break;
    case CY_LVDS_PHY_INTERFACE_CLK_148_5_MHZ:
        DBG_APP_INFO("LVDS Interface Clock Freq = 148.5 MHz\n\r");
        break;
    case CY_LVDS_PHY_INTERFACE_CLK_74_25_MHZ:
        DBG_APP_INFO("LVDS Interface Clock Freq = 74.25 MHz\n\r");
        break;
    default:
        DBG_APP_INFO("Standard LVDS Freq is not set");
        break;
    }  
} /* End of Cy_UVC_PrintLVDSConfigInfo() */

/******************************************************************************
 * Function Name: Cy_LVDS_GpifEventCb(uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent,
 *                                    void *cntxt)
 ******************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param gpifEvent
 * GPIF event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_GpifEventCb(uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent, void *cntxt)
{
     uint8_t SmState = 0;
    // CyU3vAppGpifIntr(&appCtxt);
    SmState = Cy_LVDS_GpifGetSMState(LVDSSS_LVDS,SmState);
    DBG_APP_INFO("Sm state = %d\n\r",SmState);
} /* End of Cy_LVDS_GpifEventCb() */

/******************************************************************************
 * Function Name: Cy_LVDS_PhyEventCb(uint8_t smNo, cy_en_lvds_phy_events_t phyEvent,
 *                                    void *cntxt)
 ******************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param phyEvent
 * LVDS/LVCMOS PHY event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_PhyEventCb(uint8_t smNo, cy_en_lvds_phy_events_t phyEvent, void *cntxt)
{
    cy_en_scb_i2c_status_t status;
    if(phyEvent == CY_LVDS_PHY_L1_EXIT)
    {
        if(smNo == 0)
        {
           DBG_APP_INFO("P0_L1_Exit\r\n");
        }
        else
        {
           DBG_APP_INFO("P1_L1_Exit\r\n");
        }
    }
    if(phyEvent == CY_LVDS_PHY_L1_ENTRY)
    {
        if(smNo == 0)
        {
            DBG_APP_INFO("P0_L1_Entry\r\n");
        }
        else
        {
            DBG_APP_INFO("P1_L1_Entry\r\n");
        }
    }
    if(phyEvent == CY_LVDS_PHY_L3_ENTRY)
    {
        if(smNo == 0)
        {
            DBG_APP_INFO("P0_L3_Entry\r\n");
        }
        else
        {
            DBG_APP_INFO("P1_L3_Entry\r\n");
        }
    }

    if (phyEvent == CY_LVDS_PHY_TRAINING_DONE)
    {
        DBG_APP_INFO("Port %d PHY Train Done\r\n",smNo);
        glIsLVDSPhyTrainingDone = true;
    }

    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET)
    {
        DBG_APP_INFO("Port %d Training Block Detected\r\n",smNo);
        if(smNo)
        {
            glIsLVDSLink1TrainingDone = true;
            SET_BIT (glPhyLinkTrainControl, P1_TRAINING_DONE);
            status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,glPhyLinkTrainControl,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        else
        {
            #if !PORT1_EN
            glIsLVDSLink0TrainingDone = true;
            SET_BIT (glPhyLinkTrainControl, P0_TRAINING_DONE);
            status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,glPhyLinkTrainControl,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
            #endif /* !PORT1_EN */
        }
    }
    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET_FAIL)
    {
        DBG_APP_ERR("Port %d Training Block Detect Failed\r\n",smNo);
    }
} /* End of Cy_LVDS_PhyEventCb() */

/******************************************************************************
 * Function Name: Cy_LVDS_LowPowerEventCb(cy_en_lvds_low_power_events_t lowPowerEvent, 
 *                                        void *cntxt)
 ******************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param lowPowerEvent
 * low power event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_LowPowerEventCb(cy_en_lvds_low_power_events_t lowPowerEvent, void *cntxt)
{
    if(lowPowerEvent == CY_LVDS_LOW_POWER_LNK0_L3_EXIT)
    {
        DBG_APP_INFO("P0_L3_Exit\r\n");
    }
    if(lowPowerEvent == CY_LVDS_LOW_POWER_LNK1_L3_EXIT)
    {
        DBG_APP_INFO("P1_L3_Exit\r\n");
    }
} /* End of Cy_LVDS_LowPowerEventCb() */

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifErrorCb(uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, 
 *                                    void *cntxt)
 *****************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param gpifError
 * GPIF error
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_GpifErrorCb(uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, void *cntxt)
{
    switch (gpifError)
    {
    case CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_DMA_ADDR_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR\n\r");
        break;

    case CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR\n\r");
        break;
    }
} /* End of Cy_LVDS_GpifErrorCb() */

/******************************************************************************
 * Function Name: Cy_LVDS_GpifThreadErrorCb(cy_en_lvds_gpif_thread_no_t ThNo, 
 *                       cy_en_lvds_gpif_thread_error_t ThError, void *cntxt)
 ******************************************************************************
 * Description: GPIF thread error callback function.
 *
 * Parameters:
 * \param ThNo
 * thread number
 * 
 * \param ThError
 * thread error
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_GpifThreadErrorCb (cy_en_lvds_gpif_thread_no_t ThNo,
                 cy_en_lvds_gpif_thread_error_t ThError, void *cntxt)
{
    switch (ThNo)
    {
    case CY_LVDS_GPIF_THREAD_0:
    case CY_LVDS_GPIF_THREAD_1:
    case CY_LVDS_GPIF_THREAD_2:
    case CY_LVDS_GPIF_THREAD_3:
        switch (ThError)
        {
        case CY_LVDS_GPIF_THREAD_DIR_ERROR:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_DIR_ERROR\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_WR_OVERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d__WR_OVERFLOW\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_RD_UNDERRUN:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_RD_UNDERRUN\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_SCK_ACTIVE:
            DBG_APP_INFO("CY_LVDS_GPIF_THREAD_%d_SCK_ACTIVE\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_ADAP_OVERFLOW\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_ADAP_UNDERFLOW\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_READ_FORCE_END:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_READ_FORCE_END\n\r", ThNo);
            break;

        case CY_LVDS_GPIF_THREAD_READ_BURST_ERR:
            DBG_APP_ERR("CY_LVDS_GPIF_THREAD_%d_READ_BURST_ERR\n\r", ThNo);
            break;
        }
        break;
    }
} /* End of Cy_LVDS_GpifThreadErrorCb() */

/******************************************************************************
 * Function Name: Cy_LVDS_GpifThreadEventCb(cy_en_lvds_gpif_thread_no_t ThNo, 
 *                        cy_en_lvds_gpif_thread_event_t ThEvent, void *cntxt)
 ******************************************************************************
 * Description: GPIF thread Event callback function.
 *
 * Parameters:
 * \param ThNo
 * thread number
 * 
 * \param ThEvent
 * thread event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_LVDS_GpifThreadEventCb (cy_en_lvds_gpif_thread_no_t ThNo,
                         cy_en_lvds_gpif_thread_event_t ThEvent, void *cntxt)
{
    switch(ThEvent)
    {
    case CY_LVDS_GPIF_HDR_FLG0_CLR:
        // DBG_APP_INFO("Flag-0 has been cleared\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG1_CLR:
        // DBG_APP_INFO("Flag-1 has been cleared\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG2_CLR:
        DBG_APP_INFO("Flag-2 has been cleared\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG3_CLR:
        DBG_APP_INFO("Flag-3 has been cleared\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG0_SET:
        // Cy_GPIO_Inv(P8_5_PORT, P8_5_PIN);
        // DBG_APP_INFO("Flag-0 has been set\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG1_SET:
        // DBG_APP_INFO("Flag-1 has been set\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG2_SET:
        DBG_APP_INFO("Flag-2 has been set\r\n");
        break;

    case CY_LVDS_GPIF_HDR_FLG3_SET:
        DBG_APP_INFO("Flag-3 has been set\r\n");
        break;
    }
} /* End of Cy_LVDS_GpifThreadEventCb () */

cy_stc_lvds_app_cb_t cb =
{
    .gpif_events       = Cy_LVDS_GpifEventCb,
    .gpif_error        = Cy_LVDS_GpifErrorCb,
    .gpif_thread_error = Cy_LVDS_GpifThreadErrorCb,
    .gpif_thread_event = Cy_LVDS_GpifThreadEventCb,
    .phy_events        = Cy_LVDS_PhyEventCb,
    .low_power_events  = Cy_LVDS_LowPowerEventCb
};

/* INMD config structure */
cy_stc_lvds_md_config_t mdArray0[16] = {
    {CY_LVDS_MD_VARIABLE0,             CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_PTS_TIMER_L,           CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_PTS_TIMER_H,           CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER0_L,          CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER0_H,          CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER1_L,          CY_LVDS_MD_VARIABLE},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT}
};

/*****************************************************************************
 * Function Name: Cy_LVDS_LVCMOS_Init
 *****************************************************************************
 * Summary
 *  Initialize the LVDS interface. Currently, only the SIP #0 is being initialized
 *  and configured to allow transfers into the HBW SRAM through DMA.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_LVCMOS_Init (void)
{
    cy_en_lvds_status_t status = CY_LVDS_SUCCESS;
    /* link training retry count */
    uint8_t LinkTrainRetry = 1;

    /* Data and address lines are alywas inputs for state machine 0 */
    LVDSSS_LVDS->GPIF[0].GPIF_AD_CONFIG |= _VAL2FLD(LVDSSS_LVDS_GPIF_GPIF_AD_CONFIG_DQ_INP_EN_CFG, 0x1u) |
                                           _VAL2FLD(LVDSSS_LVDS_GPIF_GPIF_AD_CONFIG_A_INP_EN_CFG, 0x1u);

    #if !PORT1_EN
    Cy_LVDS_PhySetInterruptMask(LVDSSS_LVDS,0, LVDSSS_LVDS_AFE_PHY_INTR_MONITOR_ADJUST_FAIL_Msk);
    #endif
    Cy_LVDS_PhySetInterruptMask(LVDSSS_LVDS,1, LVDSSS_LVDS_AFE_PHY_INTR_MONITOR_ADJUST_FAIL_Msk);

    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_MASKED_WD0_GPIF0_INTERRUPT_Msk|LVDSSS_LVDS_LVDS_INTR_MASKED_WD0_GPIF1_INTERRUPT_Msk|LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_DONE_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_DONE_Msk|
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DETECTED_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DETECTED_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk|
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_ENTRY_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_ENTRY_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_EXIT_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_EXIT_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L3_ENTRY_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L3_ENTRY_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK0_INTERRUPT_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK1_INTERRUPT_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_THREAD0_ERR_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_THREAD1_ERR_Msk |
                                              LVDSSS_LVDS_LVDS_INTR_WD0_THREAD2_ERR_Msk | LVDSSS_LVDS_LVDS_INTR_WD0_THREAD3_ERR_Msk | LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk);

    Cy_LVDS_RegisterCallback(LVDSSS_LVDS, &cb, &lvdsContext,&appCtxt);

#if (LVCMOS_EN && (!INMD_EN) )
    #if THREAD_INTERLEAVE_EN /* Two thread */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 1);
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 1, 1, 0, 0, 1);
    // Cy_LVDS_ThreadIntlvEnable(LVDSSS_LVDS,CY_LVDS_TH0_TH1_INTERLEAVED);
    DBG_APP_INFO("Two Threads are created\r\n");
    #else //Single thread
    // status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 1);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    DBG_APP_INFO("One Thread created\r\n");
    #endif
#else    //LVDS mode
    Cy_LVDS_SetFlagInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_WD1_TH0_HDR_FLGS_CLR_BIT0_Msk|LVDSSS_LVDS_LVDS_INTR_WD1_TH0_HDR_FLGS_SET_BIT0_Msk);
#endif

#if PORT1_EN

    #if THREAD_INTERLEAVE_EN /* Two thread configuration */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 2, 16, 0, 0, 1);
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 3, 17, 0, 0, 1);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    #else /* Single Threa configuration */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 2, 16, 0, 0, 1);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    #endif /* THREAD_INTERLEAVE_EN */
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_PPORT1_EN);
    do
    {
     /* Make sure Link Ready signal is Low before link training */
    Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
    // Cy_LVDS_Deinit(LVDSSS_LVDS, 1, &lvdsContext);
    DBG_APP_INFO("LVDS0_Deinit done\r\n");
    glIsLVDSLink1TrainingDone = false;
    glIsLVDSLink0TrainingDone = false;
    CLR_BIT (glPhyLinkTrainControl, P0_TRAINING_DONE);
    CLR_BIT (glPhyLinkTrainControl, P1_TRAINING_DONE);
    Cy_LVDS_Init(LVDSSS_LVDS, 1, &cy_lvds0_config, &lvdsContext);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    Cy_LVDS_Enable(LVDSSS_LVDS);

    DBG_APP_INFO("LVDS Training Started\r\n");
    #if LINK_TRAINING_EN
    Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
    #endif /* LINK_TRAINING_EN */
    #if (!LVCMOS_EN)
    status = Cy_LVDS_PhyTrainingStart(LVDSSS_LVDS, 1, cy_lvds0_config.phyConfig);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    #endif /* !LVCMOS_EN */
    /* Set Interrupt Mask for GPIF */
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF1_INTERRUPT_Msk);
    DBG_APP_INFO("Set Interrupt Mask for GPIF\r\n");

#if LVCMOS_EN
    status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 1, 0, 0xC);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#else
    status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 1, 0, 0);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#endif
    DBG_APP_INFO("P1_GPIF SM_Start\r\n");

#else /* PORT 0 or WIDELINK */
    #if THREAD_INTERLEAVE_EN /* Two Thread configuration */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 1);
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 1, 1, 0, 0, 1);
    #else /* Single Thread configuration */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 1);
    #endif

    do
    {
        DBG_APP_INFO("LVDS Training Started\r\n");
        // Cy_LVDS_Deinit(LVDSSS_LVDS, 0, &lvdsContext);
        DBG_APP_INFO("LVDS0_Deinit done\r\n");
        Cy_LVDS_Disable(LVDSSS_LVDS);
        DBG_APP_INFO("LVDS Disable\r\n");
        Cy_SysLib_DelayUs(10);//Delay 10uS
        /* Make sure Link Ready signal is Low before link training */
#if (FPGA_ENABLE && LINK_TRAINING_EN)
        Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
#endif /* FPGA_ENABLE && LINK_TRAINING_EN */
        glIsLVDSLink1TrainingDone = false;
        glIsLVDSLink0TrainingDone = false;
        CLR_BIT (glPhyLinkTrainControl, P0_TRAINING_DONE);
        CLR_BIT (glPhyLinkTrainControl, P1_TRAINING_DONE);
        status = Cy_LVDS_Init(LVDSSS_LVDS,0,&cy_lvds0_config,&lvdsContext);
        ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);

        Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk | LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK0_TRAINING_DONE_Msk | LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK1_TRAINING_DONE_Msk);
        Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk | LVDSSS_LVDS_LVDS_INTR_MASK_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk);
        Cy_LVDS_Enable(LVDSSS_LVDS);

#if LINK_TRAINING_EN
        Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN); /* Drive Link Ready HIGH*/
#endif

        /* In case of LVDS call PHY Training for PORT0 */
#if (!LVCMOS_EN)
        status = Cy_LVDS_PhyTrainingStart(LVDSSS_LVDS, 0, cy_lvds0_config.phyConfig);
        ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#endif

#if INMD_EN
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_INMD_EN);
    Cy_LVDS_InitMetadata(LVDSSS_LVDS, 0, 0, 16, mdArray0);
    DBG_APP_INFO("MetaDataInit - PORT 0\r\n");
#endif

#if LVCMOS_EN
        status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 0, 0, 0xC);
        ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#else
        status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 0, 0, 0);
        ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#endif
        DBG_APP_INFO("P0_GPIF SM Start\r\n");
#endif /* End of PORT1_EN */

        DBG_APP_INFO("LVDSLink1Training %d LVDSLink0Training %d \r\n",glIsLVDSLink1TrainingDone,glIsLVDSLink0TrainingDone);
        if (!WL_EN)
            glIsLVDSLink1TrainingDone = true;
        if(PORT1_EN)
            glIsLVDSLink0TrainingDone = true;
        if (LinkTrainRetry > 0)
        {
            LinkTrainRetry--;
        }
        else{
            /* exit the loop after retry count become zero*/
            vTaskDelay(50);
            break;
        }
    }while (!(glIsLVDSLink1TrainingDone && glIsLVDSLink0TrainingDone));
/* Stuck at fault counter signal select */
#if ((!LVCMOS_EN))
    LVDSSS_LVDS->AFE[0].STUCK_CNTR_SIG_SEL_0 = 0x59585756;
    LVDSSS_LVDS->AFE[0].STUCK_CNTR_SIG_SEL_1 = 0x5D5C5B5A;
#endif
    DBG_APP_INFO("LVDS_Init Done\r\n");
} /* End of Cy_LVDS_LVCMOS_Init() */

/******************************************************************************
 * Function name: Cy_Fx3g2_InitPeripheralClocks(bool adcClkEnable,
                                                bool usbfsClkEnable)
 ******************************************************************************
 *
 * Function used to enable clocks to different peripherals on the FX10/FX20 device.
 *
 * \param adcClkEnable
 * Whether to enable clock to the ADC in the USBSS block.
 *
 * \param usbfsClkEnable
 * Whether to enable bus reset detect clock input to the USBFS block.
 *
 *****************************************************************************/
void Cy_Fx3g2_InitPeripheralClocks (
        bool adcClkEnable,
        bool usbfsClkEnable)
{
    if (adcClkEnable) {
        /* Divide PERI clock at 75 MHz by 75 to get 1 MHz clock using 16-bit divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, 74);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR, CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable) {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
} /* End of Cy_Fx3g2_InitPeripheralClocks() */


/******************************************************************************
 * Function Name: Cy_LVDS_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_ISR(void)
{
    Cy_LVDS_IrqHandler(LVDSSS_LVDS, &lvdsContext);
} /* End of Cy_LVDS_ISR () */

/******************************************************************************
 * Function Name: Cy_LVDS_LPM_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS LPM Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_LPM_ISR(void)
{
    Cy_LVDS_LowPowerIrqHandler(LVDSSS_LVDS, &lvdsContext);
} /* End of Cy_LVDS_LPM_ISR() */

/******************************************************************************
 * Function Name: Cy_LVDS_Port0Dma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Port0 Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_Port0Dma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_0);
    portYIELD_FROM_ISR(true);
} /* End of Cy_LVDS_Port0Dma_ISR() */

/******************************************************************************
 * Function Name: Cy_LVDS_Port1Dma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Port1 Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_Port1Dma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_1);
    portYIELD_FROM_ISR(true);
} /* End of Cy_LVDS_Port1Dma_ISR() */

/******************************************************************************
 * Function Name: Cy_USB_SS_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for USB-SS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_SS_ISR(void)
{
    /* Call the USB32DEV interrupt handler. */
    Cy_USBSS_Cal_IntrHandler(&ssCalCtxt);
} /* End of Cy_USB_SS_ISR() */

/******************************************************************************
 * Function Name: Cy_USB_HS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-HS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_HS_ISR(void)
{
#if FREERTOS_ENABLE
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
#else
    Cy_USBHS_Cal_IntrHandler(&hsCalCtxt);
#endif /* FREERTOS_ENABLE */
} /* End of Cy_USB_HS_ISR() */

/******************************************************************************
 * Function Name: Cy_USB_IngressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Ingress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_IngressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_IN);
    portYIELD_FROM_ISR(true);
} /* End of Cy_USB_IngressDma_ISR() */

/******************************************************************************
 * Function Name: Cy_USB_EgressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Egress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EgressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_EG);
    portYIELD_FROM_ISR(true);
} /* End of Cy_USB_EgressDma_ISR */

/******************************************************************************
 * Function Name: Cy_UVC_DataWire_ISR
 ******************************************************************************
 * Summary:
 *  Handler for DataWire ISR
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_UVC_DataWire_ISR (void)
{
    /* Clear the interrupt first. */
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.uvcInEpNum, CY_USB_ENDP_DIR_IN);

    /* The current data buffer has been consumed. Move to another buffer if available. */
    Cy_UVC_AppHandleSendCompletion(&appCtxt);
} /* ENd of Cy_UVC_DataWire_ISR() */

/******************************************************************************
 * Function Name: Cy_VbusDetGpio_ISR
 ******************************************************************************
 * Summary
 *  Interrupt handler for the Vbus detect GPIO transition detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_VbusDetGpio_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_UVC_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
} /* End of Cy_VbusDetGpio_ISR() */

/******************************************************************************
 * Function Name: Cy_PrintVersionInfo(const char *type, uint32_t version)
 ******************************************************************************
 * Summary:
 *  Function to print version information to UART console.
 *
 * Parameters:
 *  type: Type of version string.
 *  version: Version number including major, minor, patch and build number.
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_PrintVersionInfo(const char *type, uint32_t version)
{
    char tString[32];
    uint16_t vBuild;
    uint8_t vMajor, vMinor, vPatch;
    uint8_t typeLen = strlen(type);

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    DBG_APP_INFO("%s", tString);
}

/******************************************************************************
 * Function Name: cy_usb_ConfigureSCBClock(uint8_t Index)
 ******************************************************************************
 * Summary:
 *  Function to initialize peripheral clock for SCB1 using the Index number
 *
 * Parameters:
 *  Index.
 *  Index number for SCB used
 *
 * Return:
 *  None
 *****************************************************************************/
void cy_usb_ConfigureSCBClock (uint8_t Index)
{
    /* Get the PERI clock frequency for the platform. */
    uint32_t hfClkFreq = Cy_SysClk_ClkPeriGetFrequency();
    Cy_SysClk_PeriphDisableDivider (CY_SYSCLK_DIV_8_BIT, Index);
    /* Configure PERI 16 bit clock divider for 8.33 MHz operation and enable it. */
    switch (hfClkFreq)
    {
    case 50000000UL:
        /* Divide 50 MHz by 6 to get 8.33 MHz. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, Index, 5);
        break;

    case 60000000UL:
        /* Divide 60 MHz by 5 to get 12 MHz. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, Index, 4);
        break;

    case 75000000UL:
        /* Divide 75 MHz by 9 to get 8.33 MHz. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, Index, 8);
        break;

    case 100000000UL:
        /* Divide 100 MHz by 12 to get 8.33 MHz. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, Index, 11);
        break;

    default:
        break;
    }

    Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_8_BIT, Index);
    Cy_SysLib_DelayUs(10);

    /* Connect the PERI clock to the SCB input. */
    Cy_SysClk_PeriphAssignDivider((en_clk_dst_t)(PCLK_SCB1_CLOCK), CY_SYSCLK_DIV_8_BIT, Index);
}

/* CYBSP_UART_SCB_IRQ */
const cy_stc_sysint_t scb_uart_irq_config = {
        .intrSrc = (IRQn_Type) scb_1_interrupt_IRQn,
        .intrPriority = 3u
};

void fpga_scbInterruptHandler(void)
{
    uint32_t writeStatus;
    uint32_t readStatus;
    uint32_t RxCount;

    writeStatus = Cy_SCB_GetTxInterruptStatus(FPGA_UART_SCB);
    Cy_SCB_ClearTxInterrupt(FPGA_UART_SCB, writeStatus);
    if((writeStatus & CY_SCB_TX_INTR_UART_DONE) != 0)
    {
        txDone = 0xFF;
    }

    readStatus = Cy_SCB_GetRxInterruptStatus(FPGA_UART_SCB);
    Cy_SCB_ClearRxInterrupt(FPGA_UART_SCB, readStatus);

    if((readStatus & CY_SCB_RX_INTR_NOT_EMPTY) != 0)
    {
        RxCount = Cy_SCB_UART_GetNumInRxFifo(FPGA_UART_SCB);
        for(uint8_t i = 0; i < RxCount; i++)
        {
            if (i > (SCB1_RXBUF_SIZE-1)) //check buffer full
                i = 0; //do not overflow
            scb_rx_buffer[i] = Cy_SCB_ReadRxFifo(FPGA_UART_SCB);
        }
    }
}

/******************************************************************************
 * Function Name: cy_usb_InitUart
 ******************************************************************************
 * Summary
 *  Initialize SCB as UART for sending uvc controls to FPGA ISP.
 *
 * Parameters:
 * \param scbIndex
 * Index of SCB to be used for UART output. Only 0 and 1 supported at
 * present.
 *
 * Return:
 *  void
 *****************************************************************************/
void cy_usb_InitUart (uint8_t scbIndex)
{
    #define SCB1_CLOCK_INDEX (0)
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_scb_uart_config_t uartCfg;
    CySCB_Type *pSCB = NULL;
    /* Only SCB0 and SCB1 supported at present. */
    CY_ASSERT_L1(scbIndex <= 1);

    cy_usb_ConfigureSCBClock (SCB1_CLOCK_INDEX);

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure the UART pins:
     * UART_TX as Strong Drive output.
     * UART_RX as input pin with output driver tri-stated.
     */
    if (scbIndex == FPGA_UART_SCB_IDX) {
        pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
        pinCfg.hsiom     = UART_TX_HSIOM;
        Cy_GPIO_Pin_Init(UART_TX_PORT, UART_TX_PIN, &pinCfg);
        pinCfg.hsiom     = UART_RX_HSIOM;
        pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
        Cy_GPIO_Pin_Init(UART_RX_PORT, UART_RX_PIN, &pinCfg);

        pSCB = FPGA_UART_SCB;
    }
    else {
        DBG_APP_INFO("Wronng SCB index\r\n");
        return;
    }

    memset((void *)&uartCfg, 0, sizeof(uartCfg));
    /* Divide 1.xx MHz clock by 10 to get baud rate of ~115200. */
    uartCfg.oversample          = 9;
    uartCfg.uartMode            = CY_SCB_UART_STANDARD;
    uartCfg.dataWidth           = 8;
    uartCfg.enableMsbFirst      = false;
    uartCfg.stopBits            = CY_SCB_UART_STOP_BITS_1;
    uartCfg.parity              = CY_SCB_UART_PARITY_NONE;
    uartCfg.breakWidth          = 12;
    uartCfg.rxFifoTriggerLevel  = 3UL;
    uartCfg.txFifoTriggerLevel  = 0UL;
    uartCfg.txFifoIntEnableMask = 512UL;

    if (Cy_SysClk_ClkPeriGetFrequency() == 60000000UL) {
        /* Divide 12 MHz clock by 13 to get baud rate of ~115200. */
        uartCfg.oversample = 13;
    }

    /* Configure and enable the SCB block as UART. */
    Cy_SCB_UART_Init(pSCB, &uartCfg, &gUartCtxt);

    // Cy_SCB_SetTxInterruptMask(pSCB,CY_SCB_TX_INTR_UART_DONE);
    // Cy_SCB_SetRxInterruptMask(pSCB,CY_SCB_RX_INTR_NOT_EMPTY);
     
    /*  Enable interrupt and UART block */
    // Cy_SysInt_Init(&scb_uart_irq_config, fpga_scbInterruptHandler);
    // NVIC_EnableIRQ((IRQn_Type) scb_uart_irq_config.intrSrc);
    Cy_SCB_UART_Enable(pSCB);
} /* End of cy_usb_InitUart() */

/******************************************************************************
 * Function Name: fpga_scbTx(uint8_t *tx_buf, uint8_t len)
 ******************************************************************************
 * Summary
 *  Function to transmit data over UART (SCB1) Tx pin
 *
 * Parameters:
 * \param tx_buf
 *  Character pointer, pointing to buffer containing data to transmit
 *
 * \param len
 *  Length of the data to transmit
 *
 * Return:
 *  void
 *****************************************************************************/
void fpga_scbTx(uint8_t *tx_buf, uint8_t len)
{
    txDone = 0x0;
    for(int i = 0; i<len; i++)
    {
       // Cy_Debug_AddToLog(1, "\n\rtx_buf = %d\n\r",*tx_buf);
        Cy_SCB_WriteTxFifo (FPGA_UART_SCB, (uint32_t)*tx_buf);
        tx_buf++;
        while(txDone == 0x0);
    }
} /* End of fpga_scbTx() */

/******************************************************************************
 * Function Name: Cy_USB_USBSSInit (void)
 ******************************************************************************
 * Summary
 *  Initialize USBSS block and attempt device enumeration.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_USB_USBSSInit (void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

    /*Can-Tx as GPIO*/
    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.hsiom     = P8_5_GPIO;
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    Cy_GPIO_Pin_Init(P8_5_PORT, P8_5_PIN, &pinCfg);

    /*Can-Rx as GPIO*/
    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    pinCfg.hsiom     = P8_4_GPIO;
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    Cy_GPIO_Pin_Init(P8_4_PORT, P8_4_PIN, &pinCfg);

    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    pinCfg.intEdge   = CY_GPIO_INTR_BOTH;
    pinCfg.intMask   = 0x01UL;
    Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    memset((void *)&pinCfg, 0, sizeof(pinCfg));
    /* Configure c_reset FPGA GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;//CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = TI180_CRESET_GPIO;
    Cy_GPIO_Pin_Init(TI180_CRESET_GPIO_PORT, TI180_CRESET_GPIO_PIN, &pinCfg);
    Cy_GPIO_Clr(TI180_CRESET_GPIO_PORT, TI180_CRESET_GPIO_PIN);
    Cy_SysLib_Delay(20);
    Cy_GPIO_Set(TI180_CRESET_GPIO_PORT, TI180_CRESET_GPIO_PIN);

    /* Configure input GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    Cy_GPIO_Pin_Init(T120_CDONE_PORT, T120_CDONE_PIN, &pinCfg);

    /* Register edge detect interrupt for Vbus detect GPIO.
     * TODO: Update code to support CM0.
     */
    /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.cm0pSrc      = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 3;
#endif
    Cy_SysInt_Init(&intrCfg, Cy_VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /*
     * CMR-4 RTL specific initialization: The LVDS2USB32SS and USB32DEV IP blocks
     * need to be enabled before we try to do High BandWidth DMA initialization.
     */
    MAIN_REG->CTRL |= MAIN_REG_CTRL_IP_ENABLED_Msk;
#if USBSS_GEN2_ENABLE
    USB32DEV->USB32DEV_PROT.PROT_SUBLINK_LSM = 0x000A000A;
#endif /* USBSS_GEN2_ENABLE */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Register the LVDS ISR and enable the interrupt for LVDS. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_lvds_int_o_IRQn;
    intrCfg.intrPriority = 6;
#else
    intrCfg.intrSrc      = NvicMux0_IRQn;
    intrCfg.intrPriority = 2;
    intrCfg.cm0pSrc      = lvds2usb32ss_lvds_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for SIP0 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc      = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc      = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port0Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for SIP1 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc      = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc      = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port1Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the USBSS ISR and enable the interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc      = NvicMux1_IRQn;
    intrCfg.cm0pSrc      = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc      = NvicMux1_IRQn;
    intrCfg.cm0pSrc      = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Ingress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc      = NvicMux3_IRQn;
    intrCfg.cm0pSrc      = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_IngressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

/* ISR for the USB Egress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc      = NvicMux2_IRQn;
    intrCfg.cm0pSrc      = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_EgressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for LVDS LPM */
    intrCfg.intrSrc      = lvds2usb32ss_lvds_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_LPM_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_wakeup_int_o_IRQn);
} /* End of Cy_USB_USBSSInit() */

/*
 * Notes on DMA Buffer RAM Usage:
 * 1. The initial part of the buffer RAM is reserved for the descriptors used by the DMA manager.
 *    The space used for this is reserved using the gHbDmaDescriptorSpace array which is placed
 *    in a section named ".hbDmaDescriptor". This array should have a minimum size of 8192 (8 KB)
 *    and has a default size allocation of 16384 bytes (16 KB). No other data should be placed
 *    in this section.
 *
 * 2. The descriptor region is followed by RW data structures which are placed in the ".descSection".
 *    Only data members placed in this section will be initialized during the firmware load
 *    process.
 *
 * 3. The ".descSection" is followed by the ".hbBufSection" which will hold data structures
 *    which do not need to be explicitly initialized (equivalent of ".bss" section).
 *
 * 4. This is followed by the ".hbDmaBufferHeap" section which will be used to allocate all
 *    the DMA buffers from. The gHbDmaBufferHeap array represents the memory region which will
 *    be given to the DMA buffer manager to allocate buffers from and can be sized based on the
 *    available memory. No other data or variables should be placed in this section.
 *
 * Any pre-initialized data which is to be placed in the High BandWidth Buffer RAM should be
 * added to the ".descSection". Any non-initialized data which is to be placed in the High
 * BandWidth Buffer RAM should be added to the ".hbBufSection".
 */

/* Region of 16 KB reserved for High BandWidth DMA descriptors. */
static __attribute__ ((section(".hbDmaDescriptor"), used)) uint32_t gHbDmaDescriptorSpace[16384 / 4];

/* Region of 960 KB reserved for DMA buffer heap. */
/*
 * Note: Since this application requires a large amount of buffer RAM, it is only supported on parts
 * that support 1 MB of DMA buffer RAM.
 */
static __attribute__ ((section(".hbDmaBufferHeap"), used)) uint32_t gHbDmaBufferHeap[960 * 1024 / 4];

/******************************************************************************
 * Function Name: Cy_InitHbDma
 ******************************************************************************
 * Summary
 *  Initialize HBDMA block
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 *****************************************************************************/
bool Cy_InitHbDma(void)
{
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Setup a HBW DMA descriptor list. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, 256U);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Verify that gHbDmaDescriptorSpace is located at the base of the DMA buffer SRAM. */
    if ((uint32_t)gHbDmaDescriptorSpace != CY_HBW_SRAM_BASE_ADDR) {
        DBG_APP_ERR("High BandWidth DMA descriptors not placed at the correct address\r\n");
        return false;
    }

    /* Setup a HBW DMA descriptor list using the space reserved in gHbDmaDescriptorSpace. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, sizeof(gHbDmaDescriptorSpace) / 16);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager to use the gHbDmaBufferHeap region. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)gHbDmaBufferHeap, sizeof(gHbDmaBufferHeap));
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Request DMA callback handling in ISR context. */
    Cy_HBDma_Mgr_DmaCallbackConfigure(&HBW_MgrCtxt, true);

    /* Both LVDS DMA adapters are used only for ingress transfers. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);

    return true;
} /* End of Cy_InitHbDma */

/******************************************************************************
 * Function Name: Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt)
 ******************************************************************************
 * Summary:
 *  Function to de-initialize USBSS device
 *
 * Parameters:
 *  \param pCalCtxt
 * USB CAL layer context pointer
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt)
{
    USB32DEV_Type *base = pCalCtxt->regBase;
    USB32DEV_MAIN_Type *USB32DEV_MAIN = &base->USB32DEV_MAIN;

    /* Disable the clock for USB3.2 function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_CLK_EN_Msk;

    /* Disable PHYSS */
    base->USB32DEV_PHYSS.USB40PHY[0].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    base->USB32DEV_PHYSS.USB40PHY[1].USB40PHY_TOP.TOP_CTRL_0 &=
                    ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    /* Disable the SuperSpeed Device function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_SSDEV_ENABLE_Msk;
} /* End of Cy_USBSS_DeInit() */

/******************************************************************************
 * Function Name: Cy_USB_DisableUsbBlock(void)
 ******************************************************************************
 * Summary:
 *  Function to disable the USB32DEV IP block after terminating current
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_DisableUsbBlock (void)
{
    /* Disable the USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL &= ~USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Disable HBDMA adapter interrupts and the adapter itself. */
    NVIC_DisableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_DisableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);
    Cy_HBDma_DeInit(&HBW_DrvCtxt);

    DBG_APP_TRACE("Disabled HBWSS DMA adapters\r\n");
} /* End of Cy_USB_DisableUsbBlock() */

/******************************************************************************
 * Function Name: Cy_USB_EnableUsbBlock(void)
 ******************************************************************************
 * Summary:
 *  Function to enable the USB32DEV IP block before enabling a new USB
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EnableUsbBlock (void)
{
    /* Enable the USB DMA adapters and respective interrupts. */
    Cy_HBDma_Init(NULL, USB32DEV, &HBW_DrvCtxt, 0, 0);

    NVIC_EnableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_EnableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);

#if USBSS_GEN2_ENABLE
    USB32DEV->USB32DEV_PROT.PROT_SUBLINK_LSM = 0x000A000A;
#endif /* USBSS_GEN2_ENABLE */

    /* Make sure to enable USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;
} /* End of Cy_USB_EnableUsbBlock() */

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
bool Cy_USB_SSConnectionEnable (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    pAppCtxt->desiredSpeed = USB_CONN_TYPE;
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, pAppCtxt->desiredSpeed);
    pAppCtxt->usbConnected = true;
    LOG_COLOR("USB Speed %d \n\r",pAppCtxt->desiredSpeed);
    return true;
#if FORCE_USBHS
    DBG_APP_INFO("USB_DEV_HS\r\n");
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, CY_USBD_USB_DEV_HS);
    pAppCtxt->usbConnected = true;
#endif /* FORCE_USBHS */

    return true;
} /* End of Cy_USB_SSConnectionEnable() */

/******************************************************************************
 * Function Name: Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt)
 *****************************************************************************
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
void Cy_USB_SSConnectionDisable (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    Cy_USBSS_DeInit(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    pAppCtxt->usbConnected = false;
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
} /* End of Cy_USB_SSConnectionDisable() */

/******************************************************************************
* Function Name: main(void)
*******************************************************************************
* Summary:
*  Entry to the application.
*
* Parameters:
*  void

* Return:
*  Does not return.
******************************************************************************/
int main (void)
{
    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init (&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device clocks to the desired values. */
    cybsp_init();

    Cy_Fx3g2_InitPeripheralClocks(true, true);

    hfclkFreq = Cy_SysClk_ClkFastGetFrequency();

    /* Initialize the PDL and register ISR for USB block. */
    Cy_USB_USBSSInit();

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

#if CY_CPU_CORTEX_M4
    /*
     * If logging is done through the USBFS port, ISR execution is required in this application.
     * Set BASEPRI value to 0 to ensure all exceptions can run and then enable interrupts.
     */
    __set_BASEPRI(0);
#endif /* CY_CPU_CORTEX_M4 */
    /* Enable interrupts. */
    __enable_irq ();

#if DEBUG_INFRA_EN
#if !USBFS_LOGS_ENABLE
    /* Initialize the UART for logging. */
    cy_usb_InitUart(LOGGING_SCB_IDX); //reconfigure scb1 with different PINs
#endif /* USBFS_LOGS_ENABLE */
    Cy_Debug_LogInit(&dbgCfg);

    /* Create task for printing logs and check status. */
    xTaskCreate(Cy_DebugPrintTaskHandler, "PrintLogTask", 512, NULL, 5, &printLogTaskHandle);
    Cy_SysLib_Delay(500);
    Cy_Debug_AddToLog(1, "***** FX20: USB Video (UVC) 4k Camera Application *****\r\n");
    /* Print application, USBD stack and HBDMA version information. */
    Cy_PrintVersionInfo("APP_VERSION: ", APP_VERSION_NUM);
    Cy_PrintVersionInfo("USBD_VERSION: ", USBD_VERSION_NUM);
    Cy_PrintVersionInfo("HBDMA_VERSION: ", HBDMA_VERSION_NUM);
#endif /* DEBUG_INFRA_EN */
    Cy_USB_I2CInit ();
    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&ssCalCtxt, 0, sizeof(ssCalCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    pCpuDmacBase = ((DMAC_Type *)DMAC_BASE);
    pCpuDw0Base = ((DW_Type *)DW0_BASE);
    pCpuDw1Base = ((DW_Type *)DW1_BASE);

    /* Clear the control signal during reset */
    /* Initial link ready signal should be zero */
#if (FPGA_ENABLE && LINK_TRAINING_EN) /*In case of LVDS or LVCMOS DDR */
    Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, LINK_READY_CTL_PORT,LINK_READY_CTL_PIN,
        CY_LVDS_PHY_GPIO_OUTPUT, CY_LVDS_PHY_GPIO_NO_INTERRUPT);
    Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
#endif /* FPGA_ENABLE && LINK_TRAINING_EN */

    /* Store IP base address in CAL context. */
    ssCalCtxt.regBase = USB32DEV;
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

    /*
     * Make sure any previous USB connection state is cleared. Give some delay
     * to allow the host to process disconnection.
     */
    Cy_USBSS_DeInit(&ssCalCtxt);
    Cy_SysLib_Delay(500);

    /* Initialize the HbDma IP and DMA Manager */
    Cy_InitHbDma();
    DBG_APP_INFO("InitHbDma done\r\n");

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt, &ssCalCtxt, &HBW_MgrCtxt);
    DBG_APP_INFO("USBD_Init done\r\n");

#if AUDIO_IF_EN
    Cy_PDM_AppMxPdmInit();
#endif /* AUDIO_IF_EN */


    /* Specify that DMA clock should be set to 240 MHz once USB 3.x connection is active. */
    Cy_USBD_SetDmaClkFreq(&usbdCtxt, CY_HBDMA_CLK_240_MHZ);

    /* Enable stall cycles between back-to-back AHB accesses to high bandwidth RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;
    /*
     * Make sure InitEventLog called before AppInit or before creating
     * thread for application.
     * Allocate a memory block and register it as USB event log buffer.
     */
    appCtxt.pUsbEvtLogBuf = (uint32_t *)g_UsbEvtLogBuf;
    Cy_USBD_InitEventLog(&usbdCtxt, appCtxt.pUsbEvtLogBuf, 512u);

    /* Initialize the application and create echo device thread. */
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base, &HBW_MgrCtxt);
    /* Register USB descriptors with the stack. */
    CyApp_RegisterUsbDescriptors(&appCtxt, CY_USBD_USB_DEV_SS_GEN1);

#if PTM_ENABLE
    Cy_SoftwareTimerInit();
#endif

    /* Invokes scheduler: Not expected to return. */
    vTaskStartScheduler();
    while (1);

    return 0;
} /* End of main() */

/******************************************************************************
 * Function Name: Cy_OnResetUser(void)
 ******************************************************************************
 * Summary:
 *  Init function which is executed before the load regions in RAM are updated.
 *  The High BandWidth subsystem needs to be enable here to allow variables
 *  placed in the High BandWidth SRAM to be updated.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}

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
void Cy_USB_AppPrintUsbEventLog (cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal)
{
    uint16_t prevLogIdx = gCurUsbEvtLogIndex;

    (void)pSSCal;

    /* Print out any pending USB event log data. */
    gCurUsbEvtLogIndex = Cy_USBD_GetEvtLogIndex(pAppCtxt->pUsbdCtxt);
    while (gCurUsbEvtLogIndex != prevLogIdx) {
        DBG_APP_INFO("USBEVT: %x\r\n", pAppCtxt->pUsbEvtLogBuf[prevLogIdx]);
        prevLogIdx++;
        if (prevLogIdx == 512u) {
            prevLogIdx = 0u;
        }
    }
} /* End of Cy_USB_AppPrintUsbEventLog() */

/* [] END OF FILE */
