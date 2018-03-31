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
#include "stm32f4xx_hal.h"
#include "build_config.h"


#if defined (CONFIG_KEYBOARD_ENABLED) || defined (CONFIG_MOUSE_ENABLED)

extern USBH_HandleTypeDef hUsbHostFS;                       //Hard-link ourselves to usb_host.c
extern InterfaceCommandClassTypeDef ConfiguredDeviceClass;  //Do a cheap hard-link to downstream_statemachine.c, rather than keep a duplicate here


//Information required to extract the data we need from incoming device reports.
uint8_t ReportButtonBitOffset;
uint8_t ReportButtonBitLength;
uint8_t ReportXBitOffset;
uint8_t ReportXBitLength;
uint8_t ReportYBitOffset;
uint8_t ReportYBitLength;
uint8_t ReportWheelBitOffset;
uint8_t ReportWheelBitLength;

//Stuff used while parsing HID report
uint8_t* ReportDataPointer;
uint8_t ItemHeader;
uint8_t ItemData;



static HAL_StatusTypeDef Downstream_HID_Mouse_ParseReportDescriptor(void);
static HAL_StatusTypeDef Downstream_HID_GetNextReportItem(void);
static void Downstream_HID_Mouse_ExtractDataFromReport(DownstreamPacketTypeDef* packetToSend);
static void Downstream_HID_Keyboard_ExtractDataFromReport(DownstreamPacketTypeDef* packetToSend);
static uint8_t Downstream_HID_Mouse_Extract8BitValue(HID_HandleTypeDef* hidHandle,
                                                     uint8_t valueBitOffset,
                                                     uint8_t valueBitLength);


InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

#ifdef CONFIG_MOUSE_ENABLED
    if (HID_Handle->Protocol == HID_MOUSE_PROTOCOL)
    {
        if (Downstream_HID_Mouse_ParseReportDescriptor() == HAL_OK)
        {
            return COMMAND_CLASS_HID_MOUSE;             //success!
        }
    }
#endif
#ifdef CONFIG_KEYBOARD_ENABLED
    if (HID_Handle->Protocol == HID_KEYBRD_PROTOCOL)
    {
            return COMMAND_CLASS_HID_KEYBOARD;          //success!
    }
#endif

    //else:
    return COMMAND_CLASS_INTERFACE;                     //fail
}


