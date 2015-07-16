/*
 * downstream_spi.h
 *
 *  Created on: 21/06/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_SPI_H_
#define INC_DOWNSTREAM_SPI_H_


#include "usbd_def.h"


#define DOWNSTREAM_PACKET_HEADER_LEN	(2)			//Min length = CommandClass & Command bytes
#define DOWNSTREAM_PACKET_LEN			(DOWNSTREAM_PACKET_HEADER_LEN + USB_HS_MAX_PACKET_SIZE)
#define DOWNSTREAM_PACKET_LEN_MIN		(DOWNSTREAM_PACKET_HEADER_LEN)


#define SPI_INTERFACE_FREAKOUT_VOID								\
	do {														\
		while (1);												\
		/*DownstreamInterfaceState = INTERFACE_STATE_ERROR;*/	\
		/*return;*/												\
} while (0);

#define SPI_INTERFACE_FREAKOUT_HAL_ERROR						\
	do {														\
		while (1);												\
		/*DownstreamInterfaceState = INTERFACE_STATE_ERROR;*/	\
		/*return HAL_ERROR;*/									\
} while (0);



typedef enum
{
	INTERFACE_STATE_RESET				= 0,
	INTERFACE_STATE_WAITING_CLIENT		= 1,
	INTERFACE_STATE_IDLE				= 2,
	INTERFACE_STATE_TX_SIZE_WAIT		= 3,
	INTERFACE_STATE_TX_SIZE				= 4,
	INTERFACE_STATE_TX_PACKET_WAIT		= 5,
	INTERFACE_STATE_TX_PACKET			= 6,
	INTERFACE_STATE_RX_SIZE_WAIT		= 7,
	INTERFACE_STATE_RX_SIZE				= 8,
	INTERFACE_STATE_RX_PACKET_WAIT		= 9,
	INTERFACE_STATE_RX_PACKET			= 10,
	INTERFACE_STATE_ERROR				= 11
} InterfaceStateTypeDef;


typedef enum
{
	NOT_BUSY 		= 0,
	BUSY			= 1
} PacketBusyTypeDef;


typedef struct
{
	PacketBusyTypeDef	Busy;						//Everything after Busy should be word-aligned
	uint16_t			Length __ALIGN_END;			//Packet length includes CommandClass, Command, and Data
	uint8_t				CommandClass;
	uint8_t				Command;
	uint8_t				Data[USB_HS_MAX_PACKET_SIZE];	//Should (must?) be word-aligned, for USB copy routine
	uint8_t				RxCrc;
}
DownstreamPacketTypeDef;



typedef void (*FreePacketCallbackTypeDef)(DownstreamPacketTypeDef* freePacket);
typedef void (*SpiPacketReceivedCallbackTypeDef)(DownstreamPacketTypeDef* replyPacket);


void Downstream_InitInterface(void);
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback);
DownstreamPacketTypeDef* Downstream_GetFreePacketImmediately(void);
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease);
HAL_StatusTypeDef Downstream_SendPacket(DownstreamPacketTypeDef* packetToWrite);
HAL_StatusTypeDef Downstream_GetPacket(SpiPacketReceivedCallbackTypeDef callback);
void Downstream_TxOkInterrupt(void);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);


#endif /* INC_DOWNSTREAM_SPI_H_ */
