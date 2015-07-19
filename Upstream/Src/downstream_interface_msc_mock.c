/*
 * downstream_interface_msc_mock.c
 *
 *  Created on: 17/07/2015
 *      Author: Robert Fisk
 *
 *      This file replaces downstream_interface_msc.c to allow operational
 *      testing of Upstream, without Downstream in place and communicating
 *      over SPI. It still attempts to write downstream packets out the SPI port.
 */


#include "stm32f4xx_hal.h"
#include "downstream_interface_msc.h"
#include "downstream_interface_def.h"
#include "downstream_spi.h"


//Stuff we need to save for our callbacks to use:
DownstreamInterfaceMSCCallbackTypeDef 			TestReadyCallback;
DownstreamInterfaceMSCCallbackUintPacketTypeDef	GetCapacityCallback;
DownstreamInterfaceMSCCallbackPacketTypeDef		GetStreamDataCallback;
uint64_t										BlockStart;
uint32_t										BlockCount;
uint32_t										ByteCount;
DownstreamPacketTypeDef*						ReadStreamPacket;
uint8_t											ReadStreamBusy;


static void DownstreamInterface_TestReadyReplyCallback(DownstreamPacketTypeDef* replyPacket);
static void DownstreamInterface_GetCapacityReplyCallback(DownstreamPacketTypeDef* replyPacket);
static void DownstreamInterface_GetStreamDataPacketCallback(DownstreamPacketTypeDef* replyPacket);
static void DownstreamInterface_BeginWriteReplyCallback(DownstreamPacketTypeDef* replyPacket);



HAL_StatusTypeDef DownstreamInterface_TestReady(DownstreamInterfaceMSCCallbackTypeDef callback)
{
	DownstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	TestReadyCallback = callback;
	freePacket = Downstream_GetFreePacketImmediately();

	freePacket->Length = DOWNSTREAM_PACKET_HEADER_LEN;
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_TEST_UNIT_READY;
	tempResult = Downstream_SendPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		return tempResult;
	}
	//return Downstream_GetPacket(DownstreamInterface_TestReadyReplyCallback);
	return Downstream_GetFreePacket(DownstreamInterface_TestReadyReplyCallback);
}

void DownstreamInterface_TestReadyReplyCallback(DownstreamPacketTypeDef* replyPacket)
{
//	if ((replyPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + 1)) ||
//		(replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) ||
//		(replyPacket->Data[0] != HAL_OK))
//	{
//		Downstream_ReleasePacket(replyPacket);
//		TestReadyCallback(HAL_ERROR);
//		return;
//	}

		Downstream_ReleasePacket(replyPacket);
		TestReadyCallback(HAL_OK);
}



HAL_StatusTypeDef DownstreamInterface_GetCapacity(DownstreamInterfaceMSCCallbackUintPacketTypeDef callback)
{
	DownstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	GetCapacityCallback = callback;
	freePacket = Downstream_GetFreePacketImmediately();

	freePacket->Length = DOWNSTREAM_PACKET_HEADER_LEN;
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_GET_CAPACITY;
	tempResult = Downstream_SendPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		return tempResult;
	}
	//return Downstream_GetPacket(DownstreamInterface_GetCapacityReplyCallback);
	return Downstream_GetFreePacket(DownstreamInterface_GetCapacityReplyCallback);
}

void DownstreamInterface_GetCapacityReplyCallback(DownstreamPacketTypeDef* replyPacket)
{
	uint32_t uint[2];

//	if ((replyPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + 8) ||
//	    (replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG)))
//	{
//		GetCapacityCallback(HAL_ERROR, NULL, NULL);
//	}

	*(uint32_t*)&(replyPacket->Data[0]) = 262144;		//* 512B = 128MB
	*(uint32_t*)&(replyPacket->Data[4]) = 512;

	uint[0] = *(uint32_t*)&(replyPacket->Data[0]);		/////////check indexing!!!
	uint[1] = *(uint32_t*)&(replyPacket->Data[4]);
	GetCapacityCallback(HAL_OK, uint, replyPacket);		//usb_msc_scsi will use this packet, so don't release now
}



HAL_StatusTypeDef DownstreamInterface_BeginRead(DownstreamInterfaceMSCCallbackTypeDef callback,
												uint64_t readBlockStart,
												uint32_t readBlockCount,
												uint32_t readByteCount)
{
	DownstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	ReadStreamPacket = NULL;			//Prepare for GetStreamDataPacket's use
	ReadStreamBusy = 0;

	TestReadyCallback = callback;
	BlockStart = readBlockStart;
	BlockCount = readBlockCount;
	ByteCount = readByteCount;
	freePacket = Downstream_GetFreePacketImmediately();

	freePacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + (4 * 3);
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_BEGIN_READ;
	*(uint64_t*)&(freePacket->Data[0]) = BlockStart;
	*(uint32_t*)&(freePacket->Data[8]) = BlockCount;

	tempResult = Downstream_SendPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		TestReadyCallback(tempResult);
	}
	//return Downstream_GetPacket(DownstreamInterface_TestReadyReplyCallback);	//Re-use TestReadyReplyCallback because it does exactly what we want!
	return Downstream_GetFreePacket(DownstreamInterface_TestReadyReplyCallback);
}