#ifdef CONFIG_MOUSE_ENABLED
static HAL_StatusTypeDef Downstream_HID_Mouse_ParseReportDescriptor(void)
{
    uint32_t currentReportBitIndex = 0;
    uint8_t currentUsagePage = 0;
    uint8_t currentUsageIndex = 0;
    uint8_t xUsageIndex = 0xFF;
    uint8_t yUsageIndex = 0xFF;
    uint8_t wheelUsageIndex = 0xFF;
    uint8_t currentReportSize = 0;
    uint8_t currentReportCount = 0;

    ReportDataPointer = hUsbHostFS.device.Data;

    ReportButtonBitLength = 0;
    ReportXBitLength = 0;
    ReportYBitLength = 0;
    ReportWheelBitLength = 0;

    while ((Downstream_HID_GetNextReportItem() == HAL_OK) && ((ReportButtonBitLength == 0) ||
                                                              (ReportXBitLength == 0)      ||
                                                              (ReportYBitLength == 0)      ||
                                                              (ReportWheelBitLength == 0)))
    {
        switch (ItemHeader)
        {
        case HID_ITEM_USAGE_PAGE:
            currentUsagePage = ItemData;
            currentUsageIndex = 0;
            xUsageIndex = 0xFF;
            yUsageIndex = 0xFF;
            wheelUsageIndex = 0xFF;
            break;

        case HID_ITEM_COLLECTION:               //Physical collections also clear the usage index...
            if (ItemData == HID_ITEM_COLLECTION_PHYS)
            {
                currentUsageIndex = 0;
                xUsageIndex = 0xFF;
                yUsageIndex = 0xFF;
                wheelUsageIndex = 0xFF;
            }
            break;

        case HID_ITEM_END_COLLECTION:           //...and so do collection ends
            currentUsageIndex = 0;
            xUsageIndex = 0xFF;
            yUsageIndex = 0xFF;
            wheelUsageIndex = 0xFF;
            break;

        case HID_ITEM_USAGE:
            switch (ItemData)
            {
            case HID_ITEM_USAGE_X:
                xUsageIndex = currentUsageIndex;
                break;

            case HID_ITEM_USAGE_Y:
                yUsageIndex = currentUsageIndex;
                break;

            case HID_ITEM_USAGE_WHEEL:
                wheelUsageIndex = currentUsageIndex;
                break;
            }
            currentUsageIndex++;
            break;

        case HID_ITEM_REPORT_SIZE:
            currentReportSize = ItemData;
            break;

        case HID_ITEM_REPORT_COUNT:
            currentReportCount = ItemData;
            break;

        case HID_ITEM_REPORT_ID:
            currentReportBitIndex += 8;         //Rudimentary support for a single report ID
            break;

        case HID_ITEM_INPUT:
            switch (currentUsagePage)
            {
            case HID_ITEM_USAGE_PAGE_BUTTON:
                if (ItemData == HID_ITEM_INPUT_ABS)
                {
                    //Buttons found!
                    if (currentReportSize != 1)
                    {
                        return HAL_ERROR;
                    }
                    ReportButtonBitOffset = currentReportBitIndex;
                    ReportButtonBitLength = currentReportCount;
                    if (ReportButtonBitLength > HID_MOUSE_MAX_BUTTONS)
                    {
                        ReportButtonBitLength = HID_MOUSE_MAX_BUTTONS;
                    }
                }
                break;

            case HID_ITEM_USAGE_PAGE_DESKTOP:
                if (ItemData == HID_ITEM_INPUT_REL)
                {
                    //Movement data found!
                    if ((currentReportSize < 8) || (currentReportSize > 16))
                    {
                        return HAL_ERROR;
                    }
                    if (xUsageIndex != 0xFF)
                    {
                        ReportXBitOffset = currentReportBitIndex + (currentReportSize * xUsageIndex);
                        ReportXBitLength = currentReportSize;
                        if ((ReportXBitOffset + ReportXBitLength) > (HID_MAX_REPORT_LEN * 8))
                        {
                            return HAL_ERROR;
                        }
                    }
                    if (yUsageIndex != 0xFF)
                    {
                        ReportYBitOffset = currentReportBitIndex + (currentReportSize * yUsageIndex);
                        ReportYBitLength = currentReportSize;
                        if ((ReportYBitOffset + ReportYBitLength) > (HID_MAX_REPORT_LEN * 8))
                        {
                            return HAL_ERROR;
                        }
                    }
                    if (wheelUsageIndex != 0xFF)
                    {
                        ReportWheelBitOffset = currentReportBitIndex + (currentReportSize * wheelUsageIndex);
                        ReportWheelBitLength = currentReportSize;
                        if ((ReportWheelBitOffset + ReportWheelBitLength) > (HID_MAX_REPORT_LEN * 8))
                        {
                            return HAL_ERROR;
                        }
                    }
                }
                break;
            }
            currentUsageIndex = 0;
            xUsageIndex = 0xFF;
            yUsageIndex = 0xFF;
            wheelUsageIndex = 0xFF;
            currentReportBitIndex += (currentReportSize * currentReportCount);
            break;
        }
    }

    //We don't mind if we didn't find a scrollwheel item.
    if ((ReportButtonBitLength == 0) ||
        (ReportXBitLength == 0) ||
        (ReportYBitLength == 0))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}


