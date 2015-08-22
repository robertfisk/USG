/*
 * upstream_statemachine.c
 *
 *  Created on: 20/08/2015
 *      Author: Robert Fisk
 */


#include "upstream_statemachine.h"
#include "upstream_spi.h"
#include "upstream_interface_def.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_msc.h"


UpstreamStateTypeDef			UpstreamState		= STATE_TEST_INTERFACE;
InterfaceCommandClassTypeDef	ActiveDeviceClass	= COMMAND_CLASS_INTERFACE;


void Upstream_TestInterfaceReplyCallback(UpstreamPacketTypeDef* replyPacket);
void Upstream_NotifyDeviceReplyCallback(UpstreamPacketTypeDef* replyPacket);



void Upstream_InitStateMachine(void)
{
	UpstreamPacketTypeDef* freePacket;
	uint16_t i;
	uint8_t  testDataValue;

	Upstream_InitSPI();

	//Prepare SPI test packet
	freePacket = Upstream_GetFreePacketImmediately();
	if (freePacket == NULL)
	{
		UpstreamState = STATE_ERROR;
		return;
	}

	freePacket->Length = UPSTREAM_PACKET_HEADER_LEN + MSC_MEDIA_PACKET;
	freePacket->CommandClass = COMMAND_CLASS_INTERFACE;
	freePacket->Command = COMMAND_INTERFACE_ECHO;
	testDataValue = 0xFF;

	//Fill our test packet with some junk
	for (i = 0; i < MSC_MEDIA_PACKET; i++)
	{
		freePacket->Data[i] = testDataValue;
		testDataValue += 39;
	}

	if (Upstream_TransmitPacket(freePacket) == HAL_OK)
	{
		Upstream_ReceivePacket(Upstream_TestInterfaceReplyCallback);
	}
}


//Used by upstream_spi freakout macro, indicates we should stop everything.
void Upstream_StateMachine_SetErrorState(void)
{
	UpstreamState = STATE_ERROR;
	if ((ActiveDeviceClass > COMMAND_CLASS_INTERFACE) &&
		(ActiveDeviceClass < COMMAND_CLASS_ERROR))
	{
		USBD_Stop(&hUsbDeviceFS);
	}
}


HAL_StatusTypeDef Upstream_StateMachine_CheckClassOperationOk(void)
{
	if (UpstreamState == STATE_ERROR)
	{
		return HAL_ERROR;
	}

	if (UpstreamState != STATE_DEVICE_ACTIVE)
	{
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return HAL_ERROR;
	}

	return HAL_OK;
}


void Upstream_TestInterfaceReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
	uint16_t i;
	uint8_t  testDataValue;

	if (UpstreamState >= STATE_ERROR)
	{
		return;
	}

	if ((UpstreamState != STATE_TEST_INTERFACE) ||
		(replyPacket == NULL))
	{
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return;
	}

	if (replyPacket->Length != (UPSTREAM_PACKET_HEADER_LEN + MSC_MEDIA_PACKET))
	{
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return;
	}

	testDataValue = 0xFF;
	for (i = 0; i < MSC_MEDIA_PACKET; i++)
	{
		if (replyPacket->Data[i] != testDataValue)
		{
			UPSTREAM_STATEMACHINE_FREAKOUT;
			return;
		}
		testDataValue += 39;
	}


	//SPI interface passed checks. Now we wait for a device to be attached to downstream.
	UpstreamState = STATE_WAIT_DEVICE;
	replyPacket->Length = UPSTREAM_PACKET_HEADER_LEN;
	replyPacket->CommandClass = COMMAND_CLASS_INTERFACE;
	replyPacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
	if (Upstream_TransmitPacket(replyPacket) == HAL_OK)
	{
		Upstream_ReceivePacket(Upstream_NotifyDeviceReplyCallback);
	}
}


void Upstream_NotifyDeviceReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
	if (UpstreamState >= STATE_ERROR)
	{
		return;
	}

	if ((UpstreamState != STATE_WAIT_DEVICE) ||
		(replyPacket == NULL))
	{
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return;
	}

	if (replyPacket->Length != (UPSTREAM_PACKET_HEADER_LEN + 1))
	{
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return;
	}

	switch (replyPacket->Data[0])
	{
	case COMMAND_CLASS_MASS_STORAGE:
		USBD_RegisterClass(&hUsbDeviceFS, &USBD_MSC);
		break;

	default:
		UPSTREAM_STATEMACHINE_FREAKOUT;
		return;
	}

	USBD_Start(&hUsbDeviceFS);
	UpstreamState = STATE_DEVICE_ACTIVE;
	ActiveDeviceClass = replyPacket->Data[0];

	//The USB device stack will now receive commands from our host.
	//All we need to do is check for downstream device removal.
}


