/*
 * board_config.h
 *
 *  Created on: 25/03/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_BOARD_CONFIG_H_
#define INC_BOARD_CONFIG_H_


#define BSRR_SHIFT_HIGH			0
#define BSRR_SHIFT_LOW			16

#define PA_JTMS					GPIO_PIN_13
#define PA_JTCK					GPIO_PIN_14
#define PA_JTDI					GPIO_PIN_15
#define PB_JTDO					GPIO_PIN_3
#define PB_NJTRST				GPIO_PIN_4

#define USB_P_PIN				GPIO_PIN_4
#define USB_P_PORT				GPIOC

#define STAT_LED_PIN			GPIO_PIN_12
#define STAT_LED_PORT			GPIOC
#define STAT_LED_ON				(STAT_LED_PORT->BSRR = (STAT_LED_PIN << BSRR_SHIFT_LOW))	//Stat LED is active-low
#define STAT_LED_OFF			(STAT_LED_PORT->BSRR = (STAT_LED_PIN << BSRR_SHIFT_HIGH))

//#define RUN_LED_ON......
#define FAULT_LED_ON			STAT_LED_ON
#define FAULT_LED_OFF			STAT_LED_OFF

#define INT_ACTIVE_PIN			GPIO_PIN_5		/////////Temporary indicator of SPI activity
#define INT_ACTIVE_PORT			GPIOB
#define INT_ACTIVE_ON			INT_ACTIVE_PORT->BSRR = (INT_ACTIVE_PIN << BSRR_SHIFT_HIGH)
#define INT_ACTIVE_OFF			INT_ACTIVE_PORT->BSRR = (INT_ACTIVE_PIN << BSRR_SHIFT_LOW)

#define SPI1_NSS_PIN			GPIO_PIN_4
#define SPI1_NSS_PORT			GPIOA
#define SPI1_NSS_ASSERT			SPI1_NSS_PORT->BSRR = (SPI1_NSS_PIN << BSRR_SHIFT_LOW)
#define SPI1_NSS_DEASSERT		SPI1_NSS_PORT->BSRR = (SPI1_NSS_PIN << BSRR_SHIFT_HIGH)

#define DOWNSTREAM_TX_OK_PIN	GPIO_PIN_3
#define DOWNSTREAM_TX_OK_PORT	GPIOA
#define DOWNSTREAM_TX_OK_ACTIVE	(!(DOWNSTREAM_TX_OK_PORT->IDR & DOWNSTREAM_TX_OK_PIN))


#endif /* INC_BOARD_CONFIG_H_ */
