/*
 * upstream_spi.h
 *
 *  Created on: 21/06/2015
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_SPI_H_
#define INC_UPSTREAM_SPI_H_


#include "usbd_config.h"


#define UPSTREAM_PACKET_HEADER_LEN	(2)			//Min length = CommandClass & Command bytes
#define UPSTREAM_PACKET_LEN			(UPSTREAM_PACKET_HEADER_LEN + MSC_MEDIA_PACKET)
#define UPSTREAM_PACKET_LEN_MIN		(UPSTREAM_PACKET_HEADER_LEN)


#define SPI_INTERFACE_FREAKOUT_VOID								\
	do {														\
		while (1);												\
		/*UpstreamInterfaceState = INTERFACE_STATE_ERROR;*/	\
		/*return;*/												\
} while (0);

#define SPI_INTERFACE_FREAKOUT_HAL_ERROR						\
	do {														\
		while (1);												\
		/*UpstreamInterfaceState = INTERFACE_STATE_ERROR;*/	\
		/*return HAL_ERROR;*/									\
} while (0);



typedef enum
{
	UPSTREAM_INTERFACE_RESET,
	UPSTREAM_INTERFACE_WAITING_CLIENT,
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
} InterfaceStateTypeDef;


typedef enum
{
	NOT_BUSY,
	BUSY
} PacketBusyTypeDef;


typedef struct
{
	PacketBusyTypeDef	Busy;						//Everything after Busy should be word-aligned
	uint16_t			Length __ALIGN_END;			//Packet length includes CommandClass, Command, and Data
	uint8_t				CommandClass;
	uint8_t				Command;
	uint8_t				Data[MSC_MEDIA_PACKET];		//Should (must?) be word-aligned, for USB copy routine
	uint8_t				RxCrc;
}
UpstreamPacketTypeDef;



typedef void (*FreePacketCallbackTypeDef)(UpstreamPacketTypeDef* freePacket);
typedef void (*SpiPacketReceivedCallbackTypeDef)(UpstreamPacketTypeDef* replyPacket);


void Upstream_InitSPI(void);
HAL_StatusTypeDef Upstream_GetFreePacket(FreePacketCallbackTypeDef callback);
UpstreamPacketTypeDef* Upstream_GetFreePacketImmediately(void);
void Upstream_ReleasePacket(UpstreamPacketTypeDef* packetToRelease);
HAL_StatusTypeDef Upstream_SendPacket(UpstreamPacketTypeDef* packetToWrite);
HAL_StatusTypeDef Upstream_GetPacket(SpiPacketReceivedCallbackTypeDef callback);
void Upstream_TxOkInterrupt(void);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);


#endif /* INC_UPSTREAM_SPI_H_ */
