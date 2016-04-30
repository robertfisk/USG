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



#define HID_ITEM_LONG               0xFE
#define HID_ITEM_LENGTH_MASK        0x03

#define HID_ITEM_USAGE_PAGE         0x05        //'global' usage page
#define HID_ITEM_USAGE_PAGE_BUTTON  0x09
#define HID_ITEM_USAGE_PAGE_DESKTOP 0x01

#define HID_ITEM_COLLECTION         0xA1
#define HID_ITEM_COLLECTION_PHYS    0x00
#define HID_ITEM_END_COLLECTION     0xC0

#define HID_ITEM_USAGE              0x09        //'local' usage
#define HID_ITEM_USAGE_X            0x30
#define HID_ITEM_USAGE_Y            0x31
#define HID_ITEM_USAGE_WHEEL        0x38

#define HID_ITEM_REPORT_SIZE        0x75        //'global' report size
#define HID_ITEM_REPORT_COUNT       0x95        //'global' report count

#define HID_ITEM_INPUT              0x81        //'main' input
#define HID_ITEM_INPUT_ABS          0x02
#define HID_ITEM_INPUT_REL          0x06



InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void);
void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
void Downstream_HID_InterruptReportCallback(DownstreamPacketTypeDef* packetToSend);



#endif /* INC_DOWNSTREAM_HID_H_ */
