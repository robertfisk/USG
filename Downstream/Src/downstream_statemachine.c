/*
 * downstream_statemachine.c
 *
 *  Created on: 2/08/2015
 *      Author: Robert Fisk
 */


#include "downstream_statemachine.h"
#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "usbh_core.h"
#include "usbh_msc.h"



DownstreamStateTypeDef			DownstreamState;
InterfaceCommandClassTypeDef	ConfiguredDeviceClass;


void Downstream_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);
void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket);
void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket);
void Downstream_PacketProcessor_EmptyReply(DownstreamPacketTypeDef* replyPacket);



void Downstream_InitStateMachine(void)
{
	DownstreamState = STATE_NOT_READY;
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
		if (DownstreamState != STATE_NOT_READY)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		Downstream_PacketProcessor_Interface(receivedPacket);
		break;

	case COMMAND_CLASS_MASS_STORAGE:
		if (DownstreamState != STATE_DEVICE_READY)
		{
			Downstream_PacketProcessor_EmptyReply(receivedPacket);
		}
		//Mass storage packet processor...
		break;

	//Add other classes here...

	default:
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
}



void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket)
{
	switch (receivedPacket->Command)
	{
	case COMMAND_INTERFACE_ECHO:
		Downstream_TransmitPacket(receivedPacket);
		Downstream_ReceivePacket(Downstream_PacketProcessor);
		break;

	case COMMAND_INTERFACE_NOTIFY_DEVICE:
		if (ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE)
		{
			Downstream_PacketProcessor_Interface_ReplyNotifyDevice(receivedPacket);
		}
		else
		{
			Downstream_ReleasePacket(receivedPacket);
			DownstreamState = STATE_WAIT_DEVICE_READY_CALLBACK;
		}
		break;

	default:
		SPI_INTERFACE_FREAKOUT_RETURN_VOID;
	}
}


void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	replyPacket->CommandClass = COMMAND_CLASS_INTERFACE;
	replyPacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
	replyPacket->Data[0] = ConfiguredDeviceClass;
	Downstream_TransmitPacket(replyPacket);

	DownstreamState = STATE_DEVICE_READY;
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


//An empty reply implies and error processing class-specific requests.
void Downstream_PacketProcessor_EmptyReply(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN;
	Downstream_TransmitPacket(replyPacket);
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id)
{
	InterfaceCommandClassTypeDef newActiveClass;

	if (id == HOST_USER_DISCONNECTION)
	{
		DownstreamState = STATE_NOT_READY;
		return;
	}


	if (id == HOST_USER_CLASS_ACTIVE)
	{
		if (DownstreamState > STATE_WAIT_DEVICE_READY_CALLBACK)
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}

		switch (phost->pActiveClass->ClassCode)
		{
		case USB_MSC_CLASS:
			newActiveClass = COMMAND_CLASS_MASS_STORAGE;
			break;

		//Add other classes here...

		default:
			newActiveClass = COMMAND_CLASS_INTERFACE;
		}

		if ((ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE) &&
			(ConfiguredDeviceClass != newActiveClass))				//To change device class, we must reboot.
		{
			SPI_INTERFACE_FREAKOUT_RETURN_VOID;
		}
		ConfiguredDeviceClass = newActiveClass;
		if (DownstreamState == STATE_WAIT_DEVICE_READY_CALLBACK)
		{
			Downstream_GetFreePacket(Downstream_PacketProcessor_Interface_ReplyNotifyDevice);
		}
		return;
	}
}

