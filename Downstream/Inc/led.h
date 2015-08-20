/*
 * led.h
 *
 *  Created on: 19/08/2015
 *      Author: Robert Fisk
 */

#ifndef INC_LED_H_
#define INC_LED_H_


void LED_Init(void);
void LED_Fault_SetBlinkRate(uint16_t newBlinkRate);
void LED_DoBlinks(void);



#define STARTUP_FLASH_DELAY		500		//units = ticks = ms

//LEDs are on for BLINK_RATE ticks, then off for BLINK_RATE ticks
#define LED_FAST_BLINK_RATE		100
#define LED_SLOW_BLINK_RATE		500



#endif /* INC_LED_H_ */
