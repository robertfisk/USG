/*
 * upstream_msc.c
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 */


#include <upstream_interface_def.h>
#include <upstream_msc.h>
#include <upstream_spi.h>
#include "stm32f4xx_hal.h"


//Stuff we need to save for our callbacks to use:
UpstreamMSCCallbackTypeDef 				TestReadyCallback;
UpstreamMSCCallbackUintPacketTypeDef	GetCapacityCallback;
UpstreamMSCCallbackPacketTypeDef		GetStreamDataCallback;
uint32_t								ByteCount;
UpstreamPacketTypeDef*					ReadStreamPacket;
uint8_t									ReadStreamBusy;


static void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_BeginWriteReplyCallback(UpstreamPacketTypeDef* replyPacket);



HAL_StatusTypeDef Upstream_MSC_TestReady(UpstreamMSCCallbackTypeDef callback)
{
	UpstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	TestReadyCallback = callback;
	freePacket = Upstream_GetFreePacketImmediately();

	freePacket->Length = UPSTREAM_PACKET_HEADER_LEN;
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_TEST_UNIT_READY;
	tempResult = Upstream_TransmitPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		return tempResult;
	}
	return Upstream_ReceivePacket(Upstream_MSC_TestReadyReplyCallback);
}


void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
	if ((replyPacket->Length != (UPSTREAM_PACKET_HEADER_LEN + 1)) ||
		(replyPacket->CommandClass != COMMAND_CLASS_MASS_STORAGE) ||
		(replyPacket->Data[0] != HAL_OK))
	{
		Upstream_ReleasePacket(replyPacket);
		TestReadyCallback(HAL_ERROR);
		return;
	}

	Upstream_ReleasePacket(replyPacket);
	TestReadyCallback(HAL_OK);
}



HAL_StatusTypeDef Upstream_MSC_GetCapacity(UpstreamMSCCallbackUintPacketTypeDef callback)
{
	UpstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	GetCapacityCallback = callback;
	freePacket = Upstream_GetFreePacketImmediately();

	freePacket->Length = UPSTREAM_PACKET_HEADER_LEN;
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_GET_CAPACITY;
	tempResult = Upstream_TransmitPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		return tempResult;
	}
	return Upstream_ReceivePacket(Upstream_MSC_GetCapacityReplyCallback);
}


void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
	uint32_t uint[2];

	if ((replyPacket->Length != (UPSTREAM_PACKET_HEADER_LEN + 8) ||
	    (replyPacket->CommandClass != COMMAND_CLASS_MASS_STORAGE)))
	{
		GetCapacityCallback(HAL_ERROR, NULL, NULL);
		return;
	}

	uint[0] = *(uint32_t*)&(replyPacket->Data[0]);
	uint[1] = *(uint32_t*)&(replyPacket->Data[4]);
	GetCapacityCallback(HAL_OK, uint, replyPacket);		//usb_msc_scsi will use this packet, so don't release now
}



HAL_StatusTypeDef Upstream_MSC_BeginRead(UpstreamMSCCallbackTypeDef callback,
										 uint64_t readBlockStart,
										 uint32_t readBlockCount,
										 uint32_t readByteCount)
{
	UpstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	ReadStreamPacket = NULL;			//Prepare for GetStreamDataPacket's use
	ReadStreamBusy = 0;
	ByteCount = readByteCount;

	TestReadyCallback = callback;
	freePacket = Upstream_GetFreePacketImmediately();

	freePacket->Length = UPSTREAM_PACKET_HEADER_LEN + (4 * 3);
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_BEGIN_READ;
	*(uint64_t*)&(freePacket->Data[0]) = readBlockStart;
	*(uint32_t*)&(freePacket->Data[8]) = readBlockCount;

	tempResult = Upstream_TransmitPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		TestReadyCallback(tempResult);
	}
	return Upstream_ReceivePacket(Upstream_MSC_TestReadyReplyCallback);	//Re-use TestReadyReplyCallback because it does exactly what we want!
}



