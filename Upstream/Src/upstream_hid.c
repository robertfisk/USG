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
#include "upstream_spi.h"
#include "upstream_interface_def.h"
#include "usbd_hid.h"


#define HID_MOUSE_DATA_LEN      4
#define HID_KEYBOARD_DATA_LEN   0


UpstreamPacketTypeDef*          UpstreamHidPacket = NULL;
UpstreamHidSendReportCallback   ReportCallback    = NULL;



void Upstream_HID_GetNextReportReceiveCallback(UpstreamPacketTypeDef* receivedPacket);



void Upstream_HID_DeInit(void)
{
    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }
}



HAL_StatusTypeDef Upstream_HID_GetNextReport(UpstreamHidSendReportCallback callback)
{
	UpstreamPacketTypeDef* freePacket;
	InterfaceCommandClassTypeDef activeClass;

	activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((activeClass != COMMAND_CLASS_HID_MOUSE))      //add classes here
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return HAL_ERROR;
    }

    //Release packet used for last transaction (if any)
    if (UpstreamHidPacket != NULL)
    {
        Upstream_ReleasePacket(UpstreamHidPacket);
        UpstreamHidPacket = NULL;
    }

    if (ReportCallback != NULL)
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return HAL_ERROR;
    }
    ReportCallback = callback;

    freePacket = Upstream_GetFreePacketImmediately();
    if (freePacket == NULL)
    {
        return HAL_ERROR;
    }

	freePacket->Length16 = UPSTREAM_PACKET_HEADER_LEN_16;
	freePacket->CommandClass = activeClass;
	freePacket->Command = COMMAND_HID_REPORT;

	if (Upstream_TransmitPacket(freePacket) == HAL_OK)
	{
		return Upstream_ReceivePacket(Upstream_HID_GetNextReportReceiveCallback);
	}

	//else:
	Upstream_ReleasePacket(freePacket);
	return HAL_ERROR;
}



void Upstream_HID_GetNextReportReceiveCallback(UpstreamPacketTypeDef* receivedPacket)
{
    UpstreamHidSendReportCallback tempReportCallback;
	InterfaceCommandClassTypeDef activeClass;
    uint8_t dataLength = 0;
    uint8_t i;

	activeClass = Upstream_StateMachine_CheckActiveClass();
    if ((activeClass != COMMAND_CLASS_HID_MOUSE))      //add classes here
    {
        UPSTREAM_STATEMACHINE_FREAKOUT;
        return;
    }

    if ((UpstreamHidPacket != NULL) ||
        (ReportCallback == NULL))
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
        if (receivedPacket->Length16 != (UPSTREAM_PACKET_HEADER_LEN_16 + ((HID_MOUSE_DATA_LEN + 1) / 2)))
        {
            UPSTREAM_SPI_FREAKOUT;
            return;
        }

        //Mouse sanity checks & stuff go here...

        dataLength = HID_MOUSE_DATA_LEN;
    }

    //Other HID classes go here...


    if (dataLength == 0)
    {
        UPSTREAM_SPI_FREAKOUT;
        return;
    }

    for (i = dataLength; i < HID_EPIN_SIZE; i++)
    {
        receivedPacket->Data[i] = 0;            //Zero out unused bytes before we send report upstream
    }

    UpstreamHidPacket = receivedPacket;         //Save packet so we can free it when upstream USB transaction is done
    tempReportCallback = ReportCallback;
    ReportCallback = NULL;
    tempReportCallback(receivedPacket->Data, HID_EPIN_SIZE);
}