//Retrieves the next item in the HID report, and at most one of its associated data bytes.
//Then it updates ReportDataPointer based on the actual length of the retrieved item.
static HAL_StatusTypeDef Downstream_HID_GetNextReportItem(void)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;
    uint32_t itemLength;

    if ((ReportDataPointer >= &hUsbHostFS.device.Data[USBH_MAX_DATA_BUFFER]) ||
        ((ReportDataPointer - &hUsbHostFS.device.Data[0]) >= HID_Handle->HID_Desc.wItemLength))
    {
        return HAL_ERROR;
    }

    ItemHeader = *ReportDataPointer & HID_ITEM_MASK;
    itemLength = *ReportDataPointer & HID_ITEM_LENGTH_MASK;
    ReportDataPointer++;

    if (itemLength == 3)
    {
        itemLength = 4;                         //Length = 3 actually means 4 bytes
    }

    if (ItemHeader == HID_ITEM_LONG)
    {
        itemLength += *ReportDataPointer;       //Long items have another length byte
    }
    else
    {
        if (itemLength > 0)
        {
            ItemData = *ReportDataPointer;      //If it is a short item, grab its first data byte
        }
    }

    ReportDataPointer += itemLength;
    return HAL_OK;
}
#endif  //#ifdef CONFIG_MOUSE_ENABLED



void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
    if (receivedPacket->Command == COMMAND_HID_GET_REPORT)
    {
        Downstream_ReleasePacket(receivedPacket);
        if (USBH_HID_GetInterruptReport(&hUsbHostFS,
                                        Downstream_HID_InterruptReportCallback) != HAL_OK)
        {
            Downstream_PacketProcessor_FreakOut();
        }
        Downstream_PacketProcessor_NotifyDisconnectReplyRequired();
        return;
    }

#ifdef CONFIG_KEYBOARD_ENABLED
    if (receivedPacket->Command == COMMAND_HID_SET_REPORT)
    {
        if ((ConfiguredDeviceClass    != COMMAND_CLASS_HID_KEYBOARD) ||
            (receivedPacket->Length16 != ((HID_KEYBOARD_OUTPUT_DATA_LEN + 1) / 2) + DOWNSTREAM_PACKET_HEADER_LEN_16) ||
            ((receivedPacket->Data[0] & ~((1 << HID_KEYBOARD_MAX_LED) - 1)) != 0))
        {
            Downstream_PacketProcessor_FreakOut();
        }
        USBH_HID_SetReport(&hUsbHostFS,
                           HID_REPORT_DIRECTION_OUT,
                           0,
                           receivedPacket->Data,
                           HID_KEYBOARD_OUTPUT_DATA_LEN,
                           Downstream_HID_SendReportCallback);
        Downstream_ReleasePacket(receivedPacket);
        Downstream_PacketProcessor_NotifyDisconnectReplyRequired();
        return;
    }
#endif  //#ifdef CONFIG_KEYBOARD_ENABLED

    //else:
    Downstream_PacketProcessor_FreakOut();
}


void Downstream_HID_InterruptReportCallback(USBH_StatusTypeDef result)
{
    DownstreamPacketTypeDef* freePacket;

    freePacket = Downstream_GetFreePacketImmediately();

    if (result == USBH_OK)
    {
        //Data received from device
#ifdef CONFIG_MOUSE_ENABLED
        if (ConfiguredDeviceClass == COMMAND_CLASS_HID_MOUSE)
        {
            Downstream_HID_Mouse_ExtractDataFromReport(freePacket);
            freePacket->Length16 = ((HID_MOUSE_INPUT_DATA_LEN + 1) / 2) + DOWNSTREAM_PACKET_HEADER_LEN_16;
        }
        else
#endif
#ifdef CONFIG_KEYBOARD_ENABLED
        if (ConfiguredDeviceClass == COMMAND_CLASS_HID_KEYBOARD)
        {
            Downstream_HID_Keyboard_ExtractDataFromReport(freePacket);
            freePacket->Length16 = ((HID_KEYBOARD_INPUT_DATA_LEN + 1) / 2) + DOWNSTREAM_PACKET_HEADER_LEN_16;
        }
        else
#endif

        //else if...
        {
            Downstream_PacketProcessor_FreakOut();
            return;
        }
    }
    else
    {
        //NAK received from device, return zero-length packet
        freePacket->Length16 = DOWNSTREAM_PACKET_HEADER_LEN_16;
    }

    freePacket->CommandClass = ConfiguredDeviceClass;
    freePacket->Command = COMMAND_HID_GET_REPORT;
    Downstream_PacketProcessor_ClassReply(freePacket);
}