HAL_StatusTypeDef Upstream_MSC_GetStreamDataPacket(UpstreamMSCCallbackPacketTypeDef callback)
{
	GetStreamDataCallback = callback;

	if (ReadStreamBusy != 0)
	{
		return HAL_OK;
	}
	ReadStreamBusy = 1;

	if (ReadStreamPacket && GetStreamDataCallback)		//Do we have a stored packet and an address to send it?
	{
		Upstream_MSC_GetStreamDataPacketCallback(ReadStreamPacket);	//Send it now!
		ReadStreamPacket = NULL;
		return HAL_OK;				//Our callback will call us again, so we don't need to get a packet in this case.
	}
	return Upstream_ReceivePacket(Upstream_MSC_GetStreamDataPacketCallback);
}


void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket)
{
	uint16_t dataLength;

	ReadStreamBusy = 0;
	if (GetStreamDataCallback == NULL)
	{
		ReadStreamPacket = replyPacket;		//We used up our callback already, so save this one for later.
		return;
	}

	dataLength = replyPacket->Length - UPSTREAM_PACKET_HEADER_LEN;

	if (((replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) == 0) ||		//Any 'command' reply (as opposed to 'data' reply) is an automatic fail here
		 (replyPacket->Length <= UPSTREAM_PACKET_HEADER_LEN) ||				//Should be at least one data byte in the reply.
		 (dataLength > ByteCount))											//No more data than expected transfer length
	{
		GetStreamDataCallback(HAL_ERROR, NULL, NULL);
		return;
	}

	ByteCount -= dataLength;
	GetStreamDataCallback(HAL_OK, replyPacket, dataLength);	//usb_msc_scsi will use this packet, so don't release now
	if (ByteCount > 0)
	{
		Upstream_MSC_GetStreamDataPacket(NULL);				//Try to get the next packet now, before USB asks for it
	}
}



HAL_StatusTypeDef Upstream_MSC_BeginWrite(UpstreamMSCCallbackTypeDef callback,
										  uint64_t readBlockStart,
										  uint32_t readBlockCount)
{
	UpstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	TestReadyCallback = callback;
	freePacket = Upstream_GetFreePacketImmediately();

	freePacket->Length = UPSTREAM_PACKET_HEADER_LEN + (4 * 3);
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_BEGIN_WRITE;
	*(uint64_t*)&(freePacket->Data[0]) = readBlockStart;
	*(uint32_t*)&(freePacket->Data[8]) = readBlockCount;

	tempResult = Upstream_TransmitPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		TestReadyCallback(tempResult);
	}
	return Upstream_ReceivePacket(Upstream_MSC_BeginWriteReplyCallback);
}


void Upstream_MSC_BeginWriteReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
	if ((replyPacket->Length != (UPSTREAM_PACKET_HEADER_LEN + 1)) ||
		(replyPacket->CommandClass != COMMAND_CLASS_MASS_STORAGE) ||
		((replyPacket->Data[0] != HAL_OK) && (replyPacket->Data[0] != HAL_BUSY)))
	{
		Upstream_ReleasePacket(replyPacket);
		TestReadyCallback(HAL_ERROR);
		return;
	}

	Upstream_ReleasePacket(replyPacket);
	TestReadyCallback(replyPacket->Data[0]);
}



HAL_StatusTypeDef Upstream_MSC_PutStreamDataPacket(UpstreamPacketTypeDef* packetToSend,
												   uint32_t dataLength)
{
	packetToSend->Length = dataLength + UPSTREAM_PACKET_HEADER_LEN;
	packetToSend->CommandClass = COMMAND_CLASS_MASS_STORAGE | COMMAND_CLASS_DATA_FLAG;
	packetToSend->Command = COMMAND_MSC_BEGIN_WRITE;
	return Upstream_TransmitPacket(packetToSend);
}

