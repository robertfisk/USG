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


#include "stm32f4xx_hal.h"
#include "upstream_spi.h"


//These defines are duplicated in downstream_hid.h. Keep them in sync!
#define HID_MOUSE_INPUT_DATA_LEN        4
#define HID_MOUSE_OUTPUT_DATA_LEN       0
#define HID_MOUSE_MAX_BUTTONS           3

#define HID_KEYBOARD_INPUT_DATA_LEN     8
#define HID_KEYBOARD_OUTPUT_DATA_LEN    1
#define HID_KEYBOARD_MAX_KEY            101
#define HID_KEYBOARD_MAX_LED            3


typedef enum
{
    KEYBOARD_OUT_STATE_IDLE,
    KEYBOARD_OUT_STATE_DATA_READY,
    KEYBOARD_OUT_STATE_BUSY
} KeyboardOutStateTypeDef;


typedef uint8_t (*UpstreamHidGetReportCallback)(uint8_t *report,
                                                uint16_t len);

void Upstream_HID_DeInit(void);
void Upstream_HID_GetNextInterruptReport(UpstreamHidGetReportCallback callback);
void Upstream_HID_SendControlReport(UpstreamPacketTypeDef* packetToSend, uint8_t dataLength);
void Upstream_HID_ReallySendControlReport(void);




#endif /* UPSTREAM_HID_H_ */
