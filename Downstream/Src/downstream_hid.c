/*
 * downstream_hid.c
 *
 *  Created on: Apr 10, 2016
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */



#include "downstream_hid.h"
#include "downstream_statemachine.h"
#include "usbh_hid.h"



#define HID_REPORT_DATA_LEN     8
#define HID_MOUSE_DATA_LEN      3
#define HID_KEYBOARD_DATA_LEN   0


extern USBH_HandleTypeDef hUsbHostFS;                       //Hard-link ourselves to usb_host.c
extern InterfaceCommandClassTypeDef ConfiguredDeviceClass;  //Do a cheap hard-link to downstream_statemachine.c, rather than keep a duplicate here



InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

    if (HID_Handle->Protocol == HID_MOUSE_BOOT_CODE)
    {
        if ((HID_Handle->length >= HID_MOUSE_DATA_LEN) && (HID_Handle->length <= HID_REPORT_DATA_LEN))
        {
            return COMMAND_CLASS_HID_MOUSE;
        }
    }

    return COMMAND_CLASS_INTERFACE;         //fail
}



void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
    if (receivedPacket->Command != COMMAND_HID_REPORT)
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    if (USBH_HID_GetInterruptReport(&hUsbHostFS,
                                    Downstream_HID_InterruptReportCallback,
                                    receivedPacket) != HAL_OK)      //Don't free the packet, USBH_HID will use it then return it to InterruptReportCallback below
    {
        Downstream_PacketProcessor_FreakOut();
    }

}


void Downstream_HID_InterruptReportCallback(DownstreamPacketTypeDef* packetToSend)
{
    if (ConfiguredDeviceClass == COMMAND_CLASS_HID_MOUSE)
    {
        packetToSend->Length16 = ((HID_MOUSE_DATA_LEN + 1) / 2) + DOWNSTREAM_PACKET_HEADER_LEN_16;
    }
    //else if...
    else
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    Downstream_PacketProcessor_ClassReply(packetToSend);
}