HAL_StatusTypeDef DownstreamInterface_GetStreamDataPacket(DownstreamInterfaceMSCCallbackPacketTypeDef callback)
{
	GetStreamDataCallback = callback;

	if (ReadStreamBusy != 0)
	{
		return HAL_OK;
	}
	ReadStreamBusy = 1;

	if (ReadStreamPacket && GetStreamDataCallback)		//Do we have a stored packet and an address to send it?
	{
		DownstreamInterface_GetStreamDataPacketCallback(ReadStreamPacket);	//Send it now!
		ReadStreamPacket = NULL;
		return HAL_OK;				//Our callback will call us again, so we don't need to get a packet in this case.
	}
	//return Downstream_GetPacket(DownstreamInterface_GetStreamDataPacketCallback);
	return Downstream_GetFreePacket(DownstreamInterface_GetStreamDataPacketCallback);
}

void DownstreamInterface_GetStreamDataPacketCallback(DownstreamPacketTypeDef* replyPacket)
{
	uint16_t dataLength;

	ReadStreamBusy = 0;
	if (GetStreamDataCallback == NULL)
	{
		ReadStreamPacket = replyPacket;		//We used up our callback already, so save this one for later.
		return;
	}

//	if (((replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) == 0) ||		//Any 'command' reply (as opposed to 'data' reply) is an automatic fail here
//		 (replyPacket->Length <= DOWNSTREAM_PACKET_HEADER_LEN) ||			//Should be at least one data byte in the reply.
//		 (replyPacket->Length > ByteCount))
//	{
//		GetStreamDataCallback(HAL_ERROR, NULL);
//		return;
//	}

	replyPacket->Length = MIN((ByteCount + DOWNSTREAM_PACKET_HEADER_LEN), (MSC_MEDIA_PACKET + DOWNSTREAM_PACKET_HEADER_LEN));

	dataLength = replyPacket->Length - DOWNSTREAM_PACKET_HEADER_LEN;
	ByteCount -= dataLength;
	GetStreamDataCallback(HAL_OK, replyPacket, dataLength);	//usb_msc_scsi will use this packet, so don't release now
	if (ByteCount > 0)
	{
		DownstreamInterface_GetStreamDataPacket(NULL);	//Try to get the next packet now, before USB asks for it
	}
}



HAL_StatusTypeDef DownstreamInterface_BeginWrite(DownstreamInterfaceMSCCallbackTypeDef callback,
												 uint64_t readBlockStart,
												 uint32_t readBlockCount)
{
	DownstreamPacketTypeDef* freePacket;
	HAL_StatusTypeDef tempResult;

	TestReadyCallback = callback;
	BlockStart = readBlockStart;
	BlockCount = readBlockCount;
	freePacket = Downstream_GetFreePacketImmediately();

	freePacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + (4 * 3);
	freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
	freePacket->Command = COMMAND_MSC_BEGIN_WRITE;
	*(uint64_t*)&(freePacket->Data[0]) = BlockStart;
	*(uint32_t*)&(freePacket->Data[8]) = BlockCount;

	tempResult = Downstream_SendPacket(freePacket);
	if (tempResult != HAL_OK)
	{
		TestReadyCallback(tempResult);
	}
	//return Downstream_GetPacket(DownstreamInterface_BeginWriteReplyCallback);
	return Downstream_GetFreePacket(DownstreamInterface_BeginWriteReplyCallback);
}

void DownstreamInterface_BeginWriteReplyCallback(DownstreamPacketTypeDef* replyPacket)
{
//	if ((replyPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + 1)) ||
//		(replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) ||
//		((replyPacket->Data[0] != HAL_OK) && (replyPacket->Data[0] != HAL_BUSY)))
//	{
//		Downstream_ReleasePacket(replyPacket);
//		TestReadyCallback(HAL_ERROR);
//		return;
//	}

	Downstream_ReleasePacket(replyPacket);
	TestReadyCallback(replyPacket->Data[0]);
}



HAL_StatusTypeDef DownstreamInterface_PutStreamDataPacket(DownstreamPacketTypeDef* packetToSend,
														  uint32_t dataLength)
{
	packetToSend->Length = dataLength + DOWNSTREAM_PACKET_HEADER_LEN;
	packetToSend->CommandClass = COMMAND_CLASS_MASS_STORAGE | COMMAND_CLASS_DATA_FLAG;
	packetToSend->Command = COMMAND_MSC_BEGIN_WRITE;
	return Downstream_SendPacket(packetToSend);
}
