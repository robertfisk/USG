/*
 * upstream_spi.h
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_SPI_H_
#define INC_UPSTREAM_SPI_H_


#include "usbh_config.h"


#define UPSTREAM_PACKET_HEADER_LEN	(2)			//Min length = CommandClass & Command bytes
#define UPSTREAM_PACKET_LEN			(UPSTREAM_PACKET_HEADER_LEN + USBH_MAX_DATA_BUFFER)
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
	INTERFACE_STATE_RESET				= 0,
//	INTERFACE_STATE_WAITING_CLIENT		= 1,
//	INTERFACE_STATE_IDLE				= 2,
//	INTERFACE_STATE_TX_SIZE_WAIT		= 3,
//	INTERFACE_STATE_TX_SIZE				= 4,
//	INTERFACE_STATE_TX_PACKET_WAIT		= 5,
//	INTERFACE_STATE_TX_PACKET			= 6,
//	INTERFACE_STATE_RX_SIZE_WAIT		= 7,
//	INTERFACE_STATE_RX_SIZE				= 8,
//	INTERFACE_STATE_RX_PACKET_WAIT		= 9,
//	INTERFACE_STATE_RX_PACKET			= 10,
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
	uint8_t				Data[USBH_MAX_DATA_BUFFER];	//Should (must?) be word-aligned, for USB copy routine
	uint8_t				RxCrc;
}
UpstreamPacketTypeDef;


void Upstream_InitInterface(void);




#endif /* INC_UPSTREAM_SPI_H_ */
