/*
 * upstream_hid.c
 *
 *  Created on: Jan 16, 2016
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */



#include "upstream_hid.h"
#include "upstream_interface_def.h"



UpstreamPacketTypeDef*          UpstreamHidPacket = NULL;
UpstreamHidGetReportCallback    GetReportCallback = NULL;

uint8_t     KeyboardOutDataAvailable = 0;
uint8_t     KeyboardOutData[HID_KEYBOARD_OUTPUT_DATA_LEN];



void Upstream_HID_GetNextInterruptReportReceiveCallback(UpstreamPacketTypeDef* receivedPacket);



void Upstream_HID_DeInit(void)
{
    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }
    GetReportCallback = NULL;
    KeyboardOutDataAvailable = 0;
}



void Upstream_HID_GetNextInterruptReport(UpstreamHidGetReportCallback callback)
{
	UpstreamPacketTypeDef* freePacket;
	InterfaceCommandClassTypeDef activeClass;

	activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((activeClass != COMMAND_CLASS_HID_MOUSE) &&
        (activeClass != COMMAND_CLASS_HID_KEYBOARD))      //add classes here
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    //Just return if we already have an outstanding request
    if (GetReportCallback != NULL)
    {
        return;
    }
    GetReportCallback = callback;

    //Release packet used for last transaction (if any)
    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }

    freePacket = Upstream_GetFreePacketImmediately();
    if (freePacket == NULL)
    {
        return;
    }

	freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
	freePacket->CommandClass = activeClass;
	freePacket->Command = COMMAND_HID_GET_REPORT;

	if (Upstream_TransmitPacket(freePacket) == HAL_OK)
	{
		Upstream_ReceivePacket(Upstream_HID_GetNextInterruptReportReceiveCallback);
	}
	else
	{
	    Upstream_ReleasePacket(freePacket);
	}
}



void Upstream_HID_GetNextInterruptReportReceiveCallback(UpstreamPacketTypeDef* receivedPacket)
{
    UpstreamHidGetReportCallback tempReportCallback;
	InterfaceCommandClassTypeDef activeClass;
    uint32_t i;
	uint8_t dataLength;


	activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((activeClass != COMMAND_CLASS_HID_MOUSE) &&
        (activeClass != COMMAND_CLASS_HID_KEYBOARD))        //add classes here
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    if ((UpstreamHidPacket != NULL) ||
        (GetReportCallback == NULL))
    {
        UPSTREAM_SPI_FREAKOUT;
        return;
    }

    if (receivedPacket == NULL)
    {
        return;             //Just give up...
    }


    if (activeClass == COMMAND_CLASS_HID_MOUSE)
    {
        if (receivedPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + ((HID_MOUSE_INPUT_DATA_LEN + 1) / 2)))
        {
            UPSTREAM_SPI_FREAKOUT;
            return;
        }
        dataLength = HID_MOUSE_INPUT_DATA_LEN;
        if ((receivedPacket->Data[0] & ~((1 << HID_MOUSE_MAX_BUTTONS) - 1)) != 0)      //Check number of buttons received
        {
            UPSTREAM_SPI_FREAKOUT;
            return;
        }
        //Other mouse sanity checks & stuff go here...
    }

    else if (activeClass == COMMAND_CLASS_HID_KEYBOARD)
    {
        if (receivedPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + ((HID_KEYBOARD_INPUT_DATA_LEN + 1) / 2)))
        {
            UPSTREAM_SPI_FREAKOUT;
            return;
        }
        dataLength = HID_KEYBOARD_INPUT_DATA_LEN;

        if (receivedPacket->Data[1] != 0)
        {
            UPSTREAM_SPI_FREAKOUT;
            return;
        }
        for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
        {
            if (receivedPacket->Data[i] > HID_KEYBOARD_MAX_KEY)
            {
                UPSTREAM_SPI_FREAKOUT;
                return;
            }
        }
        //Other keyboard sanity checks here...
    }

    //Other HID classes go here...

    else
    {
        UPSTREAM_SPI_FREAKOUT;
        return;
    }

    UpstreamHidPacket = receivedPacket;         //Save packet so we can free it when upstream USB transaction is done
    tempReportCallback = GetReportCallback;
    GetReportCallback = NULL;
    tempReportCallback(receivedPacket->Data, dataLength);


    //Check if we need to send OUT data to the keyboard before requesting next Interrupt IN data
    if (KeyboardOutDataAvailable)
    {
        Upstream_HID_ReallySendControlReport();
    }
}



void Upstream_HID_SendControlReport(UpstreamPacketTypeDef* packetToSend, uint8_t dataLength)
{
    InterfaceCommandClassTypeDef activeClass;
    uint32_t i;

    activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((packetToSend == NULL) ||
        (activeClass  != COMMAND_CLASS_HID_KEYBOARD) ||
        (dataLength   != HID_KEYBOARD_OUTPUT_DATA_LEN))
    {
        UPSTREAM_SPI_FREAKOUT;
        return;
    }

    if (GetReportCallback == NULL)
    {
        while(1);           //checkme!
    }

    //Save data until after the next interrupt data is received from Downstream
    KeyboardOutDataAvailable = 1;
    for (i = 0; i < HID_KEYBOARD_OUTPUT_DATA_LEN; i++)
    {
        KeyboardOutData[i] = packetToSend->Data[i];
    }
}



void Upstream_HID_ReallySendControlReport(void)
{
    UpstreamPacketTypeDef* freePacket;
    uint32_t i;

    KeyboardOutDataAvailable = 0;

    freePacket = Upstream_GetFreePacketImmediately();
    if (freePacket == NULL) return;

    freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16  + ((HID_KEYBOARD_OUTPUT_DATA_LEN + 1) / 2);
    freePacket->CommandClass = COMMAND_CLASS_HID_KEYBOARD;
    freePacket->Command = COMMAND_HID_SET_REPORT;

    for (i = 0; i < HID_KEYBOARD_OUTPUT_DATA_LEN; i++)
    {
        freePacket->Data[i] = KeyboardOutData[i];
    }
    freePacket->Data[0] &= ((1 << HID_KEYBOARD_MAX_LED) - 1);

    if (Upstream_TransmitPacket(freePacket) != HAL_OK)
    {
        Upstream_ReleasePacket(freePacket);
    }
}


