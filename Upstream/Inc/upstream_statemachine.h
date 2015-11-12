/*
 * upstream_statemachine.h
 *
 *  Created on: 20/08/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_UPSTREAM_STATEMACHINE_H_
#define INC_UPSTREAM_STATEMACHINE_H_


#include "led.h"


typedef enum
{
    STATE_TEST_INTERFACE,
    STATE_WAIT_DEVICE,
    STATE_DEVICE_ACTIVE,
    STATE_ERROR
} UpstreamStateTypeDef;



#define UPSTREAM_STATEMACHINE_FREAKOUT                  \
    do {                                                \
        LED_Fault_SetBlinkRate(LED_FAST_BLINK_RATE);    \
        Upstream_StateMachine_SetErrorState();          \
        while (1);                                      \
} while (0);



void Upstream_InitStateMachine(void);
void Upstream_StateMachine_SetErrorState(void);
HAL_StatusTypeDef Upstream_StateMachine_CheckClassOperationOk(void);
void Upstream_StateMachine_DeviceDisconnected(void);



#endif /* INC_UPSTREAM_STATEMACHINE_H_ */
