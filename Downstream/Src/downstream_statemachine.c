/*
 * downstream_statemachine.c
 *
 *  Created on: 2/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "downstream_statemachine.h"
#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "downstream_msc.h"
#include "downstream_hid.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include "usbh_hid.h"
#include "led.h"
#include "options.h"


DownstreamStateTypeDef          DownstreamState         = STATE_DEVICE_NOT_READY;
InterfaceCommandClassTypeDef    ConfiguredDeviceClass   = COMMAND_CLASS_INTERFACE;
uint8_t                         NotifyDisconnectReply   = 0;


static void Downstream_PacketProcessor_Interface(DownstreamPacketTypeDef* receivedPacket);
static void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket);
static void Downstream_PacketProcessor_NotifyDisconnectReply(DownstreamPacketTypeDef* packetToSend);



void Downstream_InitStateMachine(void)
{
    if ((DownstreamState       != STATE_DEVICE_NOT_READY) ||
        (ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE))
    {
        DOWNSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

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

    if (receivedPacket->CommandClass == COMMAND_CLASS_INTERFACE)
    {
        if (DownstreamState > STATE_DEVICE_READY)
        {
            DOWNSTREAM_STATEMACHINE_FREAKOUT;
            return;
        }
        Downstream_PacketProcessor_Interface(receivedPacket);
        return;
    }

    //If we get a class-specific message when our device is disconnected,
    //we need to tell Upstream of the fact (and not freak out).
    if (DownstreamState == STATE_DEVICE_NOT_READY)
    {
        Downstream_PacketProcessor_NotifyDisconnectReply(receivedPacket);
        return;
    }

    //We should only receive class-specific messages when we are in the Active state,
    //and only to our currently active device class.
    if ((DownstreamState != STATE_ACTIVE) ||
        (receivedPacket->CommandClass != ConfiguredDeviceClass))
    {
        DOWNSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }


    switch (ConfiguredDeviceClass)
    {

#ifdef ENABLE_MASS_STORAGE
    case COMMAND_CLASS_MASS_STORAGE:
        Downstream_MSC_PacketProcessor(receivedPacket);
        break;
#endif
#ifdef ENABLE_MOUSE
    case COMMAND_CLASS_HID_MOUSE:
        Downstream_HID_PacketProcessor(receivedPacket);
        break;
#endif
#ifdef ENABLE_KEYBOARD
    case COMMAND_CLASS_HID_KEYBOARD:
        Downstream_HID_PacketProcessor(receivedPacket);
        break;
#endif

    //Add other classes here...

    default:
        DOWNSTREAM_STATEMACHINE_FREAKOUT;
    }
}



//Used by downstream class interfaces, and SPI interface
void Downstream_PacketProcessor_FreakOut(void)
{
    DOWNSTREAM_STATEMACHINE_FREAKOUT;
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
        break;


    default:
        DOWNSTREAM_STATEMACHINE_FREAKOUT;
    }
}


void Downstream_PacketProcessor_Interface_ReplyNotifyDevice(DownstreamPacketTypeDef* replyPacket)
{
    replyPacket->Length16 = DOWNSTREAM_PACKET_HEADER_LEN_16 + 1;
    replyPacket->CommandClass = COMMAND_CLASS_INTERFACE;
    replyPacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
    replyPacket->Data[0] = ConfiguredDeviceClass;

    if (Downstream_TransmitPacket(replyPacket) == HAL_OK)
    {
        DownstreamState = STATE_ACTIVE;
        Downstream_ReceivePacket(Downstream_PacketProcessor);
    }
}


void Downstream_PacketProcessor_GenericErrorReply(DownstreamPacketTypeDef* replyPacket)
{
    replyPacket->Length16 = DOWNSTREAM_PACKET_HEADER_LEN_16;
    replyPacket->CommandClass = COMMAND_CLASS_ERROR;
    replyPacket->Command = COMMAND_ERROR_GENERIC;

    Downstream_TransmitPacket(replyPacket);
    Downstream_ReceivePacket(Downstream_PacketProcessor);
    NotifyDisconnectReply = 0;
}


void Downstream_PacketProcessor_ClassReply(DownstreamPacketTypeDef* replyPacket)
{
    Downstream_TransmitPacket(replyPacket);
    Downstream_ReceivePacket(Downstream_PacketProcessor);
    NotifyDisconnectReply = 0;
}


void Downstream_PacketProcessor_NotifyDisconnectReplyRequired(void)
{
    NotifyDisconnectReply = 1;
}


void Downstream_PacketProcessor_CheckNotifyDisconnectReply(void)
{
    if (NotifyDisconnectReply == 2)
    {
        Downstream_GetFreePacket(Downstream_PacketProcessor_NotifyDisconnectReply);
    }
}


void Downstream_PacketProcessor_NotifyDisconnectReply(DownstreamPacketTypeDef* packetToSend)
{
    packetToSend->Length16 = DOWNSTREAM_PACKET_HEADER_LEN_16;
    packetToSend->CommandClass = COMMAND_CLASS_ERROR;
    packetToSend->Command = COMMAND_ERROR_DEVICE_DISCONNECTED;
    Downstream_PacketProcessor_ClassReply(packetToSend);
}


//This callback receives various event ids from the host stack,
//either at INT_PRIORITY_OTG_FS or from main().
void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id)
{
    InterfaceCommandClassTypeDef newActiveClass = COMMAND_CLASS_INTERFACE;

    if (DownstreamState >= STATE_ERROR)
    {
        return;
    }

    //Called from USB interrupt.
    //Simple function shouldn't need to worry about preempting anything important.
    if (id == HOST_USER_DISCONNECTION)
    {
        DownstreamState = STATE_DEVICE_NOT_READY;
        if (NotifyDisconnectReply == 1)
        {
            NotifyDisconnectReply = 2;          //Request a 'device disconnected' reply when we get back to main()
        }
        return;
    }


    //Called from main()
    if (id == HOST_USER_CLASS_ACTIVE)
    {
        switch (phost->pActiveClass->ClassCode)
        {
#ifdef ENABLE_MASS_STORAGE
        case USB_MSC_CLASS:
            newActiveClass = Downstream_MSC_ApproveConnectedDevice();
            break;
#endif
#if defined (ENABLE_KEYBOARD) || defined (ENABLE_MOUSE)
        case USB_HID_CLASS:
            newActiveClass = Downstream_HID_ApproveConnectedDevice();
            break;
#endif

        //Add other classes here...


        }

        //Unsupported device classes will cause a slow fault flash.
        //This is distinct from the fast freakout flash caused by internal errors or attacks.
        //We consider supported classes that fail their approval checks to also be unsupported devices.
        if (newActiveClass == COMMAND_CLASS_INTERFACE)
        {
            USB_Host_Disconnect();
            LED_Fault_SetBlinkRate(LED_SLOW_BLINK_RATE);
            DownstreamState = STATE_ERROR;
            return;
        }

        //If we already configured a device class, we cannot change to a different one without rebooting.
        //This blocks 'hidden device' BadUSB attacks.
        if ((ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE) &&
            (ConfiguredDeviceClass != newActiveClass))
        {
            DOWNSTREAM_STATEMACHINE_FREAKOUT;
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

        DOWNSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }


    //Called from main():
    if ((id == HOST_USER_CLASS_FAILED) ||
        (id == HOST_USER_UNRECOVERED_ERROR))        //Probably due to a crappy device that won't enumerate!
    {
        //Unsupported device classes will cause a slow fault flash.
        //This is distinct from the fast freakout flash caused by internal errors or attacks.
        USB_Host_Disconnect();
        LED_Fault_SetBlinkRate(LED_SLOW_BLINK_RATE);
        DownstreamState = STATE_ERROR;
        return;
    }

}

