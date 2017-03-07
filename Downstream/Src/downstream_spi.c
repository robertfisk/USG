/*
 * upstream_spi.c
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "downstream_interface_def.h"
#include "downstream_spi.h"
#include "downstream_statemachine.h"
#include "board_config.h"
#include "led.h"


SPI_HandleTypeDef           Hspi1;
DownstreamPacketTypeDef     DownstreamPacket0;
DownstreamPacketTypeDef     DownstreamPacket1;
DownstreamPacketTypeDef*    CurrentWorkingPacket;
DownstreamPacketTypeDef*    NextTxPacket            = NULL;

InterfaceStateTypeDef               DownstreamInterfaceState    = DOWNSTREAM_INTERFACE_IDLE;
FreePacketCallbackTypeDef           PendingFreePacketCallback   = NULL; //Indicates someone is waiting for a packet buffer to become available
SpiPacketReceivedCallbackTypeDef    ReceivePacketCallback       = NULL; //Indicates someone is waiting for a received packet

uint32_t                    TemporaryIncomingPacketLength = 0;
uint8_t                     SpiInterruptCompleted = 0;


HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void);
void Downstream_PrepareReceivePacketSize(DownstreamPacketTypeDef* freePacket);



void Downstream_InitSPI(void)
{
    DownstreamPacket0.Busy = NOT_BUSY;
    DownstreamPacket1.Busy = NOT_BUSY;

    Hspi1.Instance = SPI1;
    Hspi1.Init.Mode = SPI_MODE_SLAVE;
    Hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    Hspi1.Init.DataSize = SPI_DATASIZE_16BIT;
    Hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    Hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    Hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
    Hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    Hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    Hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
    Hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
    Hspi1.Init.CRCPolynomial = SPI_CRC_DEFAULTPOLYNOMIAL;
    HAL_SPI_Init(&Hspi1);
}


//Used by downstream state machine and USB host classes.
HAL_StatusTypeDef Downstream_GetFreePacket(FreePacketCallbackTypeDef callback)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return HAL_ERROR;
    }

    //Do we already have a queued callback?
    if (PendingFreePacketCallback != NULL)
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }

    //Check if there is a free buffer now
    if (DownstreamPacket0.Busy == NOT_BUSY)
    {
        DownstreamPacket0.Busy = BUSY;
        callback(&DownstreamPacket0);
        return HAL_OK;
    }
    if (DownstreamPacket1.Busy == NOT_BUSY)
    {
        DownstreamPacket1.Busy = BUSY;
        callback(&DownstreamPacket1);
        return HAL_OK;
    }

    //Otherwise save requested address for when a buffer becomes free in the future
    PendingFreePacketCallback = callback;
    return HAL_OK;
}



DownstreamPacketTypeDef* Downstream_GetFreePacketImmediately(void)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return NULL;
    }

    //We are expecting a free buffer now
    if (DownstreamPacket0.Busy == NOT_BUSY)
    {
        DownstreamPacket0.Busy = BUSY;
        return &DownstreamPacket0;
    }
    if (DownstreamPacket1.Busy == NOT_BUSY)
    {
        DownstreamPacket1.Busy = BUSY;
        return &DownstreamPacket1;
    }

    //Should not happen:
    DOWNSTREAM_SPI_FREAKOUT;
    return NULL;
}



//Used by Downstream state machine and USB host classes.
void Downstream_ReleasePacket(DownstreamPacketTypeDef* packetToRelease)
{
    FreePacketCallbackTypeDef tempCallback;

    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return;
    }

    if ((packetToRelease != &DownstreamPacket0) &&
        (packetToRelease != &DownstreamPacket1))
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return;
    }

    if (PendingFreePacketCallback != NULL)
    {
        tempCallback = PendingFreePacketCallback;   //In extreme situations, running this callback can trigger another request for a free packet,
        PendingFreePacketCallback = NULL;           //thereby causing GetFreePacket to freak out. So we need to clear the callback indicator first.
        tempCallback(packetToRelease);
    }
    else
    {
        packetToRelease->Busy = NOT_BUSY;
    }
}


//Used by Downstream state machine and USB classes.
//Ok to call when idle or transmitting.
//Not OK to call when receiving or awaiting reception.
HAL_StatusTypeDef Downstream_ReceivePacket(SpiPacketReceivedCallbackTypeDef callback)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return HAL_ERROR;
    }

    if ((DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_SIZE_WAIT) ||
        (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_PACKET_WAIT) ||
        (ReceivePacketCallback != NULL))
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }

    ReceivePacketCallback = callback;
    return Downstream_CheckPreparePacketReception();
}


//Internal use only
HAL_StatusTypeDef Downstream_CheckPreparePacketReception(void)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return HAL_ERROR;
    }

    if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_IDLE)
    {
        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_RX_SIZE_WAIT;
        return Downstream_GetFreePacket(Downstream_PrepareReceivePacketSize);
    }
    return HAL_OK;
}


//Internal use only
void Downstream_PrepareReceivePacketSize(DownstreamPacketTypeDef* freePacket)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return;
    }

    if (DownstreamInterfaceState != DOWNSTREAM_INTERFACE_RX_SIZE_WAIT)
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return;
    }
    CurrentWorkingPacket = freePacket;
    CurrentWorkingPacket->Length16 = 0;
    if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
                                    (uint8_t*)&CurrentWorkingPacket->Length16,
                                    (uint8_t*)&CurrentWorkingPacket->Length16,
                                    2) != HAL_OK)       //We only need to read one word, but the peripheral library freaks out...
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return;
    }

    UPSTREAM_TX_REQUEST_ASSERT;
}



//Used by Downstream state machine and USB classes.
//Call when idle or transmitting.
//It doesn't make sense to call when receiving or awaiting reception.
HAL_StatusTypeDef Downstream_TransmitPacket(DownstreamPacketTypeDef* packetToWrite)
{
    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return HAL_ERROR;
    }

    //Sanity checks
    if ((packetToWrite != &DownstreamPacket0) &&
        (packetToWrite != &DownstreamPacket1))
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }
    if ((packetToWrite->Busy != BUSY) ||
        (packetToWrite->Length16 < DOWNSTREAM_PACKET_LEN_MIN_16) ||
        (packetToWrite->Length16 > DOWNSTREAM_PACKET_LEN_16))
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }
    if (NextTxPacket != NULL)
    {
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }

    switch (DownstreamInterfaceState)
    {
    case DOWNSTREAM_INTERFACE_TX_SIZE_WAIT:
    case DOWNSTREAM_INTERFACE_TX_PACKET_WAIT:
        NextTxPacket = packetToWrite;
        break;

    case DOWNSTREAM_INTERFACE_IDLE:
        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_SIZE_WAIT;
        CurrentWorkingPacket = packetToWrite;

        if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
                                        (uint8_t*)&CurrentWorkingPacket->Length16,
                                        (uint8_t*)&TemporaryIncomingPacketLength,
                                        2) != HAL_OK)       //We only need to write one word, but the peripheral library freaks out...
        {
            DOWNSTREAM_SPI_FREAKOUT;
            return HAL_ERROR;
        }
        UPSTREAM_TX_REQUEST_ASSERT;
        break;

    default:
        DOWNSTREAM_SPI_FREAKOUT;
        return HAL_ERROR;
    }

    return HAL_OK;
}



//Do stuff at main loop priority after SPI transaction is complete
void Downstream_SPIProcess(void)
{
    SpiPacketReceivedCallbackTypeDef tempPacketCallback;

    if (SpiInterruptCompleted == 0)
    {
        return;
    }
    SpiInterruptCompleted = 0;

    if (DownstreamInterfaceState >= DOWNSTREAM_INTERFACE_ERROR)
    {
        return;
    }

    //Finished transmitting packet size
    if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_TX_SIZE_WAIT)
    {
        if ((uint16_t)TemporaryIncomingPacketLength != 0)
        {
            //Currently we just freak out if Upstream sends us an unexpected command.
            //Theoretically we could reset our downstream state machine and accept the new command...
            DOWNSTREAM_SPI_FREAKOUT;
            return;
        }

        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_PACKET_WAIT;
        if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
                                        &CurrentWorkingPacket->CommandClass,
                                        &CurrentWorkingPacket->CommandClass,
                                        ((CurrentWorkingPacket->Length16 < 2) ? 2 : CurrentWorkingPacket->Length16)) != HAL_OK)
        {
            DOWNSTREAM_SPI_FREAKOUT;
            return;
        }
        UPSTREAM_TX_REQUEST_ASSERT;
        return;
    }

    //Finished transmitting packet body
    if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_TX_PACKET_WAIT)
    {
        Downstream_ReleasePacket(CurrentWorkingPacket);
        if (NextTxPacket != NULL)
        {
            //NextTxPacket has already passed the checks in Downstream_TransmitPacket.
            //So we just need to pass it to HAL_SPI_Transmit_DMA.
            DownstreamInterfaceState = DOWNSTREAM_INTERFACE_TX_SIZE_WAIT;
            CurrentWorkingPacket = NextTxPacket;
            NextTxPacket = NULL;
            if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
                                            (uint8_t*)&CurrentWorkingPacket->Length16,
                                            (uint8_t*)&TemporaryIncomingPacketLength,
                                            2) != HAL_OK)       //We only need to write one word, but the peripheral library freaks out...
            {
                DOWNSTREAM_SPI_FREAKOUT;
                return;
            }
            UPSTREAM_TX_REQUEST_ASSERT;
            return;
        }

        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
        if (ReceivePacketCallback != NULL)
        {
            Downstream_CheckPreparePacketReception();
        }
        return;
    }


    if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_SIZE_WAIT)
    {
        if ((CurrentWorkingPacket->Length16 < DOWNSTREAM_PACKET_LEN_MIN_16) ||
            (CurrentWorkingPacket->Length16 > DOWNSTREAM_PACKET_LEN_16))
        {
            DOWNSTREAM_SPI_FREAKOUT;
            return;
        }
        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_RX_PACKET_WAIT;
        if (HAL_SPI_TransmitReceive_DMA(&Hspi1,
                                        &CurrentWorkingPacket->CommandClass,
                                        &CurrentWorkingPacket->CommandClass,
                                        ((CurrentWorkingPacket->Length16 < 2) ? 2 : CurrentWorkingPacket->Length16)) != HAL_OK)
        {
            DOWNSTREAM_SPI_FREAKOUT;
            return;
        }
        UPSTREAM_TX_REQUEST_ASSERT;
        return;
    }

    if (DownstreamInterfaceState == DOWNSTREAM_INTERFACE_RX_PACKET_WAIT)
    {
        DownstreamInterfaceState = DOWNSTREAM_INTERFACE_IDLE;
        if (ReceivePacketCallback == NULL)
        {
            DOWNSTREAM_SPI_FREAKOUT;
            return;
        }
        //Packet processor may want to receive another packet immediately,
        //so clear ReceivePacketCallback before the call.
        //It is the callback's responsibility to release the packet buffer we are passing to it!
        tempPacketCallback = ReceivePacketCallback;
        ReceivePacketCallback = NULL;
        tempPacketCallback(CurrentWorkingPacket);
        return;
    }


    //case default:
    DOWNSTREAM_SPI_FREAKOUT;
}



//Called at the end of the SPI TxRx DMA transfer,
//at DMA2 interrupt priority. Assume *hspi points to our hspi1.
//We use TxRx to send our reply packet to check if Upstream was trying
//to send us a packet at the same time.
//We also TxRx our packet body because the SPI silicon is buggy at the end of
//a transmit-only DMA transfer with CRC! (it does not clear RXNE flag on request)
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    UPSTREAM_TX_REQUEST_DEASSERT;
    SpiInterruptCompleted = 1;
}



//Something bad happened! Possibly CRC error...
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    DOWNSTREAM_SPI_FREAKOUT;
}

