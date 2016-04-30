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



#define HID_MAX_REPORT_LEN      8
#define HID_MOUSE_DATA_LEN      4
#define HID_KEYBOARD_DATA_LEN   0

#define HID_MOUSE_MAX_BUTTONS   4



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



InterfaceCommandClassTypeDef Downstream_HID_ApproveConnectedDevice(void)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;

    if (HID_Handle->Protocol != HID_MOUSE_BOOT_CODE)
    {
        return COMMAND_CLASS_INTERFACE;             //fail
    }

    if (Downstream_HID_Mouse_ParseReportDescriptor() != HAL_OK)
    {
        return COMMAND_CLASS_INTERFACE;             //fail
    }

    return COMMAND_CLASS_HID_MOUSE;                 //success!
}



HAL_StatusTypeDef Downstream_HID_Mouse_ParseReportDescriptor(void)
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
                    }
                    if (yUsageIndex != 0xFF)
                    {
                        ReportYBitOffset = currentReportBitIndex + (currentReportSize * yUsageIndex);
                        ReportYBitLength = currentReportSize;
                    }
                    if (wheelUsageIndex != 0xFF)
                    {
                        ReportWheelBitOffset = currentReportBitIndex + (currentReportSize * wheelUsageIndex);
                        ReportWheelBitLength = currentReportSize;
                    }
                }
                break;
            }
            currentReportBitIndex += (currentReportSize * currentReportCount);
            if (currentReportBitIndex >= (HID_MAX_REPORT_LEN * 8))
            {
                return HAL_ERROR;
            }
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
HAL_StatusTypeDef Downstream_HID_GetNextReportItem(void)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;
    uint32_t itemLength;

    if ((ReportDataPointer >= &hUsbHostFS.device.Data[USBH_MAX_DATA_BUFFER]) ||
        ((ReportDataPointer - &hUsbHostFS.device.Data[0]) >= HID_Handle->HID_Desc.wItemLength))
    {
        return HAL_ERROR;
    }

    ItemHeader = *ReportDataPointer++;
    itemLength = ItemHeader & HID_ITEM_LENGTH_MASK;

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



void Downstream_HID_PacketProcessor(DownstreamPacketTypeDef* receivedPacket)
{
    if (receivedPacket->Command != COMMAND_HID_REPORT)
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    Downstream_ReleasePacket(receivedPacket);
    if (USBH_HID_GetInterruptReport(&hUsbHostFS,
                                    Downstream_HID_InterruptReportCallback) != HAL_OK)
    {
        Downstream_PacketProcessor_FreakOut();
    }
    Downstream_PacketProcessor_NotifyDisconnectReplyRequired();
}


void Downstream_HID_InterruptReportCallback(DownstreamPacketTypeDef* packetToSend)
{
    if (ConfiguredDeviceClass == COMMAND_CLASS_HID_MOUSE)
    {
        Downstream_HID_Mouse_ExtractDataFromReport(packetToSend);
        packetToSend->Length16 = ((HID_MOUSE_DATA_LEN + 1) / 2) + DOWNSTREAM_PACKET_HEADER_LEN_16;
    }
    //else if...
    else
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    packetToSend->CommandClass = ConfiguredDeviceClass;
    packetToSend->Command = COMMAND_HID_REPORT;
    Downstream_PacketProcessor_ClassReply(packetToSend);
}


void Downstream_HID_Mouse_ExtractDataFromReport(DownstreamPacketTypeDef* packetToSend)
{
    HID_HandleTypeDef* HID_Handle =  (HID_HandleTypeDef*)hUsbHostFS.pActiveClass->pData;
    uint16_t readData;

    //Grab the buttons
    readData = *(uint16_t*)&(HID_Handle->Data[(ReportButtonBitOffset / 8)]);
    readData >>= (ReportButtonBitOffset % 8);
    readData &= ((1 << ReportButtonBitLength) - 1);
    packetToSend->Data[0] = readData;

    //Grab X
    readData = *(uint16_t*)&(HID_Handle->Data[(ReportXBitOffset / 8)]);
    readData >>= (ReportXBitOffset % 8);
    packetToSend->Data[1] = (uint8_t)readData;

    //Grab Y
    readData = *(uint16_t*)&(HID_Handle->Data[(ReportYBitOffset / 8)]);
    readData >>= (ReportYBitOffset % 8);
    packetToSend->Data[2] = (uint8_t)readData;

    //Grab wheel
    readData = *(uint16_t*)&(HID_Handle->Data[(ReportWheelBitOffset / 8)]);
    readData >>= (ReportWheelBitOffset % 8);
    packetToSend->Data[3] = (uint8_t)readData;
}


