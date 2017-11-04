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
    #define KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS                10      //10ms per bin
    #define KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS                20      //20ms per bin
    #define KEYBOARD_BOTDETECT_FAST_BIN_COUNT                   25      //25 bins at 10ms = 250ms fast coverage
    #define KEYBOARD_BOTDETECT_SLOW_BIN_COUNT                   50      //50 bins at 20ms = 1 sec slow coverage, wrapped

    #define KEYBOARD_BOTDETECT_FAST_BIN_DRAIN_DIVIDER           2
    #define KEYBOARD_BOTDETECT_SLOW_BIN_DRAIN_DIVIDER           4
    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_BIN_THRESHOLD  4

    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS        4000
    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_FLASH_TIME_MS  60000   //Flash fault LED for 60 seconds after temporary lockout
#endif


//Configure mouse rate-limiting (bot detection) here:
#ifdef CONFIG_MOUSE_BOT_DETECT_ENABLED

#endif


#endif /* INC_BUILD_CONFIG_H_ */
