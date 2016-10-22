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

KeyboardOutStateTypeDef         KeyboardOutDataState = KEYBOARD_OUT_STATE_IDLE;
uint8_t                         KeyboardOutData[HID_KEYBOARD_OUTPUT_DATA_LEN];

uint8_t                         GetReportLoopIsRunning = 0;


static void Upstream_HID_ReceiveInterruptReport(void);
static void Upstream_HID_ReceiveInterruptReportCallback(UpstreamPacketTypeDef* receivedPacket);
static void Upstream_HID_SendControlReport(void);
static void Upstream_HID_SendControlReportCallback(UpstreamPacketTypeDef* receivedPacket);



void Upstream_HID_DeInit(void)
{
    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }
    GetReportCallback = NULL;
    KeyboardOutDataState = KEYBOARD_OUT_STATE_IDLE;
}



//Called by usbd_hid to request our next interrupt report
void Upstream_HID_GetInterruptReport(UpstreamHidGetReportCallback callback)
{
    GetReportCallback = callback;

    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }

    //Wakeup interrupts are apparently broken.
    //So we check for resume when the host sends us something here.
    Upstream_StateMachine_CheckResume();

    //Start our internal loop?
    if (!GetReportLoopIsRunning)
    {
        GetReportLoopIsRunning = 1;
        Upstream_HID_ReceiveInterruptReport();
    }
}


//Our internal report request loop
static void Upstream_HID_ReceiveInterruptReport(void)
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
        Upstream_ReceivePacket(Upstream_HID_ReceiveInterruptReportCallback);
    }
    else
    {
        Upstream_ReleasePacket(freePacket);
    }

}



static void Upstream_HID_ReceiveInterruptReportCallback(UpstreamPacketTypeDef* receivedPacket)
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

    if (receivedPacket == NULL)         //Error receiving packet
    {
        return;                         //Just give up...
    }

    if (receivedPacket->Length16 == UPSTREAM_PACKET_HEADER_LEN_16)
    {
        //Zero-length reply indicates no data from downstream device
        Upstream_ReleasePacket(receivedPacket);
    }
    else
    {
        if (activeClass == COMMAND_CLASS_HID_MOUSE)
        {
            if (receivedPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + ((HID_MOUSE_INPUT_DATA_LEN + 1) / 2)))
            {
                UPSTREAM_STATEMACHINE_FREAKOUT;
                return;
            }
            dataLength = HID_MOUSE_INPUT_DATA_LEN;
            if ((receivedPacket->Data[0] & ~((1 << HID_MOUSE_MAX_BUTTONS) - 1)) != 0)      //Check number of buttons received
            {
                UPSTREAM_STATEMACHINE_FREAKOUT;
                return;
            }

            //Mouse wakeup is triggered by button clicks only
            if (receivedPacket->Data[0] != 0)
            {
                if (Upstream_StateMachine_GetSuspendState())
                {
                    Upstream_StateMachine_Wakeup();         //Send wakeup signal to host
                }
            }

            //Other mouse sanity checks & stuff go here...
        }

        else if (activeClass == COMMAND_CLASS_HID_KEYBOARD)
        {
            if (receivedPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + ((HID_KEYBOARD_INPUT_DATA_LEN + 1) / 2)))
            {
                UPSTREAM_STATEMACHINE_FREAKOUT;
                return;
            }
            dataLength = HID_KEYBOARD_INPUT_DATA_LEN;

            if (receivedPacket->Data[1] != 0)
            {
                UPSTREAM_STATEMACHINE_FREAKOUT;
                return;
            }
            for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
            {
                if (receivedPacket->Data[i] > HID_KEYBOARD_MAX_KEY)
                {
                    UPSTREAM_STATEMACHINE_FREAKOUT;
                    return;
                }
            }

            if (Upstream_StateMachine_GetSuspendState())
            {
                Upstream_StateMachine_Wakeup();         //Send wakeup signal to host
            }

            //Other keyboard sanity checks here...
        }

        //Other HID classes go here...
        else
        {
            UPSTREAM_STATEMACHINE_FREAKOUT;
            return;
        }

        if ((GetReportCallback == NULL) ||
            (UpstreamHidPacket != NULL) ||
            (Upstream_StateMachine_GetSuspendState()))
        {
            Upstream_ReleasePacket(receivedPacket);
        }
        else
        {
            //Send resulting data to host
            UpstreamHidPacket = receivedPacket;         //Save packet so we can free it when upstream USB transaction is done
            tempReportCallback = GetReportCallback;
            GetReportCallback = NULL;
            tempReportCallback(receivedPacket->Data, dataLength);
        }
    }

    //Check if we need to send OUT data to the keyboard before requesting next Interrupt IN data
    if (KeyboardOutDataState == KEYBOARD_OUT_STATE_DATA_READY)
    {
        Upstream_HID_SendControlReport();
    }
    else
    {
        Upstream_HID_ReceiveInterruptReport();      //Otherwise poll downstream again
    }
}



void Upstream_HID_RequestSendControlReport(UpstreamPacketTypeDef* packetToSend, uint8_t dataLength)
{
    InterfaceCommandClassTypeDef activeClass;
    uint32_t i;

    activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((packetToSend == NULL) ||
        (activeClass  != COMMAND_CLASS_HID_KEYBOARD) ||
        (dataLength   != HID_KEYBOARD_OUTPUT_DATA_LEN))
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    //Save data until after the next interrupt data is received from Downstream
    KeyboardOutDataState = KEYBOARD_OUT_STATE_DATA_READY;
    for (i = 0; i < HID_KEYBOARD_OUTPUT_DATA_LEN; i++)
    {
        KeyboardOutData[i] = packetToSend->Data[i];
    }
}



static void Upstream_HID_SendControlReport(void)
{
    UpstreamPacketTypeDef* freePacket;
    uint32_t i;

    KeyboardOutDataState = KEYBOARD_OUT_STATE_BUSY;

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

    if (Upstream_TransmitPacket(freePacket) == HAL_OK)
    {
        Upstream_ReceivePacket(Upstream_HID_SendControlReportCallback);
    }
    else
    {
        Upstream_ReleasePacket(freePacket);
    }
}



static void Upstream_HID_SendControlReportCallback(UpstreamPacketTypeDef* receivedPacket)
{
    InterfaceCommandClassTypeDef activeClass;

    activeClass = Upstream_StateMachine_CheckActiveClass();
    if (activeClass != COMMAND_CLASS_HID_KEYBOARD)        //add classes here
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    if (receivedPacket == NULL)
    {
        return;             //Just give up...
    }

    Upstream_ReleasePacket(receivedPacket);
    KeyboardOutDataState = KEYBOARD_OUT_STATE_IDLE;
    Upstream_HID_ReceiveInterruptReport();
}



