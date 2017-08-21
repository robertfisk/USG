/*
 * build_config.h
 *
 *  Created on: Jun 20, 2017
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_BUILD_CONFIG_H_
#define INC_BUILD_CONFIG_H_


#define CONFIG_MASS_STORAGE_ENABLED
#define CONFIG_MASS_STORAGE_WRITES_PERMITTED

#define CONFIG_KEYBOARD_ENABLED
#define CONFIG_KEYBOARD_BOT_DETECT_ENABLED

#define CONFIG_MOUSE_ENABLED
//#define CONFIG_MOUSE_BOT_DETECT_ENABLED


//Configure keyboard rate-limiting (bot detection) here:
#ifdef CONFIG_KEYBOARD_BOT_DETECT_ENABLED
    #define KEYBOARD_BOTDETECT_ALPHANUM_TIME_MS                 170     //Alphanumeric keys (and space), used for high-speed typing. Limit 6 per second, 360 keys per minute
    #define KEYBOARD_BOTDETECT_NON_ALPHANUM_DIFFERENT_TIME_MS   250     //Non-alphanumeric keys. Limit 4 per second.
    #define KEYBOARD_BOTDETECT_NON_ALPHANUM_REPEATED_TIME_MS    170     //Non-alphanumeric key, pressed repeatedly. Limit 6 keys per second.
    #define KEYBOARD_BOTDETECT_MINIMUM_KEYDOWN_TIME_MS          100     //Key pressed for less than 100ms indicates bot.

    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_KEY_COUNT      2       //'Burst' = maximum number of keys with active timer before triggering temporary lockout
    #define KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_KEY_COUNT      4       //'Burst' = maximum number of keys with active timer before triggering permanent lockout
    #define KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_SHORT_COUNT    3       //One short keypress causes temporary lockout. 3 short keypresses within the temporary lockout time triggers permanent lockout

    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS        5000    //Lockout for 5 seconds when temporary lockout threshold reached
    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_LED_TIME_MS    60000   //Flash fault LED for 60 seconds after temporary lockout
#endif


//Configure mouse rate-limiting (bot detection) here:
#ifdef CONFIG_MOUSE_BOT_DETECT_ENABLED

#endif


#endif /* INC_BUILD_CONFIG_H_ */
