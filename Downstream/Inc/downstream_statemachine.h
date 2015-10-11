/*
 * downstream_statemachine.h
 *
 *  Created on: 2/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_DOWNSTREAM_STATEMACHINE_H_
#define INC_DOWNSTREAM_STATEMACHINE_H_


#include "usbh_def.h"
#include "usb_host.h"
#include "downstream_spi.h"


typedef enum
{
	STATE_DEVICE_NOT_READY,
	STATE_DEVICE_READY,			//Go here if HOST_USER_CLASS_ACTIVE callback arrives first
	STATE_WAIT_DEVICE_READY,	//Go here if COMMAND_INTERFACE_NOTIFY_DEVICE message arrives first
	STATE_ACTIVE,
	STATE_ERROR
} DownstreamStateTypeDef;



#define DOWNSTREAM_STATEMACHINE_FREAKOUT				\
	do {												\
		USB_Host_Disconnect();							\
		LED_Fault_SetBlinkRate(LED_FAST_BLINK_RATE);	\
		DownstreamState = STATE_ERROR;					\
		while (1);										\
} while (0);




void Downstream_InitStateMachine(void);
void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id);
void Downstream_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
void Downstream_PacketProcessor_GenericErrorReply(DownstreamPacketTypeDef* replyPacket);
void Downstream_PacketProcessor_ClassReply(DownstreamPacketTypeDef* replyPacket);
void Downstream_PacketProcessor_SetErrorState(void);
void Downstream_PacketProcessor_FreakOut(void);


#endif /* INC_DOWNSTREAM_STATEMACHINE_H_ */
