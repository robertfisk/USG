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



uint16_t    FaultLedBlinkRate       = 0;
uint16_t    FaultLedBlinkCounter    = 0;
uint8_t     FaultLedState           = 0;
uint8_t     StartupBlinkActive;


void LED_Init(void)
{
    FAULT_LED_ON;
    StartupBlinkActive = 1;
}


void LED_Fault_SetBlinkRate(uint16_t newBlinkRate)
{
    FaultLedBlinkRate = newBlinkRate;
    if (newBlinkRate == 0)
    {
        FaultLedState = 0;
        FAULT_LED_OFF;
    }
}


void LED_DoBlinks(void)
{
    if (StartupBlinkActive)
    {
        if (HAL_GetTick() >= STARTUP_FLASH_DELAY)
        {
            FAULT_LED_OFF;
            StartupBlinkActive = 0;
        }
    }

    if (FaultLedBlinkRate > 0)
    {
        FaultLedBlinkCounter++;
        if (FaultLedBlinkCounter >= FaultLedBlinkRate)
        {
            FaultLedBlinkCounter = 0;
            if (FaultLedState)
            {
                FaultLedState = 0;
                FAULT_LED_OFF;
            }
            else
            {
                FaultLedState = 1;
                FAULT_LED_ON;
            }
        }
    }
}

