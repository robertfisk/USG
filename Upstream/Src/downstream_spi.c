/*
 * downstream_spi.c
 *
 *  Created on: 21/06/2015
 *      Author: Robert Fisk
 */

#include <downstream_spi.h>
#include "downstream_interface_def.h"
#include "stm32f4xx_hal.h"
#include "usbd_def.h"
#include "board_config.h"



SPI_HandleTypeDef			hspi1;
DownstreamPacketTypeDef		DownstreamPacket0;
DownstreamPacketTypeDef		DownstreamPacket1;
DownstreamPacketTypeDef*	CurrentWorkingPacket;
DownstreamPacketTypeDef*	NextTxPacket;			//Indicates we have a pending TX packet

InterfaceStateTypeDef				DownstreamInterfaceState;
FreePacketCallbackTypeDef			PendingFreePacketCallback;	//Indicates someone is waiting for a packet buffer to become available
SpiPacketReceivedCallbackTypeDef	ReceivePacketCallback;		//Indicates someone is waiting for a received packet

uint8_t						SentCommandClass;
uint8_t						SentCommand;


static void SPI1_Init(void);
static HAL_StatusTypeDef Downstream_CheckBeginPacketReception(void);
static void Downstream_BeginPacketReception(DownstreamPacketTypeDef* freePacket);



void Downstream_InitInterface(void)
{
	DownstreamInterfaceState = INTERFACE_STATE_RESET;

	SPI1_Init();
	DownstreamPacket0.Busy = NOT_BUSY;
	DownstreamPacket1.Busy = NOT_BUSY;
	NextTxPacket = NULL;
	PendingFreePacketCallback = NULL;
	ReceivePacketCallback = NULL;

	//Todo: check connection to downstream, await client USB insertion

	while (!DOWNSTREAM_TX_OK_ACTIVE);
	DownstreamInterfaceState = INTERFACE_STATE_IDLE;
}


void SPI1_Init(void)
{
	hspi1.Instance = SPI1;
	hspi1.State = HAL_SPI_STATE_RESET;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;	//42MHz APB2 / 32 = 1.3Mbaud
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLED;
	hspi1.Init.CRCPolynomial = SPI_CRC_DEFAULTPOLYNOMIAL;
	HAL_SPI_Init(&hspi1);
}



