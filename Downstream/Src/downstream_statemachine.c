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
#include "led.h"
#include "interrupts.h"



DownstreamStateTypeDef			DownstreamState			= STATE_DEVICE_NOT_READY;
InterfaceCommandClassTypeDef	ConfiguredDeviceClass	= COMMAND_CLASS_INTERFACE;


void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket);
void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket);



void Downstream_InitStateMachine(void)
{
	Downstream_InitSPI();

	//Prepare to receive our first packet from Upstream!
	Downstream_ReceivePacket(Downstream_PacketProcessor);
}


void Downstream_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
	if (DownstreamState >= STATE_ERROR)
	{
		Downstream_ReleasePacket(receivedPacket);
		return;
	}

	switch (receivedPacket->CommandClass)
	{
	case COMMAND_CLASS_INTERFACE:
		if (DownstreamState > STATE_DEVICE_READY)
		{
			DOWNSTREAM_STATEMACHINE_FREAKOUT;
		}
		Downstream_PacketProcessor_Interface(receivedPacket);
		break;

	case COMMAND_CLASS_MASS_STORAGE:
		if (DownstreamState != STATE_ACTIVE)
		{
			DOWNSTREAM_STATEMACHINE_FREAKOUT;
		}
		Downstream_MSC_PacketProcessor(receivedPacket);
		break;

	//Add other classes here...

	default:
		DOWNSTREAM_STATEMACHINE_FREAKOUT;
	}
}


//Used by downstream_spi freakout macro, indicates we should stop everything.
void Downstream_PacketProcessor_SetErrorState(void)
{
	DownstreamState = STATE_ERROR;
}


//Used by downstream class interfaces
void Downstream_PacketProcessor_FreakOut(void)
{
	DOWNSTREAM_STATEMACHINE_FREAKOUT;
}



void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket)
{
	switch (receivedPacket->Command)
	{
	case COMMAND_INTERFACE_ECHO:
		if (Downstream_TransmitPacket(receivedPacket) == HAL_OK)
		{
			Downstream_ReceivePacket(Downstream_PacketProcessor);
		}
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
		DOWNSTREAM_STATEMACHINE_FREAKOUT;
		return;


	default:
		DOWNSTREAM_STATEMACHINE_FREAKOUT;
	}
}


void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN + 1;
	replyPacket->CommandClass = COMMAND_CLASS_INTERFACE;
	replyPacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
	replyPacket->Data[0] = ConfiguredDeviceClass;

	if (Downstream_TransmitPacket(replyPacket) == HAL_OK)
	{
		DownstreamState = STATE_ACTIVE;
		Downstream_ReceivePacket(Downstream_PacketProcessor);
	}
}


void Downstream_PacketProcessor_ErrorReply(DownstreamPacketTypeDef* replyPacket)
{
	replyPacket->Length = DOWNSTREAM_PACKET_HEADER_LEN;
	replyPacket->CommandClass = COMMAND_CLASS_ERROR;

	if (Downstream_TransmitPacket(replyPacket) == HAL_OK)
	{
		Downstream_ReceivePacket(Downstream_PacketProcessor);
	}
}


void Downstream_PacketProcessor_ClassReply(DownstreamPacketTypeDef* replyPacket)
{
	if (Downstream_TransmitPacket(replyPacket) == HAL_OK)
	{
		Downstream_ReceivePacket(Downstream_PacketProcessor);
	}
}


//This callback receives various event ids from the host stack,
//either at INT_PRIORITY_OTG_FS or from main().
//We should therefore elevate our execution priority to INT_PRIORITY_OTG_FS where necessary.
void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id)
{
	InterfaceCommandClassTypeDef newActiveClass = COMMAND_CLASS_INTERFACE;

	if (DownstreamState >= STATE_ERROR)
	{
		return;
	}

	//Called from USB interrupt
	if (id == HOST_USER_DISCONNECTION)
	{
		DownstreamState = STATE_DEVICE_NOT_READY;
		return;
	}

	//Elevate our priority level so we aren't interrupted
	__set_BASEPRI(INT_PRIORITY_OTG_FS);


	//Called from main()
	if (id == HOST_USER_UNRECOVERED_ERROR)
	{
		DOWNSTREAM_STATEMACHINE_FREAKOUT_NORETURN;
		__set_BASEPRI(0);
		return;
	}

	//Called from main()
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

		//Unsupported device classes will cause a slow fault flash.
		//This is distinct from the fast freakout flash caused by internal errors or attacks.
		default:
			LED_Fault_SetBlinkRate(LED_SLOW_BLINK_RATE);
			DownstreamState = STATE_ERROR;
			__set_BASEPRI(0);
			return;
		}


		//If the new device has failed its 'approval' checks, we are sufficiently freaked out.
		if (newActiveClass == COMMAND_CLASS_INTERFACE)
		{
			DOWNSTREAM_STATEMACHINE_FREAKOUT_NORETURN;
			__set_BASEPRI(0);
			return;
		}

		//If we already configured a device class, we cannot change to a different one without rebooting.
		//This blocks some BadUSB attacks.
		if ((ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE) &&
			(ConfiguredDeviceClass != newActiveClass))
		{
			DOWNSTREAM_STATEMACHINE_FREAKOUT_NORETURN;
			__set_BASEPRI(0);
			return;
		}
		ConfiguredDeviceClass = newActiveClass;

		if (DownstreamState == STATE_WAIT_DEVICE_READY)
		{
			Downstream_GetFreePacket(Downstream_PacketProcessor_Interface_ReplyNotifyDevice);
			__set_BASEPRI(0);
			return;
		}

		if (DownstreamState == STATE_DEVICE_NOT_READY)
		{
			DownstreamState = STATE_DEVICE_READY;
			__set_BASEPRI(0);
			return;
		}

		DOWNSTREAM_STATEMACHINE_FREAKOUT_NORETURN;
		__set_BASEPRI(0);
		return;
	}
}

