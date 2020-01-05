/*
 * led.h
 *
 *  Created on: 19/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_LED_H_
#define INC_LED_H_


#include "stm32f4xx_hal.h"


typedef enum
{
    LED_STATUS_STARTUP,
    LED_STATUS_OFF,
    LED_STATUS_FLASH_ERROR,
    LED_STATUS_FLASH_UNSUPPORTED,
    LED_STATUS_FLASH_BOTDETECT,
    LED_STATUS_FLASH_READWRITE
}
LedStatusTypeDef;


void LED_Init(void);
void LED_SetState(LedStatusTypeDef newState);
void LED_Tick(void);


#define STARTUP_FLASH_DELAY_MS      500     //units = ticks = ms

#define LED_ERROR_BLINK_MS          100
#define LED_UNSUPPORTED_BLINK_MS    500
#define LED_BOTDETECT_ON_MS         100
#define LED_BOTDETECT_OFF_MS        (1000 - (LED_BOTDETECT_ON_MS * 3))       //Two flashes, total period = 1 sec
#define LED_READWRITE_ON_MS         10
#define LED_READWRITE_OFF_MS        30


#endif /* INC_LED_H_ */
