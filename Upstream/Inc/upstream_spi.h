/*
 * upstream_spi.h
 *
 *  Created on: 21/06/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_UPSTREAM_SPI_H_
#define INC_UPSTREAM_SPI_H_


#include "usbd_config.h"


#define UPSTREAM_PACKET_HEADER_LEN		(2)			//Min length = CommandClass & Command bytes
#define UPSTREAM_PACKET_LEN				(UPSTREAM_PACKET_HEADER_LEN + MSC_MEDIA_PACKET)
#define UPSTREAM_PACKET_LEN_MIN			(UPSTREAM_PACKET_HEADER_LEN)

#define UPSTREAM_PACKET_HEADER_LEN_16	(UPSTREAM_PACKET_HEADER_LEN / 2)
#define UPSTREAM_PACKET_LEN_16			(UPSTREAM_PACKET_LEN / 2)
#define UPSTREAM_PACKET_LEN_MIN_16		(UPSTREAM_PACKET_LEN_MIN / 2)


#define UPSTREAM_SPI_FREAKOUT								\
	do {													\
		LED_Fault_SetBlinkRate(LED_FAST_BLINK_RATE);		\
		/*UpstreamInterfaceState = UPSTREAM_INTERFACE_ERROR; */ \
		Upstream_StateMachine_SetErrorState();				\
		while (1);											\
} while (0);



typedef enum
{
	UPSTREAM_INTERFACE_IDLE,
	UPSTREAM_INTERFACE_TX_SIZE_WAIT,
	UPSTREAM_INTERFACE_TX_SIZE,
	UPSTREAM_INTERFACE_TX_PACKET_WAIT,
	UPSTREAM_INTERFACE_TX_PACKET,
	UPSTREAM_INTERFACE_RX_SIZE_WAIT,
	UPSTREAM_INTERFACE_RX_SIZE,
	UPSTREAM_INTERFACE_RX_PACKET_WAIT,
	UPSTREAM_INTERFACE_RX_PACKET,
	UPSTREAM_INTERFACE_ERROR
}
InterfaceStateTypeDef;


typedef enum
{
	NOT_BUSY,
	BUSY
}
PacketBusyTypeDef;


typedef struct
{
	PacketBusyTypeDef	Busy;						//Everything after Busy should be word-aligned
	uint16_t			Length16 __ALIGN_END;			//Packet length includes CommandClass, Command, and Data
	uint8_t				CommandClass;
	uint8_t				Command;
	uint8_t				Data[MSC_MEDIA_PACKET];		//Should (must?) be word-aligned, for USB copy routine
}
UpstreamPacketTypeDef;



typedef void (*FreePacketCallbackTypeDef)(UpstreamPacketTypeDef* freePacket);
typedef void (*SpiPacketReceivedCallbackTypeDef)(UpstreamPacketTypeDef* replyPacket);


void Upstream_InitSPI(void);
HAL_StatusTypeDef Upstream_GetFreePacket(FreePacketCallbackTypeDef callback);
UpstreamPacketTypeDef* Upstream_GetFreePacketImmediately(void);
void Upstream_ReleasePacket(UpstreamPacketTypeDef* packetToRelease);
HAL_StatusTypeDef Upstream_TransmitPacket(UpstreamPacketTypeDef* packetToWrite);
HAL_StatusTypeDef Upstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback);
void Upstream_TxOkInterrupt(void);
void Upstream_SPIProcess_InterruptSafe(void);
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);


#endif /* INC_UPSTREAM_SPI_H_ */
