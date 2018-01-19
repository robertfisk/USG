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
#include "usbd_hid.h"
#include "math.h"



//Variables common between keyboard and mouse bot detection:
uint32_t                        TemporaryLockoutTimeMs;
volatile LockoutStateTypeDef    LockoutState = LOCKOUT_STATE_INACTIVE;



//Variables specific to keyboard bot detection:
#if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)
    uint32_t            LastKeyDownTime = 0;
    KeyTimerLogTypeDef  KeyTimerLog[KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS] = {0};
    uint8_t             OldKeyboardInData[HID_KEYBOARD_INPUT_DATA_LEN]  = {0};

    uint8_t             KeyDelayFastBinDrainDivideCount     = 0;
    uint8_t             KeyDelaySlowBinDrainDivideCount     = 0;
    uint8_t             KeyDowntimeFastBinDrainDivideCount  = 0;
    uint8_t             KeyDowntimeSlowBinDrainDivideCount  = 0;
    uint8_t             KeyDelayFastBinArray[KEYBOARD_BOTDETECT_FAST_BIN_COUNT]     = {0};
    uint8_t             KeyDelaySlowBinArray[KEYBOARD_BOTDETECT_SLOW_BIN_COUNT]     = {0};
    uint8_t             KeyDowntimeFastBinArray[KEYBOARD_BOTDETECT_FAST_BIN_COUNT]  = {0};
    uint8_t             KeyDowntimeSlowBinArray[KEYBOARD_BOTDETECT_SLOW_BIN_COUNT]  = {0};

    //Debug:
//    uint8_t             KeyDelayFastBinArrayPeak;
//    uint8_t             KeyDelaySlowBinArrayPeak;
//    uint8_t             KeyDowntimeFastBinArrayPeak;
//    uint8_t             KeyDowntimeSlowBinArrayPeak;

    static uint32_t     Upstream_HID_BotDetectKeyboard_RolloverCheck(uint8_t* keyboardInData);
    static void         Upstream_HID_BotDetectKeyboard_DoLockout(void);
    static void         Upstream_HID_BotDetectKeyboard_KeyDown(uint8_t keyCode);
    static void         Upstream_HID_BotDetectKeyboard_KeyUp(uint8_t keyCode);
#endif



//Variables specific to mouse bot detection:
#if defined (CONFIG_MOUSE_ENABLED) && defined (CONFIG_MOUSE_BOT_DETECT_ENABLED)
    uint32_t    LastMouseMoveTime               = 0;

    //Jump detection stuff
    uint32_t    FirstMouseMoveTime              = 0;
    uint8_t     JumpMouseIsMoving               = 0;

    //Constant acceleration detection stuff
    uint16_t    MouseVelocityHistory[MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE] = {0};
    int32_t     PreviousSmoothedAcceleration    = 0;
    int32_t     ConstantAccelerationCounter     = 0;

    //Jiggle detection stuff
    uint8_t     MouseStopIntervalBinDrainDivideCount    = 0;
    uint8_t     MouseStopIntervalBinArray[MOUSE_BOTDETECT_JIGGLE_BIN_COUNT] = {0};

    //Debug:
//    int8_t     ConstantAccelerationCounterMax   = 0;
//    int8_t      ConstantAccelerationCounterMin  = 0;
//    uint8_t     MouseStopIntervalBinArrayPeak   = 0;

    static void Upstream_HID_BotDetectMouse_DoLockout(void);
#endif



//Code specific to keyboard bot detection:
#if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)

