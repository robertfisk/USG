/*
 * upstream_spi.c
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */


#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "downstream_statemachine.h"
#include "board_config.h"
#include "led.h"


SPI_HandleTypeDef			Hspi1;
DownstreamPacketTypeDef		DownstreamPacket0;
DownstreamPacketTypeDef		DownstreamPacket1;
DownstreamPacketTypeDef*	CurrentWorkingPacket;
DownstreamPacketTypeDef*	NextTxPacket			= NULL;

InterfaceStateTypeDef				DownstreamInterfaceState	= DOWNSTREAM_INTERFACE_IDLE;
FreePacketCallbackTypeDef			PendingFreePacketCallback	= NULL;	//Indicates someone is waiting for a packet buffer to become available
SpiPacketReceivedCallbackTypeDef	ReceivePacketCallback		= NULL;	//Indicates someone is waiting for a received packet



void SPI1_Init(void);
HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void);
void Downstream_PreparePacketReception(DownstreamPacketTypeDef* freePacket);



void Downstream_InitSPI(void)
{
	SPI1_Init();

	DownstreamPacket0.Busy = NOT_BUSY;
	DownstreamPacket1.Busy = NOT_BUSY;
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


//Used by downstream state machine and USB host classes.
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback)
{
	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return HAL_ERROR;
	}

	//Do we already have a queued callback?
	if (PendingFreePacketCallback != NULL)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
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


//Used by Downstream state machine and USB host classes.
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease)
{
	FreePacketCallbackTypeDef tempCallback;

	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return;
	}

	if ((packetToRelease != &DownstreamPacket0) &&
		(packetToRelease != &DownstreamPacket1))
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
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



//Used by Downstream state machine and USB classes.
//Ok to call when idle or transmitting.
//Not OK to call when receiving or awaiting reception.
HAL_StatusTypeDef Downstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback)
{
	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return HAL_ERROR;
	}

	if (ReceivePacketCallback != NULL)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
	}
	ReceivePacketCallback = callback;
	return Downstream_CheckPreparePacketReception();
}


//Internal use only
HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void)
{
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
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}
	CurrentWorkingPacket = freePacket;
	//CurrentWorkingPacket->Length = 0;
	//if (HAL_SPI_TransmitReceive_DMA(... ????
	if (HAL_SPI_Receive_DMA(&Hspi1,
							(uint8_t*)&CurrentWorkingPacket->Length,
							(2 + 1)) != HAL_OK)		//"When the CRC feature is enabled the pData Length must be Size + 1"
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}

	UPSTREAM_TX_REQUEST_ASSERT;
}


//Called at the end of the SPI RX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	SpiPacketReceivedCallbackTypeDef tempPacketCallback;

	UPSTREAM_TX_REQUEST_DEASSERT;

	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return;
	}

	if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_SIZE_WAIT)
	{
		if ((CurrentWorkingPacket->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
			(CurrentWorkingPacket->Length > DOWNSTREAM_PACKET_LEN))
		{
			DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
		}
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_RX_PACKET_WAIT;
		if ((HAL_SPI_Receive_DMA(&Hspi1,
								 &CurrentWorkingPacket->CommandClass,
								 CurrentWorkingPacket->Length + 1)) != HAL_OK)	//"When the CRC feature is enabled the pData Length must be Size + 1"
		{
			DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
		}
		UPSTREAM_TX_REQUEST_ASSERT;
		return;
	}

	if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_PACKET_WAIT)
	{
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
		if (ReceivePacketCallback == NULL)
		{
			DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
		}
		//Packet processor may want to receive another packet immediately,
		//so clear ReceivePacketCallback before the call.
		//It is the callback's responsibility to release the packet buffer we are passing to it!
		tempPacketCallback = ReceivePacketCallback;
		ReceivePacketCallback = NULL;
		tempPacketCallback(CurrentWorkingPacket);
		return;
	}

	DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
}



//Used by Downstream state machine (and USB classes?).
//Call when idle or transmitting.
//It doesn't make sense to call when receiving or awaiting reception.
HAL_StatusTypeDef Downstream_TransmitPacket(DownstreamPacketTypeDef* packetToWrite)
{
	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return HAL_ERROR;
	}

	//Sanity checks
	if ((packetToWrite != &DownstreamPacket0) &&
		(packetToWrite != &DownstreamPacket1))
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
	}
	if ((packetToWrite->Busy != BUSY) ||
		(packetToWrite->Length < DOWNSTREAM_PACKET_LEN_MIN) ||
		(packetToWrite->Length > DOWNSTREAM_PACKET_LEN))
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
	}
	if (NextTxPacket != NULL)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
	}

	switch (DownstreamInterfaceState)
	{
	case DOWNSTREAM_INTERFACE_TX_SIZE_WAIT:
	case DOWNSTREAM_INTERFACE_TX_PACKET_WAIT:
		NextTxPacket = packetToWrite;
		break;

	case DOWNSTREAM_INTERFACE_IDLE:
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_SIZE_WAIT;
		CurrentWorkingPacket = packetToWrite;
		if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
										(uint8_t*)&CurrentWorkingPacket->Length,
										(uint8_t*)&CurrentWorkingPacket->Length,
										2 + 1) != HAL_OK)		//"When the CRC feature is enabled the pRxData Length must be Size + 1"
		{
			DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
		}
		UPSTREAM_TX_REQUEST_ASSERT;
		break;

	default:
		DOWNSTREAM_SPI_FREAKOUT_RETURN_HAL_ERROR;
	}

	return HAL_OK;
}


//Called at the end of the SPI TxRx DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
//We use TxRx to send our reply packet to check if Upstream was trying
//to send us a packet at the same time.
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	UPSTREAM_TX_REQUEST_DEASSERT;

	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return;
	}

	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_TX_SIZE_WAIT)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}

	if (CurrentWorkingPacket->Length != 0)
	{
		//Currently we just freak out if Upstream sends us an unexpected command.
		//Theoretically we could reset our downstream state machine and accept the new command...
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_PACKET_WAIT;
	if ((HAL_SPI_Transmit_DMA(&Hspi1,
							  &CurrentWorkingPacket->CommandClass,
							  CurrentWorkingPacket->Length)) != HAL_OK)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}
	UPSTREAM_TX_REQUEST_ASSERT;
}


//Called at the end of the SPI TX DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	UPSTREAM_TX_REQUEST_DEASSERT;

	if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
	{
		return;
	}

	if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_TX_PACKET_WAIT)
	{
		DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
	}

	Downstream_ReleasePacket(CurrentWorkingPacket);
	if (NextTxPacket != NULL)
	{
		//NextTxPacket has already passed the checks in Downstream_TransmitPacket.
		//So we just need to pass it to HAL_SPI_Transmit_DMA.
		DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_SIZE_WAIT;
		CurrentWorkingPacket = NextTxPacket;
		NextTxPacket = NULL;
		if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
										(uint8_t*)&CurrentWorkingPacket->Length,
										(uint8_t*)&CurrentWorkingPacket->Length,
										2 + 1) != HAL_OK)		//"When the CRC feature is enabled the pRxData Length must be Size + 1"
		{
			DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
		}
		UPSTREAM_TX_REQUEST_ASSERT;
		return;
	}

	DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
	if (ReceivePacketCallback != NULL)
	{
		Downstream_CheckPreparePacketReception();
	}
}



//Something bad happened! Possibly CRC error...
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
	DOWNSTREAM_SPI_FREAKOUT_RETURN_VOID;
}

