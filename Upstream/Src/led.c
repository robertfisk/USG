/*
 * led.c
 *
 *  Created on: 19/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#include "led.h"
#include "board_config.h"

uint32_t            FaultLedCounter;

LedStatusTypeDef    FaultLedState;
uint16_t            FaultLedOnMs;
uint16_t            FaultLedOffMs;
uint8_t             FaultLedBlinkCount;

uint8_t             FaultLedBlinkCountState;
uint8_t             FaultLedOutputState;


void LED_Init(void)
{
    FAULT_LED_ON;
    FaultLedState = LED_STATUS_STARTUP;
}


void LED_SetState(LedStatusTypeDef newState)
{
    FaultLedState = newState;

    switch (FaultLedState)
    {
    case LED_STATUS_OFF:
        FaultLedCounter = UINT32_MAX;
        FaultLedBlinkCountState = 0;
        FaultLedOutputState = 0;
        FAULT_LED_OFF;
        break;

    case LED_STATUS_FLASH_UNSUPPORTED:
        FaultLedOnMs = LED_UNSUPPORTED_BLINK_MS;
        FaultLedOffMs = LED_UNSUPPORTED_BLINK_MS;
        FaultLedBlinkCount = 1;
        break;

    case LED_STATUS_FLASH_BOTDETECT:
        FaultLedOnMs = LED_BOTDETECT_BLINK_MS;
        FaultLedOffMs = LED_BOTDETECT_OFF_MS;
        FaultLedBlinkCount = 2;
        break;

    default:
        FaultLedOnMs = LED_ERROR_BLINK_MS;           //Everything else is LED_STATUS_ERROR
        FaultLedOffMs = LED_ERROR_BLINK_MS;
        FaultLedBlinkCount = 1;
    }
}


void LED_Tick(void)
{
    if (FaultLedState == LED_STATUS_OFF) return;

    if (FaultLedState == LED_STATUS_STARTUP)
    {
        if (HAL_GetTick() >= STARTUP_FLASH_DELAY_MS)
        {
            LED_SetState(LED_STATUS_OFF);
        }
        return;
    }

    if (FaultLedOutputState)
    {
        if (FaultLedCounter++ >= FaultLedOnMs)                  //Check to turn LED off
        {
            FaultLedBlinkCountState++;
            FaultLedCounter = 0;
            FaultLedOutputState = 0;
            FAULT_LED_OFF;
        }
    }
    else
    {
        if (FaultLedBlinkCountState >= FaultLedBlinkCount)      //Checks to turn LED on...
        {
            if (FaultLedCounter++ >= FaultLedOffMs)             //Last flash may have longer off-time
            {
                FaultLedBlinkCountState = 0;
                FaultLedCounter = 0;
                FaultLedOutputState = 1;
                FAULT_LED_ON;
            }
        }
        else
        {
            if (FaultLedCounter++ >= FaultLedOnMs)              //Flash sequence uses on-time as intermediate off-time
            {
                FaultLedCounter = 0;
                FaultLedOutputState = 1;
                FAULT_LED_ON;
            }
        }
    }
}