//Checks if received keyboard data is from a real human.
//This is not entirely bulletproof as an attacking device may randomize its keypresses.
void Upstream_HID_BotDetectKeyboard(uint8_t* keyboardInData)
{
    uint32_t i;
    uint32_t j;
    uint8_t  tempModifier;

    if (Upstream_HID_BotDetectKeyboard_RolloverCheck(keyboardInData)) return;

    //Process modifier keys in first byte
    tempModifier = keyboardInData[0];
    for (i = 0; i < 8; i++)
    {
        if ((tempModifier & 1) && !(OldKeyboardInData[0] & 1))
        {
            Upstream_HID_BotDetectKeyboard_KeyDown(KEY_MODIFIER_BASE + i);
        }
        if (!(tempModifier & 1) && (OldKeyboardInData[0] & 1))
        {
            Upstream_HID_BotDetectKeyboard_KeyUp(KEY_MODIFIER_BASE + i);
        }
        tempModifier >>= 1;
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
                 if (OldKeyboardInData[i] == keyboardInData[j]) break;
             }
             if (j >= HID_KEYBOARD_INPUT_DATA_LEN)
             {
                 Upstream_HID_BotDetectKeyboard_KeyUp(OldKeyboardInData[i]);
             }
         }
     }

    //Check for evidence of bot typing
    for (i = 0; i < KEYBOARD_BOTDETECT_FAST_BIN_COUNT; i++)
    {
        if ((KeyDelayFastBinArray[i]    > KEYBOARD_BOTDETECT_LOCKOUT_BIN_THRESHOLD) ||
            (KeyDowntimeFastBinArray[i] > KEYBOARD_BOTDETECT_LOCKOUT_BIN_THRESHOLD))
        {
            Upstream_HID_BotDetectKeyboard_DoLockout();
            break;
        }
        //Debug:
//        if (KeyDelayFastBinArray[i]    > KeyDelayFastBinArrayPeak)    KeyDelayFastBinArrayPeak = KeyDelayFastBinArray[i];
//        if (KeyDowntimeFastBinArray[i] > KeyDowntimeFastBinArrayPeak) KeyDowntimeFastBinArrayPeak = KeyDowntimeFastBinArray[i];
    }
    for (i = 0; i < KEYBOARD_BOTDETECT_SLOW_BIN_COUNT; i++)
    {
        if ((KeyDelaySlowBinArray[i]    > KEYBOARD_BOTDETECT_LOCKOUT_BIN_THRESHOLD) ||
            (KeyDowntimeSlowBinArray[i] > KEYBOARD_BOTDETECT_LOCKOUT_BIN_THRESHOLD))
        {
            Upstream_HID_BotDetectKeyboard_DoLockout();
            break;
        }
        //Debug:
//        if (KeyDelaySlowBinArray[i]    > KeyDelaySlowBinArrayPeak)    KeyDelaySlowBinArrayPeak = KeyDelaySlowBinArray[i];
//        if (KeyDowntimeSlowBinArray[i] > KeyDowntimeSlowBinArrayPeak) KeyDowntimeSlowBinArrayPeak = KeyDowntimeSlowBinArray[i];
    }

    //Copy new data to old array
    for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
    {
        OldKeyboardInData[i] = keyboardInData[i];
    }

    //Host receives no data if we are locked
    if ((LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE) ||
        (LockoutState == LOCKOUT_STATE_PERMANENT_ACTIVE))
    {
        for (i = 0; i < HID_KEYBOARD_INPUT_DATA_LEN; i++)
        {
            keyboardInData[i] = 0;
        }
    }
}



static void Upstream_HID_BotDetectKeyboard_DoLockout(void)
{
    uint32_t i;

    if (LockoutState == LOCKOUT_STATE_PERMANENT_ACTIVE) return;

    //Are we already in warning state? -> activate permanent lockout
    if ((LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE) ||
        (LockoutState == LOCKOUT_STATE_TEMPORARY_FLASHING))
    {
        LockoutState = LOCKOUT_STATE_PERMANENT_ACTIVE;
        return;
    }

    //Otherwise, reset counters and give warning
    for (i = 0; i < KEYBOARD_BOTDETECT_FAST_BIN_COUNT; i++)
    {
        KeyDelayFastBinArray[i] = 0;
        KeyDowntimeFastBinArray[i] = 0;
    }
    for (i = 0; i < KEYBOARD_BOTDETECT_SLOW_BIN_COUNT; i++)
    {
        KeyDelaySlowBinArray[i] = 0;
        KeyDowntimeSlowBinArray[i] = 0;
    }

    TemporaryLockoutTimeMs = 0;
    LockoutState = LOCKOUT_STATE_TEMPORARY_ACTIVE;
    LED_SetState(LED_STATUS_FLASH_BOTDETECT);
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
    if ((LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE) ||
        (LockoutState == LOCKOUT_STATE_PERMANENT_ACTIVE))
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
    uint32_t i;
    uint32_t keyDelay;
    uint32_t now = HAL_GetTick();

    keyDelay = now - LastKeyDownTime;
    if (keyDelay < (KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS * KEYBOARD_BOTDETECT_FAST_BIN_COUNT))
    {
        KeyDelayFastBinArray[(keyDelay / KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS)]++;                          //Add key to fast bin

        //Drain fast bins at specified rate
        KeyDelayFastBinDrainDivideCount++;
        if (KeyDelayFastBinDrainDivideCount >= KEYBOARD_BOTDETECT_FAST_BIN_DRAIN_DIVIDER)
        {
            KeyDelayFastBinDrainDivideCount = 0;
            for (i = 0; i < KEYBOARD_BOTDETECT_FAST_BIN_COUNT; i++)
            {
                if (KeyDelayFastBinArray[i] > 0) KeyDelayFastBinArray[i]--;
            }
        }
    }
    else
    {
        keyDelay = keyDelay % (KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS * KEYBOARD_BOTDETECT_SLOW_BIN_COUNT);   //Wrap slow key time into the slow array
        KeyDelaySlowBinArray[(keyDelay / KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS)]++;                          //Add key to slow bin

        //Drain slow bins at specified rate
        KeyDelaySlowBinDrainDivideCount++;
        if (KeyDelaySlowBinDrainDivideCount >= KEYBOARD_BOTDETECT_SLOW_BIN_DRAIN_DIVIDER)
        {
            KeyDelaySlowBinDrainDivideCount = 0;
            for (i = 0; i < KEYBOARD_BOTDETECT_SLOW_BIN_COUNT; i++)
            {
                if (KeyDelaySlowBinArray[i] > 0) KeyDelaySlowBinArray[i]--;
            }
        }
    }
    LastKeyDownTime = now;

    for (i = 0; i < KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS; i++)
    {
        if (KeyTimerLog[i].KeyCode == 0) break;
    }
    if (i >= KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS) while (1);         //Totally should not happen
    KeyTimerLog[i].KeyCode = keyCode;
    KeyTimerLog[i].KeyDownStart = now;
}



