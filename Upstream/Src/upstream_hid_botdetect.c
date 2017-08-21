/*
 * upstream_hid_botdetect.c
 *
 *  Created on: Aug 17, 2017
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
 
 
#include "upstream_hid_botdetect.h"
#include "upstream_hid.h"
#include "build_config.h"



//Variables common between keyboard and mouse bot detection:
uint32_t                        TemporaryLockoutStartTime;
LockoutStateTypeDef             PermanentLockoutState   = LOCKOUT_STATE_INACTIVE;
volatile LockoutStateTypeDef    TemporaryLockoutState   = LOCKOUT_STATE_INACTIVE;



//Variables specific to keyboard bot detection:
#if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)
    KeyTimerLogTypeDef  KeyTimerLog[KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_KEY_COUNT];
    volatile uint8_t    KeyTimerCount           = 0;
    uint8_t             LastKeyCode             = 0;
    uint8_t             ShortKeypressCount      = 0;
    uint8_t             OldKeyboardInData[HID_KEYBOARD_INPUT_DATA_LEN] = {0};
#endif



//Variables specific to mouse bot detection:
#if defined (CONFIG_MOUSE_ENABLED) && defined (CONFIG_MOUSE_BOT_DETECT_ENABLED)

#endif



//Code specific to keyboard bot detection:
#if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)

static uint32_t    Upstream_HID_BotDetectKeyboard_RolloverCheck(uint8_t* keyboardInData);
static uint32_t    Upstream_HID_BotDetectKeyboard_KeyIsAlphanum(uint8_t keyCode);
static void        Upstream_HID_BotDetectKeyboard_KeyDown(uint8_t keyCode);
static void        Upstream_HID_BotDetectKeyboard_KeyUp(uint8_t keyCode);


//Checks if received keyboard data is from a real human.
//This is not entirely bulletproof as an attacking device may slow its keypresses below the threshold.
void Upstream_HID_BotDetectKeyboard(uint8_t* keyboardInData)
{
    uint32_t i;
    uint32_t j;
    uint8_t  tempByte;

    if (Upstream_HID_BotDetectKeyboard_RolloverCheck(keyboardInData)) return;

    //Process modifier keys in first byte
    tempByte = keyboardInData[0];
    for (i = 0; i < 8; i++)
    {
        if ((tempByte & 1) && !(OldKeyboardInData[0] & 1))
        {
            Upstream_HID_BotDetectKeyboard_KeyDown(KEY_MODIFIER_BASE + i);
        }

        if (!(tempByte & 1) && (OldKeyboardInData[0] & 1))
        {
            Upstream_HID_BotDetectKeyboard_KeyDown(KEY_MODIFIER_BASE + i);
        }

        tempByte >>= 1;
        OldKeyboardInData[0] >>= 1;
    }


    //Process key array: search for keydowns
    for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
    {
        if (keyboardInData[i] >= KEY_A)
        {
            for (j = 2; j < HID_KEYBOARD_INPUT_DATA_LEN; j++)
            {
                if (keyboardInData[i] == OldKeyboardInData[j]) break;
            }
            if (j >= HID_KEYBOARD_INPUT_DATA_LEN)
            {
                Upstream_HID_BotDetectKeyboard_KeyDown(keyboardInData[i]);
            }
        }
    }

    //Process key array: search for keyups
     for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
     {
         if (OldKeyboardInData[i] >= KEY_A)
         {
             for (j = 2; j < HID_KEYBOARD_INPUT_DATA_LEN; j++)
             {
                 if (keyboardInData[i] == OldKeyboardInData[j]) break;
             }
             if (j >= HID_KEYBOARD_INPUT_DATA_LEN)
             {
                 Upstream_HID_BotDetectKeyboard_KeyUp(OldKeyboardInData[i]);
             }
         }
     }

    //Copy new data to old array
    for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
    {
        OldKeyboardInData[i] = keyboardInData[i];
    }

    //Final checks
    if (KeyTimerCount >= KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_KEY_COUNT)
    {
        TemporaryLockoutStartTime = HAL_GetTick();
        TemporaryLockoutState = LOCKOUT_STATE_ACTIVE;
        ////////////Set LED state
    }

    //Host receives no data if we are locked
    if ((TemporaryLockoutState == LOCKOUT_STATE_ACTIVE) || (PermanentLockoutState == LOCKOUT_STATE_ACTIVE))
    {
        for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
        {
            keyboardInData[i] = 0;
        }
    }
}



//Keyboard reports a rollover code when there are too many keys to scan/report.
static uint32_t Upstream_HID_BotDetectKeyboard_RolloverCheck(uint8_t* keyboardInData)
{
    uint32_t i;

    for (i = 2; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
    {
        if (keyboardInData[i] == KEY_ROLLOVER) break;
    }
    if (i >= HID_KEYBOARD_INPUT_DATA_LEN) return 0;

    //As I am unclear on the exact usage and interpretation of the rollover code,
    //we are going to play it safe by copying the old keyboard data over the new array.
    //This ensures the host interprets a rollover event exactly the way we do!

    //Host receives no data if we are locked
    if ((TemporaryLockoutState == LOCKOUT_STATE_ACTIVE) || (PermanentLockoutState == LOCKOUT_STATE_ACTIVE))
    {
        for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
        {
            keyboardInData[i] = 0;
        }
    }
    else
    {
        for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
        {
            keyboardInData[i] = OldKeyboardInData[i];
        }
    }
    return 1;
}



static void Upstream_HID_BotDetectKeyboard_KeyDown(uint8_t keyCode)
{
    uint32_t tempKeyActiveTime;

    if (KeyTimerCount >= KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_KEY_COUNT)
    {
        PermanentLockoutState = LOCKOUT_STATE_ACTIVE;
        ////////////Set LED state
        return;
    }

    if (Upstream_HID_BotDetectKeyboard_KeyIsAlphanum(keyCode))
    {
        tempKeyActiveTime = KEYBOARD_BOTDETECT_ALPHANUM_TIME_MS;
    }
    else
    {
        if (keyCode == LastKeyCode)
        {
            tempKeyActiveTime = KEYBOARD_BOTDETECT_NON_ALPHANUM_REPEATED_TIME_MS;
        }
        else
        {
            tempKeyActiveTime = KEYBOARD_BOTDETECT_NON_ALPHANUM_DIFFERENT_TIME_MS;
        }
    }
    LastKeyCode = keyCode;

    //Disable systick interrupt while adding key to array
    __disable_irq();
    KeyTimerLog[KeyTimerCount].KeyCode = keyCode;
    KeyTimerLog[KeyTimerCount].KeyDownTime = HAL_GetTick();
    KeyTimerLog[KeyTimerCount++].KeyActiveTimer = tempKeyActiveTime;
    __enable_irq();
}



static uint32_t Upstream_HID_BotDetectKeyboard_KeyIsAlphanum(uint8_t keyCode)
{
    if ((keyCode >= KEY_A) && (keyCode <= KEY_Z)) return 1;
    if ((keyCode >= KEY_1) && (keyCode <= KEY_0)) return 1;
    if ((keyCode >= KEY_PAD_1) && (keyCode <= KEY_PAD_0)) return 1;
    if (keyCode == KEY_SPACE) return 1;

    //else:
    return 0;
}



static void Upstream_HID_BotDetectKeyboard_KeyUp(uint8_t keyCode)
{
    uint32_t i;

    //Search through active key array to see if this key was pressed recently
    __disable_irq();
    for (i = 0; i < KeyTimerCount; i++)
    {
        if (KeyTimerLog[i].KeyCode == keyCode) break;
    }
    if (i >= KeyTimerCount)
    {
        __enable_irq();
        return;
    }

    //Was this is a short keypress?
    if ((int32_t)(HAL_GetTick() - KeyTimerLog[i].KeyDownTime) < KEYBOARD_BOTDETECT_MINIMUM_KEYDOWN_TIME_MS)
    {
        ShortKeypressCount++;
        ////////////Set LED state
        if (ShortKeypressCount >= KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_SHORT_COUNT)
        {
            PermanentLockoutState = LOCKOUT_STATE_ACTIVE;
        }
        else
        {
            TemporaryLockoutStartTime = HAL_GetTick();
            TemporaryLockoutState = LOCKOUT_STATE_ACTIVE;
        }
    }
    __enable_irq();
}

#endif  //if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)





//Called by Systick_Handler every 1ms, at high interrupt priority.
void Upstream_HID_BotDetect_Systick(void)
{
    uint32_t i;

//Keyboard-specific stuff:
#if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)
    if (KeyTimerCount > 0)                              //Check if current active key has timed out
    {
        KeyTimerLog[0].KeyActiveTimer--;
        if (KeyTimerLog[0].KeyActiveTimer <= 0)
        {
            KeyTimerCount--;
            for (i = 0; i < KeyTimerCount; i++)         //Shuffle other keys down
            {
                KeyTimerLog[i].KeyCode = KeyTimerLog[i + 1].KeyCode;
                KeyTimerLog[i].KeyDownTime = KeyTimerLog[i + 1].KeyDownTime;
                KeyTimerLog[i].KeyActiveTimer = KeyTimerLog[i + 1].KeyActiveTimer;
            }
        }
    }

    //Check if temporary lockout has expired
    if (TemporaryLockoutState == LOCKOUT_STATE_ACTIVE)
    {
        if ((int32_t)(HAL_GetTick() - TemporaryLockoutStartTime) > KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS)
        {
            TemporaryLockoutState = LOCKOUT_STATE_FLASHING;
        }
    }
    else if (TemporaryLockoutState == LOCKOUT_STATE_FLASHING)
    {
        if ((int32_t)(HAL_GetTick() - TemporaryLockoutStartTime) > KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_LED_TIME_MS)
        {
            ////////////Set LEDs off
            TemporaryLockoutState = LOCKOUT_STATE_INACTIVE;
        }
    }
#endif


#if defined (CONFIG_MOUSE_ENABLED) && defined (CONFIG_MOUSE_BOT_DETECT_ENABLED)

#endif
}



