/*
 * upstream_hid_botdetect.h
 *
 *  Created on: Aug 17, 2017
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_HID_BOTDETECT_H_
#define INC_UPSTREAM_HID_BOTDETECT_H_


#include "stm32f4xx_hal.h"


#define KEY_ROLLOVER            0x01                //Rollover means we hold the last reported key status
#define KEY_A                   0x04
#define KEY_MODIFIER_BASE       0xE0                //First modifier key is L-Ctl


#define KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS  14      //This is here because it is not a tuneable parameter



typedef enum
{
    LOCKOUT_STATE_INACTIVE = 0,
    LOCKOUT_STATE_TEMPORARY_ACTIVE,
    LOCKOUT_STATE_TEMPORARY_FLASHING,
    LOCKOUT_STATE_PERMANENT_ACTIVE
}
LockoutStateTypeDef;


typedef struct
{
    uint8_t  KeyCode;
    uint32_t KeyDownStart;
}
KeyTimerLogTypeDef;

typedef struct
{
    uint8_t  moveDelay;
    uint16_t velocity;
}
MouseVelocityHistoryTypeDef;


void Upstream_HID_BotDetect_Systick(void);
void Upstream_HID_BotDetectKeyboard(uint8_t* keyboardInData);
void Upstream_HID_BotDetectMouse(uint8_t* mouseInData);



#endif /* INC_UPSTREAM_HID_BOTDETECT_H_ */
