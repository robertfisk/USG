/*
 * upstream_hid_botdetect.h
 *
 *  Created on: Aug 17, 2017
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_HID_BOTDETECT_H_
#define INC_UPSTREAM_HID_BOTDETECT_H_


#include "stm32f4xx_hal.h"


#define KEY_ROLLOVER            0x01        //Rollover means we hold the last reported key status
#define KEY_A                   0x04
#define KEY_Z                   0x1D
#define KEY_1                   0x1E
#define KEY_0                   0x27
#define KEY_SPACE               0x2C
#define KEY_COMMA               0x36
#define KEY_FULLSTOP            0x37
#define KEY_PAD_1               0x59
#define KEY_PAD_0               0x62
#define KEY_MODIFIER_BASE       0xE0        //First modifier key is L-Ctl
#define KEY_MODIFIER_SHIFT_L    0xE1
#define KEY_MODIFIER_SHIFT_R    0xE5


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
    uint32_t KeyDownTime;
    uint32_t KeyActiveTimer;
}
KeyTimerLogTypeDef;


void Upstream_HID_BotDetect_Systick(void);
void Upstream_HID_BotDetectKeyboard(uint8_t* keyboardInData);



#endif /* INC_UPSTREAM_HID_BOTDETECT_H_ */