//Used by USB interface classes, and by our internal RX code.
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback)
{
	//Sanity checks
	if 	((DownstreamInterfaceState < INTERFACE_STATE_IDLE) ||
		 (DownstreamInterfaceState > INTERFACE_STATE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	//Do we already have a queued callback?
	if (PendingFreePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	//Check if there is a free buffer now
	if (DownstreamPacket0.Busy == NOT_BUSY)
	{
		DownstreamPacket0.Busy = BUSY;
		callback(&DownstreamPacket0);
		return HAL_OK;
	}
	if (DownstreamPacket1.Busy == NOT_BUSY)
	{
		DownstreamPacket1.Busy = BUSY;
		callback(&DownstreamPacket1);
		return HAL_OK;
	}

	//Otherwise save requested address for when a buffer becomes free in the future
	PendingFreePacketCallback = callback;
	return HAL_OK;
}


DownstreamPacketTypeDef* Downstream_GetFreePacketImmediately(void)
{
	//Sanity checks
	if 	((DownstreamInterfaceState < INTERFACE_STATE_IDLE) ||
		 (DownstreamInterfaceState > INTERFACE_STATE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	//We are expecting a free buffer now
	if (DownstreamPacket0.Busy == NOT_BUSY)
	{
		DownstreamPacket0.Busy = BUSY;
		return &DownstreamPacket0;
	}
	if (DownstreamPacket1.Busy == NOT_BUSY)
	{
		DownstreamPacket1.Busy = BUSY;
		return &DownstreamPacket1;
	}

	//Should not happen:
	SPI_INTERFACE_FREAKOUT_HAL_ERROR;
}


//Used by USB interface classes, and by our internal RX code.
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease)
{
	FreePacketCallbackTypeDef tempCallback;

	if (PendingFreePacketCallback != NULL)
	{
		tempCallback = PendingFreePacketCallback;	//In extreme situations, running this callback can trigger another request for a free packet,
		PendingFreePacketCallback = NULL;			//thereby causing GetFreePacket to freak out. So we need to clear the callback indicator first.
		tempCallback(packetToRelease);
	}
	else
	{
		packetToRelease->Busy = NOT_BUSY;
	}
}


//Used by USB interface classes only.
//OK to call when still transmitting another packet.
//Not OK to call when receiving or waiting for downstream reply.
HAL_StatusTypeDef Downstream_SendPacket(DownstreamPacketTypeDef* packetToWrite)
{
	//Sanity checks
	if ((packetToWrite != &DownstreamPacket0) &&
		(packetToWrite != &DownstreamPacket1))
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}
	if ((packetToWrite->Busy != BUSY) ||
		(packetToWrite->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
		(packetToWrite->Length > DOWNSTREAM_PACKET_LEN))
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	//Cancel any outstanding receive request
	ReceivePacketCallback = NULL;

	switch (DownstreamInterfaceState)
	{
	case INTERFACE_STATE_TX_SIZE_WAIT:
	case INTERFACE_STATE_TX_SIZE:
	case INTERFACE_STATE_TX_PACKET_WAIT:
	case INTERFACE_STATE_TX_PACKET:
		if (NextTxPacket != NULL)
		{
			SPI_INTERFACE_FREAKOUT_HAL_ERROR;
		}
		NextTxPacket = packetToWrite;
		break;

	case INTERFACE_STATE_RX_SIZE_WAIT:
	case INTERFACE_STATE_RX_SIZE:
	case INTERFACE_STATE_RX_PACKET_WAIT:
	case INTERFACE_STATE_RX_PACKET:
		//We can't let the size/packet sequence get out of sync.
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;

	case INTERFACE_STATE_IDLE:
		DownstreamInterfaceState = INTERFACE_STATE_TX_SIZE_WAIT;
		CurrentWorkingPacket = packetToWrite;
		SentCommandClass = CurrentWorkingPacket->CommandClass;
		SentCommand = CurrentWorkingPacket->Command;
		if (DOWNSTREAM_TX_OK_ACTIVE)
		{
			Downstream_TxOkInterrupt();		//Manually trigger edge interrupt processing if the line was already asserted
		}
		break;

	default:
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}
	return HAL_OK;
}



//Called at the end of the SPI TX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SPI1_NSS_DEASSERT;

	if ((DownstreamInterfaceState != INTERFACE_STATE_TX_SIZE) &&
		(DownstreamInterfaceState != INTERFACE_STATE_TX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_VOID;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_TX_SIZE)
	{
		DownstreamInterfaceState = INTERFACE_STATE_TX_PACKET_WAIT;
		if (DOWNSTREAM_TX_OK_ACTIVE)
		{
			Downstream_TxOkInterrupt();
		}
		return;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_TX_PACKET)
	{
		if ((PendingFreePacketCallback != NULL) && (NextTxPacket == NULL))
		{
			//SPI_INTERFACE_FREAKOUT_VOID;		///////////////////////////////////////!
		}

		Downstream_ReleasePacket(CurrentWorkingPacket);
		if (NextTxPacket != NULL)
		{
			//NextTxPacket has already passed the checks in SendDownstreamPacket.
			//So we just need to pass it to HAL_SPI_Transmit_DMA.
			DownstreamInterfaceState = INTERFACE_STATE_TX_SIZE_WAIT;
			CurrentWorkingPacket = NextTxPacket;
			NextTxPacket = NULL;
			if (DOWNSTREAM_TX_OK_ACTIVE)
			{
				Downstream_TxOkInterrupt();
			}
			return;
		}

		DownstreamInterfaceState = INTERFACE_STATE_IDLE;
		if (ReceivePacketCallback != NULL)
		{
			Downstream_CheckBeginPacketReception();
		}
	}
}


//Used by USB interface classes.
//Ok to call when transmitting, receiving, or waiting for downstream.
HAL_StatusTypeDef Downstream_GetPacket(SpiPacketReceivedCallbackTypeDef callback)
{
	if (ReceivePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	ReceivePacketCallback = callback;
	return Downstream_CheckBeginPacketReception();
}


//Internal use only.
HAL_StatusTypeDef Downstream_CheckBeginPacketReception(void)
{
	if ((DownstreamInterfaceState < INTERFACE_STATE_IDLE) ||
		(DownstreamInterfaceState > INTERFACE_STATE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_HAL_ERROR;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_IDLE)
	{
		DownstreamInterfaceState = INTERFACE_STATE_RX_SIZE_WAIT;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_RX_SIZE_WAIT)
	{
		if (DOWNSTREAM_TX_OK_ACTIVE)
		{
			//DownstreamTxOkInterrupt();
			Downstream_GetFreePacket(Downstream_BeginPacketReception);		//Take a shortcut here :)
		}
	}
	return HAL_OK;
}


//This is called by EXTI3 falling edge interrupt,
//indicating that downstream is ready for next transaction.
void Downstream_TxOkInterrupt(void)
{
	switch (DownstreamInterfaceState)
	{
	case INTERFACE_STATE_TX_SIZE_WAIT:
		DownstreamInterfaceState = INTERFACE_STATE_TX_SIZE;
		SPI1_NSS_ASSERT;
		if (HAL_SPI_Transmit_DMA(&hspi1,
								 (uint8_t*)&CurrentWorkingPacket->Length,
								 2) != HAL_OK)
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		break;

	case INTERFACE_STATE_TX_PACKET_WAIT:
		DownstreamInterfaceState = INTERFACE_STATE_TX_PACKET;
		SPI1_NSS_ASSERT;
		if ((HAL_SPI_Transmit_DMA(&hspi1,
								  &CurrentWorkingPacket->CommandClass,
								  CurrentWorkingPacket->Length)) != HAL_OK)
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		break;

	case INTERFACE_STATE_RX_SIZE_WAIT:
		Downstream_GetFreePacket(Downstream_BeginPacketReception);
		break;

	case INTERFACE_STATE_RX_PACKET_WAIT:
		DownstreamInterfaceState = INTERFACE_STATE_RX_PACKET;
		SPI1_NSS_ASSERT;
		if ((HAL_SPI_Receive_DMA(&hspi1,
								 &CurrentWorkingPacket->CommandClass,
								 (CurrentWorkingPacket->Length + 1))) != HAL_OK)	//"When the CRC feature is enabled the pData Length must be Size + 1"
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		break;

	default:
		SPI_INTERFACE_FREAKOUT_VOID;
	}
}


//Internal use only.
//Called when we want to receive downstream packet, and a packet buffer has become free.
void Downstream_BeginPacketReception(DownstreamPacketTypeDef* freePacket)
{
	if (DownstreamInterfaceState != INTERFACE_STATE_RX_SIZE_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_VOID;
	}
	DownstreamInterfaceState = INTERFACE_STATE_RX_SIZE;
	CurrentWorkingPacket = freePacket;
	CurrentWorkingPacket->Length = 0;		//Our RX buffer is used by HAL_SPI_Receive_DMA as dummy TX data, we set Length to 0 so downstream will know this is a dummy packet.
	SPI1_NSS_ASSERT;
	if (HAL_SPI_Receive_DMA(&hspi1,
							(uint8_t*)&CurrentWorkingPacket->Length,
							(2 + 1)) != HAL_OK)		//"When the CRC feature is enabled the pData Length must be Size + 1"
	{
		SPI_INTERFACE_FREAKOUT_VOID;
	}
}


//Called at the end of the SPI TX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SpiPacketReceivedCallbackTypeDef tempPacketCallback;

	SPI1_NSS_DEASSERT;

	if ((DownstreamInterfaceState != INTERFACE_STATE_RX_SIZE) &&
		(DownstreamInterfaceState != INTERFACE_STATE_RX_PACKET))
	{
		SPI_INTERFACE_FREAKOUT_VOID;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_RX_SIZE)
	{
		if ((CurrentWorkingPacket->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
			(CurrentWorkingPacket->Length > DOWNSTREAM_PACKET_LEN))
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		DownstreamInterfaceState = INTERFACE_STATE_RX_PACKET_WAIT;
		if (DOWNSTREAM_TX_OK_ACTIVE)
		{
			Downstream_TxOkInterrupt();
		}
		return;
	}

	if (DownstreamInterfaceState == INTERFACE_STATE_RX_PACKET)
	{
		DownstreamInterfaceState = INTERFACE_STATE_IDLE;
		if ((SentCommandClass != (CurrentWorkingPacket->CommandClass & COMMAND_CLASS_MASK)) ||
			(SentCommand != CurrentWorkingPacket->Command))
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		if (ReceivePacketCallback == NULL)
		{
			SPI_INTERFACE_FREAKOUT_VOID;
		}
		//USB interface may want to receive another packet immediately,
		//so clear ReceivePacketCallback before the call.
		//It is the callback's responsibility to release the packet buffer we are passing to it!
		tempPacketCallback = ReceivePacketCallback;
		ReceivePacketCallback = NULL;
		tempPacketCallback(CurrentWorkingPacket);
	}
}


//Something bad happened! Possibly CRC error...
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	SPI_INTERFACE_FREAKOUT_VOID;
}