static void Upstream_HID_BotDetectKeyboard_KeyUp(uint8_t keyCode)
{
    uint32_t i;
    uint32_t keyDowntime;

    for (i = 0; i < KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS; i++)
    {
        if (KeyTimerLog[i].KeyCode == keyCode) break;
    }
    if (i >= KEYBOARD_BOTDETECT_MAX_ACTIVE_KEYS) while (1);         //Totally should not happen

    KeyTimerLog[i].KeyCode = 0;                                     //Clear out the key entry
    keyDowntime = HAL_GetTick() - KeyTimerLog[i].KeyDownStart;
    if (keyDowntime < (KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS * KEYBOARD_BOTDETECT_FAST_BIN_COUNT))
    {
        KeyDowntimeFastBinArray[(keyDowntime / KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS)]++;                          //Add key to fast bin

        //Drain fast bins at specified rate
        KeyDowntimeFastBinDrainDivideCount++;
        if (KeyDowntimeFastBinDrainDivideCount >= KEYBOARD_BOTDETECT_FAST_BIN_DRAIN_DIVIDER)
        {
            KeyDowntimeFastBinDrainDivideCount = 0;
            for (i = 0; i < KEYBOARD_BOTDETECT_FAST_BIN_COUNT; i++)
            {
                if (KeyDowntimeFastBinArray[i] > 0) KeyDowntimeFastBinArray[i]--;
            }
        }
    }
    else
    {
        keyDowntime = keyDowntime % (KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS * KEYBOARD_BOTDETECT_SLOW_BIN_COUNT);   //Wrap slow key time into the slow array
        KeyDowntimeSlowBinArray[(keyDowntime / KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS)]++;                          //Add key to slow bin

        //Drain slow bins at specified rate
        KeyDowntimeSlowBinDrainDivideCount++;
        if (KeyDowntimeSlowBinDrainDivideCount >= KEYBOARD_BOTDETECT_SLOW_BIN_DRAIN_DIVIDER)
        {
            KeyDowntimeSlowBinDrainDivideCount = 0;
            for (i = 0; i < KEYBOARD_BOTDETECT_SLOW_BIN_COUNT; i++)
            {
                if (KeyDowntimeSlowBinArray[i] > 0) KeyDowntimeSlowBinArray[i]--;
            }
        }
    }
}

#endif  //if defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)





//Called by Systick_Handler every 1ms, at high interrupt priority.
void Upstream_HID_BotDetect_Systick(void)
{
#if (defined (CONFIG_KEYBOARD_ENABLED) && defined (CONFIG_KEYBOARD_BOT_DETECT_ENABLED)) || \
    (defined (CONFIG_MOUSE_ENABLED) && defined (CONFIG_MOUSE_BOT_DETECT_ENABLED))

    //Check if temporary lockout has expired
    if (LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE)
    {
        if (TemporaryLockoutTimeMs++ > BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS)
        {
            LockoutState = LOCKOUT_STATE_TEMPORARY_FLASHING;
        }
    }
    else if (LockoutState == LOCKOUT_STATE_TEMPORARY_FLASHING)
    {
        if (TemporaryLockoutTimeMs++ > BOTDETECT_TEMPORARY_LOCKOUT_FLASH_TIME_MS)
        {
            LED_SetState(LED_STATUS_OFF);
            LockoutState = LOCKOUT_STATE_INACTIVE;
        }
    }
#endif
}



