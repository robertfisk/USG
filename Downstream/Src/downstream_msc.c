/*
 * downstream_msc.c
 *
 *  Created on: 8/08/2015
 *      Author: Robert Fisk
 */


#include "downstream_msc.h"
#include "downstream_interface_def.h"
#include "downstream_statemachine.h"
#include "usbh_msc.h"


extern USBH_HandleTypeDef hUsbHostFS;		//Hard-link ourselves to usb_host.c


void Downstream_MSC_PacketProcessor_TestUnitReady(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_GetCapacity(DownstreamPacketTypeDef* receivedPacket);
void Downstream_MSC_PacketProcessor_BeginRead(DownstreamPacketTypeDef* receivedPacket);



//High-level checks on the connected device. We don't want some weirdly
//configured device to bomb our USB stack, accidentally or otherwise.
HAL_StatusTypeDef Downstream_MSC_ApproveConnectedDevice(void)
{
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	if ((MSC_Handle->unit[0].capacity.block_nbr == 0) ||
		(MSC_Handle->unit[0].capacity.block_nbr == UINT32_MAX))
	{
		return HAL_ERROR;
	}

	if (MSC_Handle->unit[0].capacity.block_size != 512)
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

	default:
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
	}

}


void Downstream_MSC_PacketProcessor_TestUnitReady(DownstreamPacketTypeDef* receivedPacket)
{
	if (USBH_MSC_UnitIsReady(&hUsbHostFS, 0))
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
	*(uint32_t*)&(receivedPacket->Data[0]) = MSC_Handle->unit[0].capacity.block_nbr;
	*(uint32_t*)&(receivedPacket->Data[4]) = (uint32_t)MSC_Handle->unit[0].capacity.block_size;
	Downstream_PacketProcessor_ClassReply(receivedPacket);
}



void Downstream_MSC_PacketProcessor_BeginRead(DownstreamPacketTypeDef* receivedPacket)
{
	uint64_t address;
	uint32_t count;
	MSC_HandleTypeDef* MSC_Handle =  (MSC_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

	if (receivedPacket->Length != (DOWNSTREAM_PACKET_HEADER_LEN + (4 * 3)))
	{
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
		return;
	}

	address = *(uint64_t*)&(receivedPacket->Data[0]);
	count = *(uint32_t*)&(receivedPacket->Data[8]);
	if ((address 		       >= (uint64_t)MSC_Handle->unit[0].capacity.block_nbr) ||
		((address + count - 1) >= (uint64_t)MSC_Handle->unit[0].capacity.block_nbr))
	{
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
		return;
	}

	if (USBH_MSC_UnitIsReady(&hUsbHostFS, 0))
	{
		receivedPacket->Data[0] = HAL_OK;
	}
	else
	{
		receivedPacket->Data[0] = HAL_ERROR;
	}
	receivedPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	Downstream_TransmitPacket(receivedPacket);

	USBH_MSC_Read(&hUsbHostFS,
				  0,
				  (uint32_t)address,
				  count);
}

