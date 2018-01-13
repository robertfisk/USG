/*
 * downstream_hid.h
 *
 *  Created on: Apr 10, 2016
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_DOWNSTREAM_HID_H_
#define INC_DOWNSTREAM_HID_H_


#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "usbh_def.h"


#define HID_MAX_REPORT_LEN              8

//These defines are duplicated in upstream_hid.h. Keep them in sync!
#define HID_MOUSE_INPUT_DATA_LEN        4
#define HID_MOUSE_OUTPUT_DATA_LEN       0
#define HID_MOUSE_MAX_BUTTONS           3

#define HID_KEYBOARD_INPUT_DATA_LEN     8
#define HID_KEYBOARD_OUTPUT_DATA_LEN    1
#define HID_KEYBOARD_MAX_KEY            101             //Also set in Upstream's HID report descriptor
#define HID_KEYBOARD_MAX_LED            3


//Stuff for parsing HID descriptors:
#define HID_ITEM_LONG               0xFC
#define HID_ITEM_MASK               0xFC
#define HID_ITEM_LENGTH_MASK        0x03

#define HID_ITEM_USAGE_PAGE         0x04        //'global' usage page
#define HID_ITEM_USAGE_PAGE_BUTTON  0x09
#define HID_ITEM_USAGE_PAGE_DESKTOP 0x01

#define HID_ITEM_COLLECTION         0xA0
#define HID_ITEM_COLLECTION_PHYS    0x00
#define HID_ITEM_END_COLLECTION     0xC0

#define HID_ITEM_USAGE              0x08        //'local' usage
#define HID_ITEM_USAGE_X            0x30
#define HID_ITEM_USAGE_Y            0x31
#define HID_ITEM_USAGE_WHEEL        0x38

#define HID_ITEM_REPORT_SIZE        0x74        //'global' report size
#define HID_ITEM_REPORT_COUNT       0x94        //'global' report count
#define HID_ITEM_REPORT_ID          0x84

#define HID_ITEM_INPUT              0x80        //'main' input
#define HID_ITEM_INPUT_ABS          0x02
#define HID_ITEM_INPUT_REL          0x06


typedef void (*TransactionCompleteCallbackTypeDef)(USBH_StatusTypeDef result);

InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void);
void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
void Downstream_HID_InterruptReportCallback(USBH_StatusTypeDef result);
void Downstream_HID_SendReportCallback(USBH_StatusTypeDef result);



#endif /* INC_DOWNSTREAM_HID_H_ */
