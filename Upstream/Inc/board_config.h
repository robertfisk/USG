/*
 * board_config.h
 *
 *  Created on: 25/03/2015
 *      Author: Robert Fisk
 */

#ifndef INC_BOARD_CONFIG_H_
#define INC_BOARD_CONFIG_H_



#define PA_JTMS					GPIO_PIN_13
#define PA_JTCK					GPIO_PIN_14
#define PA_JTDI					GPIO_PIN_15
#define PB_JTDO					GPIO_PIN_3
#define PB_NJTRST				GPIO_PIN_4

#define USB_P_PIN				GPIO_PIN_4
#define USB_P_PORT				GPIOC

#define STAT_LED_PIN			GPIO_PIN_12
#define STAT_LED_PORT			GPIOC
#define STAT_LED_BSRR_OFF		(STAT_LED_PIN)
#define STAT_LED_BSRR_ON		(STAT_LED_PIN << 16)




#endif /* INC_BOARD_CONFIG_H_ */