//Code specific to mouse bot detection:
#if defined (CONFIG_MOUSE_ENABLED) && defined (CONFIG_MOUSE_BOT_DETECT_ENABLED)

void Upstream_HID_BotDetectMouse(uint8_t* mouseInData)
{
    uint32_t i;
    uint32_t now = HAL_GetTick();
    uint32_t moveDelay;
    uint32_t velocity;
    int8_t  mouseX;
    int8_t  mouseY;

    //Constant acceleration detection stuff
    uint32_t newSmoothedVelocity;
    uint32_t oldSmoothedVelocity;
    int32_t  newSmoothedAcceleration;
    int32_t  smoothedAccelerationMatchError;


    mouseX = mouseInData[1];
    mouseY = mouseInData[2];
    velocity = (sqrtf(((int32_t)mouseX * mouseX) +
                      ((int32_t)mouseY * mouseY))) * MOUSE_BOTDETECT_VELOCITY_MULTIPLIER;       //Multiply floating-point sqrt result to avoid integer rounding errors
    moveDelay = now - LastMouseMoveTime;


    //Reset constant acceleration detection state after a few seconds of inactivity
    if (moveDelay > MOUSE_BOTDETECT_VELOCITY_RESET_TIMEOUT_MS)
    {
        //Is this the start of a new movement?
        if (velocity != 0)
        {
            for (i = 0; i < MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE; i++)
            {
                MouseVelocityHistory[i] = 0;
            }
            ConstantAccelerationCounter = 0;
        }
    }


    //Jiggle detection: did the mouse stop moving?
    if (moveDelay > ((MOUSE_BOTDETECT_JIGGLE_STOP_PERIODS * HID_FS_BINTERVAL) - (HID_FS_BINTERVAL / 2)))
    {
        //Is this the start of a new movement?
        if (velocity != 0)
        {
            //Jiggle detection: add stopped time to jiggle bins
            moveDelay = moveDelay % (MOUSE_BOTDETECT_JIGGLE_BIN_WIDTH_MS * MOUSE_BOTDETECT_JIGGLE_BIN_COUNT);   //Wrap stopped time into the array
            MouseStopIntervalBinArray[(moveDelay / MOUSE_BOTDETECT_JIGGLE_BIN_WIDTH_MS)]++;
            if (MouseStopIntervalBinArray[(moveDelay / MOUSE_BOTDETECT_JIGGLE_BIN_WIDTH_MS)] > MOUSE_BOTDETECT_LOCKOUT_JIGGLE_BIN_THRESHOLD)
            {
                Upstream_HID_BotDetectMouse_DoLockout();
            }

            //Debug:
//            if (MouseStopIntervalBinArray[(moveDelay / MOUSE_BOTDETECT_JIGGLE_BIN_WIDTH_MS)] > MouseStopIntervalBinArrayPeak)
//            {
//                MouseStopIntervalBinArrayPeak = MouseStopIntervalBinArray[(moveDelay / MOUSE_BOTDETECT_JIGGLE_BIN_WIDTH_MS)];
//            }

            //Drain jiggle bins at specified rate
            MouseStopIntervalBinDrainDivideCount++;
            if (MouseStopIntervalBinDrainDivideCount >= MOUSE_BOTDETECT_JIGGLE_BIN_DIVIDER)
            {
                MouseStopIntervalBinDrainDivideCount = 0;
                for (i = 0; i < MOUSE_BOTDETECT_JIGGLE_BIN_COUNT; i++)
                {
                    if (MouseStopIntervalBinArray[i] > 0) MouseStopIntervalBinArray[i]--;
                }
            }
        }
    }


    //Jump detection: did the mouse stop moving briefly?
    if (moveDelay > ((MOUSE_BOTDETECT_JUMP_PERIODS * HID_FS_BINTERVAL) - (HID_FS_BINTERVAL / 2)))
    {
        FirstMouseMoveTime = 0;
        if (JumpMouseIsMoving)              //Was a significant movement in progress?
        {
            JumpMouseIsMoving = 0;
            if ((LastMouseMoveTime - FirstMouseMoveTime) < ((MOUSE_BOTDETECT_JUMP_PERIODS * HID_FS_BINTERVAL) - (HID_FS_BINTERVAL / 2)))
            {
                Upstream_HID_BotDetectMouse_DoLockout();
            }
        }
    }


    if (velocity != 0)
    {
        //Jump detection
        LastMouseMoveTime = now;
        if (FirstMouseMoveTime == 0)
        {
            FirstMouseMoveTime = now;
        }
        if (velocity > (MOUSE_BOTDETECT_JUMP_VELOCITY_THRESHOLD * MOUSE_BOTDETECT_VELOCITY_MULTIPLIER))
        {
            JumpMouseIsMoving = 1;
        }

        //Constant acceleration detection
        for (i = (MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE - 1); i > 0; i--)   //Shuffle down history data
        {
            MouseVelocityHistory[i] = MouseVelocityHistory[i - 1];
        }
        MouseVelocityHistory[0] = velocity;                                 //Store latest data at head

        if (MouseVelocityHistory[(MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE - 1)] > 0)
        {
            velocity = 0;                                                   //Calculate new and old average velocities
            for (i = 0; i < (MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE / 2); i++)
            {
                velocity  += MouseVelocityHistory[i];
            }
            newSmoothedVelocity = (velocity * 8) / (MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE / 2);     //Multiply velocity up to avoid rounding errors on divide

            velocity = 0;
            for (i = (MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE / 2); i < MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE; i++)
            {
                velocity  += MouseVelocityHistory[i];
            }
            oldSmoothedVelocity = (velocity * 8) / (MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE / 2);     //Multiply velocity up to avoid rounding errors on divide

            newSmoothedAcceleration = newSmoothedVelocity - oldSmoothedVelocity;
            smoothedAccelerationMatchError = (oldSmoothedVelocity * MOUSE_BOTDETECT_VELOCITY_MATCH_ERROR) / MOUSE_BOTDETECT_VELOCITY_MATCH_BASE;
            if (((PreviousSmoothedAcceleration + smoothedAccelerationMatchError) >= newSmoothedAcceleration) &&
                ((PreviousSmoothedAcceleration - smoothedAccelerationMatchError) <= newSmoothedAcceleration))
            {
                ConstantAccelerationCounter++;
                if (ConstantAccelerationCounter > MOUSE_BOTDETECT_CONSTANT_ACCEL_LOCKOUT)
                {
                    Upstream_HID_BotDetectMouse_DoLockout();
                }
                else if (ConstantAccelerationCounter >= MOUSE_BOTDETECT_CONSTANT_ACCEL_STOP)        //Stop mouse movement if it looks suspiciously constant
                {
                    mouseInData[1] = 0;
                    mouseInData[2] = 0;
                }
            }
            else
            {
                if (ConstantAccelerationCounter > -MOUSE_BOTDETECT_CONSTANT_ACCEL_CREDIT) ConstantAccelerationCounter--;
            }
            PreviousSmoothedAcceleration = newSmoothedAcceleration;

            //Debug:
//            if (ConstantAccelerationCounter > ConstantAccelerationCounterMax) ConstantAccelerationCounterMax = ConstantAccelerationCounter;
//            if (ConstantAccelerationCounter < ConstantAccelerationCounterMin) ConstantAccelerationCounterMin = ConstantAccelerationCounter;
        }
    }
    else
    {
        mouseInData[1] = 0;             //If we don't want to process this event, makes sure no movement data gets through
        mouseInData[2] = 0;
    }

    //Host receives no data if we are locked
    if ((LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE) ||
        (LockoutState == LOCKOUT_STATE_PERMANENT_ACTIVE))
    {
        for (i = 0; i < HID_MOUSE_INPUT_DATA_LEN; i++)
        {
            mouseInData[i] = 0;
        }
    }
}



static void Upstream_HID_BotDetectMouse_DoLockout(void)
{
    uint32_t i;

    if (LockoutState == LOCKOUT_STATE_PERMANENT_ACTIVE) return;

    //Are we already in warning state? -> activate permanent lockout
    if ((LockoutState == LOCKOUT_STATE_TEMPORARY_ACTIVE) ||
        (LockoutState == LOCKOUT_STATE_TEMPORARY_FLASHING))
    {
        LockoutState = LOCKOUT_STATE_PERMANENT_ACTIVE;
        return;
    }

    //Otherwise, reset counters and give warning
    for (i = 0; i < MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE; i++)
    {
        MouseVelocityHistory[i] = 0;
    }
    ConstantAccelerationCounter = 0;

    for (i = 0; i < MOUSE_BOTDETECT_JIGGLE_BIN_COUNT; i++)
    {
        MouseStopIntervalBinArray[i] = 0;
    }

    TemporaryLockoutTimeMs = 0;
    LockoutState = LOCKOUT_STATE_TEMPORARY_ACTIVE;
    LED_SetState(LED_STATUS_FLASH_BOTDETECT);
}

#endif
