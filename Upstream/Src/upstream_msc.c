/*
 * upstream_msc.c
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "upstream_interface_def.h"
#include "upstream_msc.h"
#include "upstream_spi.h"
#include "upstream_statemachine.h"
#include "stm32f4xx_hal.h"


//Stuff we need to save for our callbacks to use:
UpstreamMSCCallbackTypeDef              TestReadyCallback;
UpstreamMSCCallbackUintPacketTypeDef    GetCapacityCallback;
UpstreamMSCCallbackPacketTypeDef        GetStreamDataCallback;
uint64_t                                BlockStart;
uint32_t                                BlockCount;
uint32_t                                ByteCount;

UpstreamPacketTypeDef*                  ReadStreamPacket;
uint8_t                                 ReadStreamBusy;


static void Upstream_MSC_TestReadyFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_BeginReadFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_BeginWriteFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_BeginWriteReplyCallback(UpstreamPacketTypeDef* replyPacket);



HAL_StatusTypeDef Upstream_MSC_TestReady(UpstreamMSCCallbackTypeDef callback)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
    	//UPSTREAM_STATEMACHINE_FREAKOUT;
        return HAL_ERROR;
    }
    
    TestReadyCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_TestReadyFreePacketCallback);
}



void Upstream_MSC_TestReadyFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_TEST_UNIT_READY;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        //Upstream_ReleasePacket(freePacket);					/////////!!!!!!!!!!!!!???????????
        if (Upstream_ReceivePacket(Upstream_MSC_TestReadyReplyCallback) != HAL_OK)
        {
            TestReadyCallback(HAL_ERROR);
        }
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    TestReadyCallback(HAL_ERROR);
}



void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return;
    }

    if (replyPacket == NULL)
    {
        TestReadyCallback(HAL_ERROR);
        return;
    }

    if ((replyPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + 1)) ||
        (replyPacket->Data[0]  != HAL_OK))
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

    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    GetCapacityCallback = callback;
    freePacket = Upstream_GetFreePacketImmediately();
    if (freePacket == NULL)
    {
        return HAL_ERROR;
    }

    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_GET_CAPACITY;
    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        return Upstream_ReceivePacket(Upstream_MSC_GetCapacityReplyCallback);
    }
    //else:
    Upstream_ReleasePacket(freePacket); ////////?????????
    return HAL_ERROR;
}


void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    uint32_t uint1;
    uint32_t uint2;

    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return;
    }

    if (replyPacket == NULL)
    {
        GetCapacityCallback(NULL, 0, 0);
        return;
    }

    if (replyPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + (8 / 2)))
    {
        GetCapacityCallback(NULL, 0, 0);
        return;
    }

    uint1 = *(uint32_t*)&(replyPacket->Data[0]);
    uint2 = *(uint32_t*)&(replyPacket->Data[4]);
    GetCapacityCallback(replyPacket, uint1, uint2);     //usb_msc_scsi will use this packet, so don't release now
}



HAL_StatusTypeDef Upstream_MSC_BeginRead(UpstreamMSCCallbackTypeDef callback,
                                         uint64_t readBlockStart,
                                         uint32_t readBlockCount,
                                         uint32_t readByteCount)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    BlockStart = readBlockStart;
    BlockCount = readBlockCount;
    ByteCount = readByteCount;
    ReadStreamPacket = NULL;            //Prepare for GetStreamDataPacket's use
    ReadStreamBusy = 0;

    TestReadyCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_BeginReadFreePacketCallback);
}



void Upstream_MSC_BeginReadFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16 + ((4 * 3) / 2);
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_READ;
    *(uint64_t*)&(freePacket->Data[0]) = BlockStart;
    *(uint32_t*)&(freePacket->Data[8]) = BlockCount;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        if (Upstream_ReceivePacket(Upstream_MSC_TestReadyReplyCallback) != HAL_OK)  //Re-use TestReadyReplyCallback because it does exactly what we want!
        {
            TestReadyCallback(HAL_ERROR);
        }
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    TestReadyCallback(HAL_ERROR);
}



HAL_StatusTypeDef Upstream_MSC_GetStreamDataPacket(UpstreamMSCCallbackPacketTypeDef callback)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }
    GetStreamDataCallback = callback;

    if (ReadStreamBusy != 0)
    {
        return HAL_OK;
    }
    ReadStreamBusy = 1;

    if (ReadStreamPacket && GetStreamDataCallback)      //Do we have a stored packet and an address to send it?
    {
        Upstream_MSC_GetStreamDataPacketCallback(ReadStreamPacket); //Send it now!
        ReadStreamPacket = NULL;
        return HAL_OK;              //Our callback will call us again, so we don't need to get a packet in this case.
    }
    return Upstream_ReceivePacket(Upstream_MSC_GetStreamDataPacketCallback);
}



void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket)
{
    uint16_t dataLength8;

    ReadStreamBusy = 0;

    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return;
    }

    if (GetStreamDataCallback == NULL)
    {
        ReadStreamPacket = replyPacket;     //We used up our callback already, so save this one for later.
        return;
    }

    if (replyPacket == NULL)
    {
        GetStreamDataCallback(NULL, 0);
        return;
    }

    dataLength8 = (replyPacket->Length16 - UPSTREAM_PACKET_HEADER_LEN_16) * 2;

    if (((replyPacket->CommandClass & COMMAND_CLASS_DATA_FLAG) == 0) ||     //Any 'command' reply (as opposed to 'data' reply) is an automatic fail here
         (replyPacket->Length16 <= UPSTREAM_PACKET_HEADER_LEN_16) ||        //Should be at least one data byte in the reply.
         (dataLength8 > ByteCount))                                         //No more data than expected transfer length
    {
        GetStreamDataCallback(NULL, 0);
        return;
    }

    ByteCount -= dataLength8;
    GetStreamDataCallback(replyPacket, dataLength8);    //usb_msc_scsi will use this packet, so don't release now
    if (ByteCount > 0)
    {
        Upstream_MSC_GetStreamDataPacket(NULL);         //Try to get the next packet now, before USB asks for it
    }
}



HAL_StatusTypeDef Upstream_MSC_BeginWrite(UpstreamMSCCallbackTypeDef callback,
                                          uint64_t writeBlockStart,
                                          uint32_t writeBlockCount)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    BlockStart = writeBlockStart;
    BlockCount = writeBlockCount;
    TestReadyCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_BeginWriteFreePacketCallback);
}



void Upstream_MSC_BeginWriteFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16 + ((4 * 3) / 2);
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_WRITE;
    *(uint64_t*)&(freePacket->Data[0]) = BlockStart;
    *(uint32_t*)&(freePacket->Data[8]) = BlockCount;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        if (Upstream_ReceivePacket(Upstream_MSC_BeginWriteReplyCallback) != HAL_OK)
        {
            TestReadyCallback(HAL_ERROR);
        }
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    TestReadyCallback(HAL_ERROR);
}



void Upstream_MSC_BeginWriteReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    uint8_t tempResult;

    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return;
    }

    if (replyPacket == NULL)
    {
        TestReadyCallback(HAL_ERROR);
        return;
    }

    if ((replyPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + 1)) ||
        ((replyPacket->Data[0] != HAL_OK) && (replyPacket->Data[0] != HAL_BUSY)))
    {
        Upstream_ReleasePacket(replyPacket);
        TestReadyCallback(HAL_ERROR);
        return;
    }

    tempResult = replyPacket->Data[0];
    Upstream_ReleasePacket(replyPacket);
    TestReadyCallback(tempResult);
}



HAL_StatusTypeDef Upstream_MSC_PutStreamDataPacket(UpstreamPacketTypeDef* packetToSend,
                                                   uint32_t dataLength8)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    if ((dataLength8 % 2) != 0)
    {
        return HAL_ERROR;
    }

    packetToSend->Length16 = (dataLength8 / 2) + UPSTREAM_PACKET_HEADER_LEN_16;
    packetToSend->CommandClass = COMMAND_CLASS_MASS_STORAGE | COMMAND_CLASS_DATA_FLAG;
    packetToSend->Command = COMMAND_MSC_WRITE;
    return Upstream_TransmitPacket(packetToSend);
}

