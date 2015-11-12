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


void LED_Init(void)
{
    //RUN_LED_ON;
    FAULT_LED_ON;
    HAL_Delay(STARTUP_FLASH_DELAY);
    //RUN_LED_OFF;
    FAULT_LED_OFF;
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