#ifdef CONFIG_MOUSE_ENABLED
static void Downstream_HID_Mouse_ExtractDataFromReport(DownstreamPacketTypeDef* packetToSend)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;
    uint32_t readData;

    readData = *(uint32_t*)&(HID_Handle->Data[(ReportButtonBitOffset / 8)]);
    readData >>= (ReportButtonBitOffset % 8);
    readData &= ((1 << ReportButtonBitLength) - 1);             //Truncate extra buttons
    packetToSend->Data[0] = readData;

    packetToSend->Data[1] = Downstream_HID_Mouse_Extract8BitValue(HID_Handle,
                                                                  ReportXBitOffset,
                                                                  ReportXBitLength);

    packetToSend->Data[2] = Downstream_HID_Mouse_Extract8BitValue(HID_Handle,
                                                                  ReportYBitOffset,
                                                                  ReportYBitLength);

    packetToSend->Data[3] = Downstream_HID_Mouse_Extract8BitValue(HID_Handle,
                                                                  ReportWheelBitOffset,
                                                                  ReportWheelBitLength);
}



static uint8_t Downstream_HID_Mouse_Extract8BitValue(HID_HandleTypeDef* hidHandle,
                                                     uint8_t valueBitOffset,
                                                     uint8_t valueBitLength)
{
    int32_t readData;

    readData = *(uint32_t*)&(hidHandle->Data[(valueBitOffset / 8)]);
    readData >>= (valueBitOffset % 8);
    if (readData & (1 << (valueBitLength - 1)))             //If value is negative...
    {
        readData |= ~((1 << valueBitLength) - 1);           //...sign-extend to full width...
    }
    else
    {
        readData &= (1 << valueBitLength) - 1;              //...else zero out unwanted bits
    }
    if (readData < INT8_MIN) readData = INT8_MIN;           //Limit to 8-bit values
    if (readData > INT8_MAX) readData = INT8_MAX;
    return (int8_t)readData;
}
#endif  //#ifdef CONFIG_MOUSE_ENABLED


#ifdef CONFIG_KEYBOARD_ENABLED
static void Downstream_HID_Keyboard_ExtractDataFromReport(DownstreamPacketTypeDef* packetToSend)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;
    uint32_t i;
    uint8_t readData;

    packetToSend->Data[0] = HID_Handle->Data[0];        //Modifier keys - pass straight through
    packetToSend->Data[1] = 0;                          //Constant byte

    for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)   //Key array
    {
        readData = HID_Handle->Data[i];
        if (readData > HID_KEYBOARD_MAX_KEY)
        {
            readData = HID_KEYBOARD_MAX_KEY;
        }
        packetToSend->Data[i] = readData;
    }
}
#endif  //#ifdef CONFIG_KEYBOARD_ENABLED



void Downstream_HID_SendReportCallback(USBH_StatusTypeDef result)
{
    UNUSED(result);

    DownstreamPacketTypeDef* freePacket;

    freePacket = Downstream_GetFreePacketImmediately();
    freePacket->Length16 = DOWNSTREAM_PACKET_HEADER_LEN_16;
    freePacket->CommandClass = ConfiguredDeviceClass;
    freePacket->Command = COMMAND_HID_SET_REPORT;
    Downstream_PacketProcessor_ClassReply(freePacket);
}


#endif  //#if defined (CONFIG_KEYBOARD_ENABLED) || defined (CONFIG_MOUSE_ENABLED)

