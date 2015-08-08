/*
 * downstream_statemachine.h
 *
 *  Created on: 2/08/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_STATEMACHINE_H_
#define INC_DOWNSTREAM_STATEMACHINE_H_


#include "usbh_def.h"


typedef enum
{
	STATE_NOT_READY,
	STATE_WAIT_DEVICE_READY_CALLBACK,
	STATE_DEVICE_READY,
	STATE_ERROR
} DownstreamStateTypeDef;



void Downstream_InitStateMachine(void);
void Downstream_HostUserCallback(USBH_HandleTypeDef *phost, uint8_t id);



#endif /* INC_DOWNSTREAM_STATEMACHINE_H_ */
