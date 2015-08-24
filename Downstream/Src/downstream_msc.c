/*
 * downstream_msc.c
 *
 *  Created on: 8/08/2015
 *      Author: Robert Fisk
 */


#include "downstream_msc.h"
#include "downstream_interface_def.h"
#include "downstream_statemachine.h"
#include "downstream_spi.h"
#include "usbh_msc.h"


extern USBH_HandleTypeDef hUsbHostFS;		//Hard-link ourselves to usb_host.c


//Stuff we need to save for our callbacks to use:
DownstreamMSCCallbackPacketTypeDef	GetStreamDataCallback;
uint32_t							ByteCount;
DownstreamPacketTypeDef*			ReadStreamPacket;
uint8_t								ReadStreamBusy;


void Downstream_MSC_PacketProcessor_TestUnitReady(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_GetCapacity(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_BeginRead(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_BeginWrite(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_RdWrCompleteCallback(USBH_StatusTypeDef result);
void Downstream_MSC_GetStreamDataPacketCallback(DownstreamPacketTypeDef* replyPacket);


//High-level checks on the connected device. We don't want some weirdly
//configured device to bomb our USB stack, accidentally or otherwise.
HAL_StatusTypeDef Downstream_MSC_ApproveConnectedDevice(void)
{
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	if (MSC_Handle->unit[MSC_FIXED_LUN].error != MSC_OK)
	{
		return HAL_ERROR;
	}

	if ((MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr == 0) ||
		(MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr == UINT32_MAX))
	{
		return HAL_ERROR;
	}

	if (MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_size != MSC_SUPPORTED_BLOCK_SIZE)
	{
		return HAL_ERROR;
	}

	return HAL_OK;
}


void Downstream_MSC_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
	switch (receivedPacket->Command)
	{
	case COMMAND_MSC_TEST_UNIT_READY:
		Downstream_MSC_PacketProcessor_TestUnitReady(receivedPacket);
		break;

	case COMMAND_MSC_GET_CAPACITY:
		Downstream_MSC_PacketProcessor_GetCapacity(receivedPacket);
		break;

	case COMMAND_MSC_BEGIN_READ:
		Downstream_MSC_PacketProcessor_BeginRead(receivedPacket);
		break;

	case COMMAND_MSC_BEGIN_WRITE:
		Downstream_MSC_PacketProcessor_BeginWrite(receivedPacket);
		break;

	default:
		Downstream_PacketProcessor_FreakOut();
	}

}


void Downstream_MSC_PacketProcessor_TestUnitReady(DownstreamPacketTypeDef* receivedPacket)
{
	if (USBH_MSC_UnitIsReady(&hUsbHostFS, MSC_FIXED_LUN))
	{
		receivedPacket->Data[0] = HAL_OK;
	}
	else
	{
		receivedPacket->Data[0] = HAL_ERROR;
	}
	receivedPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	Downstream_PacketProcessor_ClassReply(receivedPacket);
}


void Downstream_MSC_PacketProcessor_GetCapacity(DownstreamPacketTypeDef* receivedPacket)
{
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	receivedPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 8;
	*(uint32_t*)&(receivedPacket->Data[0]) = MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr;
	*(uint32_t*)&(receivedPacket->Data[4]) = (uint32_t)MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_size;
	Downstream_PacketProcessor_ClassReply(receivedPacket);
}



void Downstream_MSC_PacketProcessor_BeginRead(DownstreamPacketTypeDef* receivedPacket)
{
	uint64_t readBlockAddress;
	uint32_t readBlockCount;
	uint64_t readByteCount;
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	if (receivedPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + (4 * 3)))
	{
		Downstream_PacketProcessor_FreakOut();
		return;
	}

	readBlockAddress = *(uint64_t*)&(receivedPacket->Data[0]);
	readBlockCount   = *(uint32_t*)&(receivedPacket->Data[8]);
	readByteCount    = readBlockCount * MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_size;
	if ((readBlockAddress						 >= (uint64_t)MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr) ||
		((readBlockAddress + readBlockCount - 1) >= (uint64_t)MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr) ||
		(readByteCount > UINT32_MAX))
	{
		Downstream_PacketProcessor_FreakOut();
		return;
	}

	receivedPacket->Data[0] = HAL_ERROR;
	receivedPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	if (USBH_MSC_UnitIsReady(&hUsbHostFS, MSC_FIXED_LUN))
	{
		if (USBH_MSC_Read(&hUsbHostFS,
						  MSC_FIXED_LUN,
						  (uint32_t)readBlockAddress,
						  readBlockCount,
						  Downstream_MSC_PacketProcessor_RdWrCompleteCallback) == USBH_OK)
		{
			receivedPacket->Data[0] = HAL_OK;
		}
	}
	Downstream_TransmitPacket(receivedPacket);
}


void Downstream_MSC_PacketProcessor_RdWrCompleteCallback(USBH_StatusTypeDef result)
{
	if (result != USBH_OK)
	{
		Downstream_GetFreePacket(Downstream_PacketProcessor_GenericErrorReply);
		return;
	}
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}



void Downstream_MSC_PacketProcessor_BeginWrite(DownstreamPacketTypeDef* receivedPacket)
{
	uint64_t writeBlockAddress;
	uint32_t writeBlockCount;
	uint64_t writeByteCount;
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	if (receivedPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + (4 * 3)))
	{
		Downstream_PacketProcessor_FreakOut();
		return;
	}

	writeBlockAddress = *(uint64_t*)&(receivedPacket->Data[0]);
	writeBlockCount   = *(uint32_t*)&(receivedPacket->Data[8]);
	writeByteCount    = writeBlockCount * MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_size;
	if ((writeBlockAddress						   >= (uint64_t)MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr) ||
		((writeBlockAddress + writeBlockCount - 1) >= (uint64_t)MSC_Handle->unit[MSC_FIXED_LUN].capacity.block_nbr) ||
		(writeByteCount > UINT32_MAX))
	{
		Downstream_PacketProcessor_FreakOut();
		return;
	}

	ReadStreamPacket = NULL;		//Prepare for GetStreamDataPacket's use
	ReadStreamBusy = 0;
	ByteCount = (uint32_t)writeByteCount;

	receivedPacket->Data[0] = HAL_ERROR;
	receivedPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;

	//Our host stack has no way to detect if write-protection is enabled.
	//So currently we can't return HAL_BUSY to Upstream in this situation.
	if (USBH_MSC_UnitIsReady(&hUsbHostFS, MSC_FIXED_LUN))
	{
		if (USBH_MSC_Write(&hUsbHostFS,
						   MSC_FIXED_LUN,
						   (uint32_t)writeBlockAddress,
						   writeBlockCount,
						   Downstream_MSC_PacketProcessor_RdWrCompleteCallback) == USBH_OK)
		{
			receivedPacket->Data[0] = HAL_OK;
		}
	}
	Downstream_TransmitPacket(receivedPacket);
}


//Used by USB MSC host driver
HAL_StatusTypeDef Downstream_MSC_PutStreamDataPacket(DownstreamPacketTypeDef* packetToSend,
													 uint32_t dataLength)
{
	packetToSend->Length = dataLength + DOWNSTREAM_PACKET_HEADER_LEN;
	packetToSend->CommandClass = COMMAND_CLASS_MASS_STORAGE | COMMAND_CLASS_DATA_FLAG;
	packetToSend->Command = COMMAND_MSC_BEGIN_WRITE;
	return Downstream_TransmitPacket(packetToSend);
}


//Used by USB MSC host driver
HAL_StatusTypeDef Downstream_MSC_GetStreamDataPacket(DownstreamMSCCallbackPacketTypeDef callback)
{
	GetStreamDataCallback = callback;

	if (ReadStreamBusy != 0)
	{
		return HAL_OK;
	}
	ReadStreamBusy = 1;

	if (ReadStreamPacket && GetStreamDataCallback)		//Do we have a stored packet and an address to send it?
	{
		Downstream_MSC_GetStreamDataPacketCallback(ReadStreamPacket);	//Send it now!
		ReadStreamPacket = NULL;
		return HAL_OK;				//Our callback will call us again, so we don't need to get a packet in this case.
	}
	return Downstream_ReceivePacket(Downstream_MSC_GetStreamDataPacketCallback);
}


void Downstream_MSC_GetStreamDataPacketCallback(DownstreamPacketTypeDef* replyPacket)
{
	uint16_t dataLength;

	ReadStreamBusy = 0;
	if (GetStreamDataCallback == NULL)
	{
		ReadStreamPacket = replyPacket;		//We used up our callback already, so save this one for later.
		return;
	}

	if (((replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) == 0) ||		//Any incoming 'command' (as opposed to incoming 'data') is an automatic fail here
		 (replyPacket->Length <= DOWNSTREAM_PACKET_HEADER_LEN) ||			//Should be at least one data byte in the packet.
		 (replyPacket->Length > ByteCount))
	{
		Downstream_PacketProcessor_FreakOut();
		return;
	}

	dataLength = replyPacket->Length - DOWNSTREAM_PACKET_HEADER_LEN;
	ByteCount -= dataLength;
	GetStreamDataCallback(replyPacket, dataLength);	//usb_msc_scsi will use this packet, so don't release now
	if (ByteCount > 0)
	{
		Downstream_MSC_GetStreamDataPacket(NULL);	//Try to get the next packet now, before USB asks for it
	}

}

