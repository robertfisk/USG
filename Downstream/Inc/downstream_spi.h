/*
 * downstream_spi.h
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_SPI_H_
#define INC_DOWNSTREAM_SPI_H_


#include "usbh_config.h"


#define DOWNSTREAM_PACKET_HEADER_LEN	(2)			//Min length = CommandClass & Command bytes
#define DOWNSTREAM_PACKET_LEN			(DOWNSTREAM_PACKET_HEADER_LEN + USBH_MAX_DATA_BUFFER)
#define DOWNSTREAM_PACKET_LEN_MIN		(DOWNSTREAM_PACKET_HEADER_LEN)


#define DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID							\
	do {															\
		LED_Fault_SetBlinkRate(LED_FAST_BLINK_RATE);				\
		Downstream_PacketProcessor_SetErrorState();					\
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_ERROR;		\
		while (1);													\
		/*return;*/													\
	} while (0);

#define DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR					\
	do {															\
		LED_Fault_SetBlinkRate(LED_FAST_BLINK_RATE);				\
		Downstream_PacketProcessor_SetErrorState();					\
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_ERROR;		\
		while (1);													\
		/*return HAL_ERROR;*/										\
	} while (0);



typedef enum
{
	DOWNSTREAM_INTERFACE_IDLE,
	DOWNSTREAM_INTERFACE_RX_SIZE_WAIT,
	DOWNSTREAM_INTERFACE_RX_PACKET_WAIT,
	DOWNSTREAM_INTERFACE_TX_SIZE_WAIT,
	DOWNSTREAM_INTERFACE_TX_PACKET_WAIT,
	DOWNSTREAM_INTERFACE_ERROR
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
	uint16_t			Length __ALIGN_END;			//Packet length includes CommandClass, Command, and Data
	uint8_t				CommandClass;
	uint8_t				Command;
	uint8_t				Data[USBH_MAX_DATA_BUFFER];	//Should (must?) be word-aligned, for USB copy routine
	uint8_t				RxCrc;
}
DownstreamPacketTypeDef;



typedef void (*FreePacketCallbackTypeDef)(DownstreamPacketTypeDef* freePacket);
typedef void (*SpiPacketReceivedCallbackTypeDef)(DownstreamPacketTypeDef* receivedPacket);


void Downstream_InitSPI(void);
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback);
DownstreamPacketTypeDef* Downstream_GetFreePacketImmediately(void);
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease);
HAL_StatusTypeDef Downstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback);
HAL_StatusTypeDef Downstream_TransmitPacket(DownstreamPacketTypeDef* packetToWrite);

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);


#endif /* INC_DOWNSTREAM_SPI_H_ */
