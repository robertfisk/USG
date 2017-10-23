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
    #define KEYBOARD_BOTDETECT_FASTKEY_TIME_MS                  125     //Alphanumeric keys (& space & shift & comma & fullstop), used for high-speed typing. Limit 8 per second
    #define KEYBOARD_BOTDETECT_FASTKEY_TIME_REPEATED_MS         170     //Alphanumeric keys pressed repeatedly. Limit 6 keys per second.
    #define KEYBOARD_BOTDETECT_SLOWKEY_TIME_MS                  200     //Non-alphanumeric keys. Limit 5 per second.
    #define KEYBOARD_BOTDETECT_SLOWKEY_TIME_REPEATED_MS         170     //Non-alphanumeric key, pressed repeatedly. Limit 6 keys per second.
    #define KEYBOARD_BOTDETECT_MINIMUM_KEYDOWN_TIME_MS          30      //Key pressed less than this indicates bot. This value must be less than the smallest of the above ratelimit key times for the code to work correctly!

    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_KEY_COUNT      3       //'Burst' = maximum number of keys with active timer before triggering temporary lockout
    #define KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_KEY_COUNT      5       //'Burst' = maximum number of keys with active timer before triggering permanent lockout
    #define KEYBOARD_BOTDETECT_PERMANENT_LOCKOUT_SHORT_COUNT    3       //One short keypress causes temporary lockout. 3 short keypresses within the temporary lockout time triggers permanent lockout

    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS        5000    //Lockout for 5 seconds when temporary lockout threshold reached
    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_FLASH_TIME_MS  60000   //Flash fault LED for 60 seconds after temporary lockout
#endif


//Configure mouse rate-limiting (bot detection) here:
#ifdef CONFIG_MOUSE_BOT_DETECT_ENABLED

#endif


#endif /* INC_BUILD_CONFIG_H_ */
