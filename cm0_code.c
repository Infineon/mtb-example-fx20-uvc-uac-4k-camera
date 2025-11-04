/******************************************************************************
* \file cm0_code.c
* \version 1.0
*
* Source file which provides binary content equivalent to the Cortex-M0+ based
* start-up code which enables the Cortex-M4 processor.
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

#include "cy_pdl.h"
#include <stdint.h>

#if (!CY_CPU_CORTEX_M4)
#error "INVALID: CM0+ NOT SUPPORTED"
#endif /* !CY_CPU_CORTEX_M4 */

#if BLOAD_ENABLE

/* Section reserved to store the application signature. Size is equal to flash row size. */
__attribute__ ((section(".cy_app_signature"), used)) const uint8_t cyFx3g2AppSignature[512] = {0};

#if defined (__ARMCC_VERSION)
/*******************************************************************************
* Function Name: Cy_AppVerify_MemorySymbols
****************************************************************************//**
*
* The intention of the function is to declare boundaries of the memories for the
* MDK compilers. For the rest of the supported compilers, this is done using
* linker configuration files. The following symbols used by the cymcuelftool.
*
*******************************************************************************/
#if (CY_FLASH_SIZE == 0x00080000UL)
__asm(
"Cy_AppVerify_MemorySymbols:\n"
        ".global __cy_app_verify_start\n"
        ".global __cy_app_verify_length\n"
        ".global __cy_app_signature_addr\n"
        ".equ __cy_app_verify_start, 0x10008000\n"
        ".equ __cy_app_verify_length, 0x00077E00\n"
        ".equ __cy_app_signature_addr, 0x1007FE00\n"
     );
#elif (CY_FLASH_SIZE == 0x00040000UL)
__asm(
"Cy_AppVerify_MemorySymbols:\n"
        ".global __cy_app_verify_start\n"
        ".global __cy_app_verify_length\n"
        ".global __cy_app_signature_addr\n"
        ".equ __cy_app_verify_start, 0x10008000\n"
        ".equ __cy_app_verify_length, 0x00037E00\n"
        ".equ __cy_app_signature_addr, 0x1003FE00\n"
     );
#endif /* (CY_FLASH_SIZE == 0x00080000UL) */
#endif /* defined (__ARMCC_VERSION) */

#endif /* BLOAD_ENABLE */

#if (CY_CPU_CORTEX_M4)

#if BLOAD_ENABLE

__attribute__ ((section(".cm0_code"), used))
const uint32_t Cm0Code[256] = {
    0x08002000,
    0x100080E5,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x100080EF,
    0x22044B03,
    0x46C0601A,
    0xBF3046C0,
    0x46C0E7F8,
    0xE000ED10,
    0x4B0CB510,
    0x601A2201,
    0x46C046C0,
    0x46C046C0,
    0x4A0A4B09,
    0x490A601A,
    0x4B0A680A,
    0x4B0A401A,
    0x600B4313,
    0x681B4B09,
    0xD5FB06DB,
    0xF7FFB662,
    0x46C0FFDD,
    0xE000E180,
    0x40200200,
    0x10008400,
    0x40201200,
    0x0000FFFC,
    0x05FA0003,
    0x40200004,
    0xF7FF2000,
    0xBF30FFD5,
    0x47704770,
    0x46C04770,
    0x10008000,
    0x08000000,
    0x00000080,
    0x10008114,
    0x08000000,
    0x00000000,
    0x08000000,
    0x00000000
};

#else

__attribute__ ((section(".cm0_code"), used))
const uint32_t Cm0Code[256] = {
    0x08002000,
    0x100000D1,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x100000DB,
    0x22044B03,
    0x46C0601A,
    0xBF3046C0,
    0x46C0E7F8,
    0xE000ED10,
    0x4B08B510,
    0x601A4A08,
    0x680A4908,
    0x401A4B08,
    0x43134B08,
    0x4B08600B,
    0x06DB681B,
    0xB662D5FB,
    0xFFE4F7FF,
    0x40200200,
    0x10000400,
    0x40201200,
    0x0000FFFC,
    0x05FA0003,
    0x40200004,
    0xF7FF2000,
    0xBF30FFDF,
    0x47704770,
    0x46C04770,
    0x10000000,
    0x08000000,
    0x00000080,
    0x10000100,
    0x08000000,
    0x00000000,
    0x08000000,
    0x00000000
};

#endif /* BLOAD_ENABLE */

#endif /* (CY_CPU_CORTEX_M4) */

/*[]*/
