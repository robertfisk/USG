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
#include "build_config.h"


#ifdef CONFIG_MASS_STORAGE_ENABLED


//Stuff we need to save for our callbacks to use:
UpstreamMSCCallbackTypeDef              TestReadyCallback;
UpstreamMSCCallbackTypeDef              BeginReadCallback;
UpstreamMSCCallbackTypeDef              BeginWriteCallback;
UpstreamMSCCallbackTypeDef              DisconnectCallback;
UpstreamMSCCallbackUintPacketTypeDef    GetCapacityCallback;
UpstreamMSCCallbackPacketTypeDef        GetStreamDataCallback;
uint64_t                                BlockStart;
uint32_t                                BlockCount;
uint32_t                                ByteCount;

UpstreamPacketTypeDef*                  ReadStreamPacket;
uint8_t                                 ReadStreamBusy;


static void Upstream_MSC_TestReadyFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetCapacityFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket);
static void Upstream_MSC_BeginReadFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_BeginWriteFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_RequestDisconnectFreePacketCallback(UpstreamPacketTypeDef* freePacket);
static void Upstream_MSC_RequestDisconnectReplyCallback(UpstreamPacketTypeDef* replyPacket);



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



static void Upstream_MSC_TestReadyFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_TEST_UNIT_READY;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        //Upstream_PacketManager will free the packet when the transfer is done
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



static void Upstream_MSC_TestReadyReplyCallback(UpstreamPacketTypeDef* replyPacket)
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
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    GetCapacityCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_GetCapacityFreePacketCallback);
}



static void Upstream_MSC_GetCapacityFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_GET_CAPACITY;
    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        if (Upstream_ReceivePacket(Upstream_MSC_GetCapacityReplyCallback) != HAL_OK)
        {
            GetCapacityCallback(NULL, 0, 0);
        }
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    GetCapacityCallback(NULL, 0, 0);
}


static void Upstream_MSC_GetCapacityReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    uint32_t block_count;
    uint32_t block_size;

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
        Upstream_ReleasePacket(replyPacket);
        GetCapacityCallback(NULL, 0, 0);
        return;
    }

    block_count = *(uint32_t*)&(replyPacket->Data[0]);
    block_size = *(uint32_t*)&(replyPacket->Data[4]);

    if ((block_count < MSC_MINIMUM_BLOCK_COUNT) ||
        (block_size != MSC_SUPPORTED_BLOCK_SIZE))
    {
        Upstream_ReleasePacket(replyPacket);
        GetCapacityCallback(NULL, 0, 0);
        return;
    }
    GetCapacityCallback(replyPacket, block_count, block_size);     //usb_msc_scsi will use this packet, so don't release now
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

    BeginReadCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_BeginReadFreePacketCallback);
}



static void Upstream_MSC_BeginReadFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16 + ((4 * 3) / 2);
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_READ;
    *(uint64_t*)&(freePacket->Data[0]) = BlockStart;
    *(uint32_t*)&(freePacket->Data[8]) = BlockCount;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        BeginReadCallback(HAL_OK);
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    BeginReadCallback(HAL_ERROR);
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



static void Upstream_MSC_GetStreamDataPacketCallback(UpstreamPacketTypeDef* replyPacket)
{
    uint32_t dataLength8;

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
         (dataLength8 != MSC_MINIMUM_DATA_UNIT) ||                          //Should only receive integer units of MSC block size
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


#ifdef CONFIG_MASS_STORAGE_WRITES_PERMITTED
HAL_StatusTypeDef Upstream_MSC_BeginWrite(UpstreamMSCCallbackTypeDef callback,
                                          uint64_t writeBlockStart,
                                          uint32_t writeBlockCount)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE)
    {
        return HAL_ERROR;
    }

    LED_SetState(LED_STATUS_FLASH_READWRITE);
    BlockStart = writeBlockStart;
    BlockCount = writeBlockCount;
    BeginWriteCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_BeginWriteFreePacketCallback);
}



static void Upstream_MSC_BeginWriteFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16 + ((4 * 3) / 2);
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_WRITE;
    *(uint64_t*)&(freePacket->Data[0]) = BlockStart;
    *(uint32_t*)&(freePacket->Data[8]) = BlockCount;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        BeginWriteCallback(HAL_OK);
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    BeginWriteCallback(HAL_ERROR);
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
#endif  //#ifdef CONFIG_MASS_STORAGE_WRITES_PERMITTED



HAL_StatusTypeDef Upstream_MSC_RequestDisconnect(UpstreamMSCCallbackTypeDef callback)
{
    if (Upstream_StateMachine_CheckActiveClass() != COMMAND_CLASS_MASS_STORAGE) return HAL_ERROR;

    DisconnectCallback = callback;
    return Upstream_GetFreePacket(Upstream_MSC_RequestDisconnectFreePacketCallback);
}



static void Upstream_MSC_RequestDisconnectFreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_MASS_STORAGE;
    freePacket->Command = COMMAND_MSC_DISCONNECT;

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        //Upstream_PacketManager will free the packet when the transfer is done
        if (Upstream_ReceivePacket(Upstream_MSC_RequestDisconnectReplyCallback) != HAL_OK)
        {
            DisconnectCallback(HAL_ERROR);
        }
        return;
    }

    //else:
    Upstream_ReleasePacket(freePacket);
    DisconnectCallback(HAL_ERROR);
}



static void Upstream_MSC_RequestDisconnectReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    //Acknowledge the SCSI Stop command to host now.
    //We will disconnect from host when Downstream replies with COMMAND_ERROR_DEVICE_DISCONNECTED.

    Upstream_ReleasePacket(replyPacket);
    DisconnectCallback(HAL_OK);
}



#endif  //#ifdef CONFIG_MASS_STORAGE_ENABLED

