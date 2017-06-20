/*
 * upstream_statemachine.c
 *
 *  Created on: 20/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "upstream_statemachine.h"
#include "upstream_spi.h"
#include "upstream_hid.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_msc.h"
#include "usbd_hid.h"
#include "options.h"


UpstreamStateTypeDef            UpstreamState           = STATE_TEST_INTERFACE;
InterfaceCommandClassTypeDef    ConfiguredDeviceClass   = COMMAND_CLASS_INTERFACE;


void Upstream_StateMachine_TestInterfaceReplyCallback(UpstreamPacketTypeDef* replyPacket);
void Upstream_StateMachine_NotifyDevice(UpstreamPacketTypeDef* freePacket);
void Upstream_StateMachine_NotifyDeviceReplyCallback(UpstreamPacketTypeDef* replyPacket);



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

    freePacket->Length16 = UPSTREAM_PACKET_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_INTERFACE;
    freePacket->Command = COMMAND_INTERFACE_ECHO;

    //Fill our test packet with some junk
    testDataValue = 0xFF;
    for (i = 0; i < MSC_MEDIA_PACKET; i++)
    {
        freePacket->Data[i] = testDataValue;
        testDataValue += 39;
    }

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        Upstream_ReceivePacket(Upstream_StateMachine_TestInterfaceReplyCallback);
    }
}


//Used by upstream_spi freakout macro, indicates we should stop everything.
void Upstream_StateMachine_SetErrorState(void)
{
    UpstreamState = STATE_ERROR;
    if ((ConfiguredDeviceClass > COMMAND_CLASS_INTERFACE) &&
        (ConfiguredDeviceClass < COMMAND_CLASS_ERROR))
    {
        USBD_Stop(&hUsbDeviceFS);
    }
}


InterfaceCommandClassTypeDef Upstream_StateMachine_CheckActiveClass(void)
{
    if (UpstreamState == STATE_ERROR)
    {
        return COMMAND_CLASS_ERROR;
    }

    if ((UpstreamState != STATE_DEVICE_ACTIVE) &&
        (UpstreamState != STATE_SUSPENDED))
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return COMMAND_CLASS_INTERFACE;
    }

    return ConfiguredDeviceClass;
}


uint32_t Upstream_StateMachine_GetSuspendState(void)
{
    if (UpstreamState == STATE_SUSPENDED)
    {
        return 1;
    }
    return 0;
}


void Upstream_StateMachine_TestInterfaceReplyCallback(UpstreamPacketTypeDef* replyPacket)
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

    if (replyPacket->Length16 != UPSTREAM_PACKET_LEN_16)
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
    Upstream_StateMachine_NotifyDevice(replyPacket);
}


void Upstream_StateMachine_NotifyDevice(UpstreamPacketTypeDef* freePacket)
{
    UpstreamState = STATE_WAIT_DEVICE;
    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = COMMAND_CLASS_INTERFACE;
    freePacket->Command = COMMAND_INTERFACE_NOTIFY_DEVICE;
    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        Upstream_ReceivePacket(Upstream_StateMachine_NotifyDeviceReplyCallback);
    }
}


void Upstream_StateMachine_NotifyDeviceReplyCallback(UpstreamPacketTypeDef* replyPacket)
{
    InterfaceCommandClassTypeDef newActiveClass = COMMAND_CLASS_INTERFACE;
    USBD_ClassTypeDef*           newClassPointer;

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

    if (replyPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + 1))
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    switch (replyPacket->Data[0])
    {
#ifdef ENABLE_MASS_STORAGE
    case COMMAND_CLASS_MASS_STORAGE:
        newActiveClass = COMMAND_CLASS_MASS_STORAGE;
        newClassPointer = &USBD_MSC;
        break;
#endif
#ifdef ENABLE_MOUSE
    case COMMAND_CLASS_HID_MOUSE:
        newActiveClass = COMMAND_CLASS_HID_MOUSE;
        newClassPointer = &USBD_HID;
        USBD_HID_PreinitMouse();
        break;
#endif
#ifdef ENABLE_KEYBOARD
    case COMMAND_CLASS_HID_KEYBOARD:
        newActiveClass = COMMAND_CLASS_HID_KEYBOARD;
        newClassPointer = &USBD_HID;
        USBD_HID_PreinitKeyboard();
        break;
#endif
        //Add other supported classes here...
    }

    Upstream_ReleasePacket(replyPacket);

    if (newActiveClass == COMMAND_CLASS_INTERFACE)
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    //Downstream should never change the active device class without rebooting!
    if ((ConfiguredDeviceClass != COMMAND_CLASS_INTERFACE) &&
        (ConfiguredDeviceClass != newActiveClass))
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    UpstreamState = STATE_DEVICE_ACTIVE;
    ConfiguredDeviceClass = newActiveClass;
    USBD_RegisterClass(&hUsbDeviceFS, newClassPointer);
    USBD_Start(&hUsbDeviceFS);

    //The USB device stack will now receive commands from our host.
    //All we need to do is monitor for downstream device disconnection.
}


void Upstream_StateMachine_DeviceDisconnected(void)
{
    if ((ConfiguredDeviceClass == COMMAND_CLASS_INTERFACE) ||
        (ConfiguredDeviceClass >= COMMAND_CLASS_ERROR))
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    USBD_Stop(&hUsbDeviceFS);
    Upstream_GetFreePacket(Upstream_StateMachine_NotifyDevice);
}


//Suspend event activated by our host
void Upstream_StateMachine_Suspend(void)
{
    if (UpstreamState >= STATE_ERROR)
    {
        return;
    }
    if (UpstreamState != STATE_DEVICE_ACTIVE)
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    UpstreamState = STATE_SUSPENDED;
}


//Resume event activated by our host
void Upstream_StateMachine_CheckResume(void)
{
    if (UpstreamState == STATE_SUSPENDED)
    {
        UpstreamState = STATE_DEVICE_ACTIVE;
    }
}


//Activated by downstream report, causing us to wake up the host
void Upstream_StateMachine_Wakeup(void)
{
    USBD_ClassTypeDef* activeClass;

    if (UpstreamState != STATE_SUSPENDED)
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    //This is how I'd wakeup the host, IF IT ACTUALLY WORKED!
    //USBD_LL_WakeupHost(&hUsbDeviceFS);

    //This is really ugly! But wakeup seems to be broken on the STM32, so we do it the hard way.
    activeClass = USBD_DeInit(&hUsbDeviceFS);
    USB_Device_Init();
    USBD_RegisterClass(&hUsbDeviceFS, activeClass);
    USBD_Start(&hUsbDeviceFS);
}

