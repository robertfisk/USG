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




InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void);
void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
void Downstream_HID_InterruptReportCallback(DownstreamPacketTypeDef* packetToSend);



#endif /* INC_DOWNSTREAM_HID_H_ */
