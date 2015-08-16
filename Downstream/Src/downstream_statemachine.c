/*
 * downstream_statemachine.c
 *
 *  Created on: 2/08/2015
 *      Author: Robert Fisk
 */


#include "downstream_statemachine.h"
#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "downstream_msc.h"
#include "usbh_core.h"
#include "usbh_msc.h"



DownstreamStateTypeDef			DownstreamState;
InterfaceCommandClassTypeDef	ConfiguredDeviceClass;


void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket);
void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket);



void Downstream_InitStateMachine(void)
{
	DownstreamState = STATE_DEVICE_NOT_READY;
	ConfiguredDeviceClass = COMMAND_CLASS_INTERFACE;
	Downstream_InitSPI();

	//Prepare to receive our first packet from Upstream!
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


void Downstream_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
	switch (receivedPacket->CommandClass)
	{
	case COMMAND_CLASS_INTERFACE:
		if (DownstreamState > STATE_DEVICE_READY)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		Downstream_PacketProcessor_Interface(receivedPacket);
		break;

	case COMMAND_CLASS_MASS_STORAGE:
		if (DownstreamState != STATE_ACTIVE)
		{
			Downstream_PacketProcessor_ErrorReply(receivedPacket);
			return;
		}
		Downstream_MSC_PacketProcessor(receivedPacket);
		break;

	//Add other classes here...

	default:
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
	}
}



void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket)
{
	switch (receivedPacket->Command)
	{
	case COMMAND_INTERFACE_ECHO:
		Downstream_TransmitPacket(receivedPacket);
		Downstream_ReceivePacket(Downstream_PacketProcessor);
		return;

	case COMMAND_INTERFACE_NOTIFY_DEVICE:
		if (DownstreamState == STATE_DEVICE_READY)
		{
			Downstream_PacketProcessor_Interface_ReplyNotifyDevice(receivedPacket);
			return;
		}

		if (DownstreamState == STATE_DEVICE_NOT_READY)
		{
			DownstreamState = STATE_WAIT_DEVICE_READY;
			Downstream_ReleasePacket(receivedPacket);
			return;
		}
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
		return;


	default:
		Downstream_PacketProcessor_ErrorReply(receivedPacket);
	}
}


void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	replyPacket->CommandClass = COMMAND_CLASS_INTERFACE;
	replyPacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
	replyPacket->Data[0] = ConfiguredDeviceClass;
	Downstream_TransmitPacket(replyPacket);

	DownstreamState = STATE_ACTIVE;
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


void Downstream_PacketProcessor_ErrorReply(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN;
	replyPacket->CommandClass = COMMAND_CLASS_ERROR;
	Downstream_TransmitPacket(replyPacket);
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


void Downstream_PacketProcessor_ClassReply(DownstreamPacketTypeDef* replyPacket)
{
	Downstream_TransmitPacket(replyPacket);
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


//This callback receives various event ids from the host stack, either
//at INT_PRIORITY_OTG_FS or from main(). We should therefore be prepared
//for pre-emption by USB or SPI/DMA interrupts.
void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id)
{
	InterfaceCommandClassTypeDef newActiveClass = COMMAND_CLASS_INTERFACE;

	//Called from USB interrupt
	if (id == HOST_USER_DISCONNECTION)
	{
		DownstreamState = STATE_DEVICE_NOT_READY;
		return;
	}

	//Called from main(). Beware pre-emption!
	if (id == HOST_USER_CLASS_ACTIVE)
	{
		switch (phost->pActiveClass->ClassCode)
		{
		case USB_MSC_CLASS:
			if (Downstream_MSC_ApproveConnectedDevice() == HAL_OK)
			{
				newActiveClass = COMMAND_CLASS_MASS_STORAGE;
			}
			break;

		//Add other classes here...
		}

		//To change device class, we must reboot.
		if ((ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE) &&
			(ConfiguredDeviceClass != newActiveClass))
		{
			SPI_INTERFACE_FREAKOUT_NO_RETURN;
			DownstreamState = STATE_ERROR;
			return;
		}

		if (newActiveClass == COMMAND_CLASS_INTERFACE)
		{
			return;
		}

		ConfiguredDeviceClass = newActiveClass;
		if (DownstreamState == STATE_WAIT_DEVICE_READY)
		{
			Downstream_GetFreePacket(Downstream_PacketProcessor_Interface_ReplyNotifyDevice);
			return;
		}

		if (DownstreamState == STATE_DEVICE_NOT_READY)
		{
			DownstreamState = STATE_DEVICE_READY;
			return;
		}

		SPI_INTERFACE_FREAKOUT_NO_RETURN;
		DownstreamState = STATE_ERROR;
		return;
	}
}

