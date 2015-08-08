/*
 * upstream_spi.c
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */


#include <downstream_interface_def.h>
#include <downstream_spi.h>
#include "board_config.h"


SPI_HandleTypeDef			Hspi1;
DownstreamPacketTypeDef		DownstreamPacket0;
DownstreamPacketTypeDef		DownstreamPacket1;
DownstreamPacketTypeDef*	CurrentWorkingPacket;

InterfaceStateTypeDef				DownstreamInterfaceState;
FreePacketCallbackTypeDef			PendingFreePacketCallback;	//Indicates someone is waiting for a packet buffer to become available
SpiPacketReceivedCallbackTypeDef	ReceivePacketCallback;		//Indicates someone is waiting for a received packet



void SPI1_Init(void);
HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void);
void Downstream_PreparePacketReception(DownstreamPacketTypeDef* freePacket);



void Downstream_InitSPI(void)
{
	SPI1_Init();

	DownstreamPacket0.Busy = NOT_BUSY;
	DownstreamPacket1.Busy = NOT_BUSY;
	//NextTxPacket = NULL;
	PendingFreePacketCallback = NULL;
	ReceivePacketCallback = NULL;

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
}


void SPI1_Init(void)
{
	  Hspi1.Instance = SPI1;
	  Hspi1.Init.Mode = SPI_MODE_SLAVE;
	  Hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	  Hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	  Hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	  Hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	  Hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
	  Hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	  Hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	  Hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
	  Hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
	  Hspi1.Init.CRCPolynomial = SPI_CRC_DEFAULTPOLYNOMIAL;
	  HAL_SPI_Init(&Hspi1);
}


//Used by...
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback)
{
	//Sanity checks
	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	//Do we already have a queued callback?
	if (PendingFreePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
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


//DownstreamPacketTypeDef* Downstream_GetFreePacketImmediately(void)
//{
//	//Sanity checks
//	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
//	{
//		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
//	}
//
//	//We are expecting a free buffer now
//	if (DownstreamPacket0.Busy == NOT_BUSY)
//	{
//		DownstreamPacket0.Busy = BUSY;
//		return &DownstreamPacket0;
//	}
//	if (DownstreamPacket1.Busy == NOT_BUSY)
//	{
//		DownstreamPacket1.Busy = BUSY;
//		return &DownstreamPacket1;
//	}
//
//	//Should not happen:
//	SPI_INTERFACE_FREAKOUT_NO_RETURN;
//}


//Used by...
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease)
{
	FreePacketCallbackTypeDef tempCallback;

	if ((packetToRelease != &DownstreamPacket0) &&
		(packetToRelease != &DownstreamPacket1))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

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



//Used by Downstream state machine (and USB classes?).
//Ok to call when idle or transmitting.
//Not OK to call when receiving or waiting for downstream reply.
HAL_StatusTypeDef Downstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback)
{
	if (ReceivePacketCallback != NULL)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}
	ReceivePacketCallback = callback;
	return Downstream_CheckPreparePacketReception();
}


//Internal use only
HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void)
{
	if (DownstreamInterfaceState > DOWNSTREAM_INTERFACE_TX_PACKET_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_IDLE)
	{
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_RX_SIZE_WAIT;
		return Downstream_GetFreePacket(Downstream_PreparePacketReception);
	}
	return HAL_OK;
}


//Internal use only
void Downstream_PreparePacketReception(DownstreamPacketTypeDef* freePacket)
{
	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_RX_SIZE_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
	CurrentWorkingPacket = freePacket;
	//CurrentWorkingPacket->Length = 0;
	//if (HAL_SPI_TransmitReceive_DMA(... ????
	if (HAL_SPI_Receive_DMA(&Hspi1,
							(uint8_t*)&CurrentWorkingPacket->Length,
							(2 + 1)) != HAL_OK)		//"When the CRC feature is enabled the pData Length must be Size + 1"
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	UPSTREAM_TX_REQUEST_ASSERT;
}


//Called at the end of the SPI RX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SpiPacketReceivedCallbackTypeDef tempPacketCallback;

	UPSTREAM_TX_REQUEST_DEASSERT;

	if ((DownstreamInterfaceState != DOWNSTREAM_INTERFACE_RX_SIZE_WAIT) &&
		(DownstreamInterfaceState != DOWNSTREAM_INTERFACE_RX_PACKET_WAIT))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_SIZE_WAIT)
	{
		if ((CurrentWorkingPacket->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
			(CurrentWorkingPacket->Length > DOWNSTREAM_PACKET_LEN))
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_RX_PACKET_WAIT;
		if ((HAL_SPI_Receive_DMA(&Hspi1,
								 &CurrentWorkingPacket->CommandClass,
								 CurrentWorkingPacket->Length + 1)) != HAL_OK)	//"When the CRC feature is enabled the pData Length must be Size + 1"
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		UPSTREAM_TX_REQUEST_ASSERT;
		return;
	}

	if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_PACKET_WAIT)
	{
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
		if (ReceivePacketCallback == NULL)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		//Packet processor may want to receive another packet immediately,
		//so clear ReceivePacketCallback before the call.
		//It is the callback's responsibility to release the packet buffer we are passing to it!
		tempPacketCallback = ReceivePacketCallback;
		ReceivePacketCallback = NULL;
		tempPacketCallback(CurrentWorkingPacket);
	}
}



//Used by Downstream state machine (and USB classes?).
//Call when idle only.
HAL_StatusTypeDef Downstream_TransmitPacket(DownstreamPacketTypeDef* packetToWrite)
{
	//Sanity checks
	if ((packetToWrite != &DownstreamPacket0) &&
		(packetToWrite != &DownstreamPacket1))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}
	if ((packetToWrite->Busy != BUSY) ||
		(packetToWrite->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
		(packetToWrite->Length > DOWNSTREAM_PACKET_LEN))
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_IDLE)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_HAL_ERROR;
	}

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_SIZE_WAIT;
	CurrentWorkingPacket = packetToWrite;
	if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
							 	 	(uint8_t*)&CurrentWorkingPacket->Length,
									(uint8_t*)&CurrentWorkingPacket->Length,
									2 + 1) != HAL_OK)		//"When the CRC feature is enabled the pRxData Length must be Size + 1"
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	UPSTREAM_TX_REQUEST_ASSERT;
	return HAL_OK;
}


//Called at the end of the SPI TxRx DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
//We use TxRx while sending our reply packet to check if Upstream was trying
//to send us a packet at the same time.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	UPSTREAM_TX_REQUEST_DEASSERT;

	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_TX_SIZE_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	if (CurrentWorkingPacket->Length != 0)
	{
		//Currently we just freak out if Upstream sends us an unexpected command.
		//Theoretically we could reset our downstream state machine and accept the new command...
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_PACKET_WAIT;
	if ((HAL_SPI_Transmit_DMA(&Hspi1,
							  &CurrentWorkingPacket->CommandClass,
							  CurrentWorkingPacket->Length)) != HAL_OK)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
	UPSTREAM_TX_REQUEST_ASSERT;
}


//Called at the end of the SPI TX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	UPSTREAM_TX_REQUEST_DEASSERT;

	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_TX_PACKET_WAIT)
	{
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
	Downstream_ReleasePacket(CurrentWorkingPacket);
	if (ReceivePacketCallback != NULL)
	{
		Downstream_CheckPreparePacketReception();
	}
}



//Something bad happened! Possibly CRC error...
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	SPI_INTERFACE_FREAKOUT_RETURN_VOID;
}

