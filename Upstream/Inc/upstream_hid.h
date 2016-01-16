/*
 * upstream_hid.h
 *
 *  Created on: Jan 16, 2016
 *      Author: Robert Fisk
  *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#ifndef UPSTREAM_HID_H_
#define UPSTREAM_HID_H_



//#include "usbd_def.h"
#include "stm32f4xx_hal.h"
#include "upstream_interface_def.h"


typedef uint8_t UpstreamHidSendReportCallback(uint8_t *report,
                                              uint16_t len);


void Upstream_HID_Init(InterfaceCommandClassTypeDef newClass);
void Upstream_HID_DeInit(void);
void Upstream_HID_GetNextReport(UpstreamHidSendReportCallback callback);





#endif /* UPSTREAM_HID_H_ */
