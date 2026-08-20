/******************************************************************************
* \file app_version.h
* \version 1.0.0
*
* @brief @{Version definition for the firmware application. This version
* information follows a Cypress defined format that identifies the type
* of application along with the version information.@}
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
#ifndef _APP_VERSION_H_
#define _APP_VERSION_H_
/******************************************************************************
 * Constant definitions.
 *****************************************************************************/
#define APP_VERSION_MAJOR      (1u)
#define APP_VERSION_MINOR      (0u)
#define APP_VERSION_PATCH      (1u)
#define APP_VERSION_BUILD      (85u)

#define APP_VERSION_NUM        ((APP_VERSION_MAJOR << 28u) | (APP_VERSION_MINOR << 24u) | \
        (APP_VERSION_PATCH << 16u) | (APP_VERSION_BUILD))

#endif /* _APP_VERSION_H_ */
/* [] END OF FILE */
