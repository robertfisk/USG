/*
 * upstream_interface_def.h
 *
 *  Created on: 22/06/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_UPSTREAM_INTERFACE_DEF_H_
#define INC_UPSTREAM_INTERFACE_DEF_H_


//***************
// Attention!
// Keep this file synchronised with downstream_interface_def.h
// in the Downstream project.
//***************


//Upstream only supports one LUN.
//Downstream may support > 1 LUN and only report the first active one to upstream.
#define UPSTREAM_LUN_NBR			1
#define COMMAND_CLASS_DATA_FLAG		0x80
#define COMMAND_CLASS_MASK			((uint8_t)(~COMMAND_CLASS_DATA_FLAG))


typedef enum
{
	COMMAND_CLASS_INTERFACE,
	COMMAND_CLASS_MASS_STORAGE,
	//...
	COMMAND_CLASS_ERROR
}
InterfaceCommandClassTypeDef;


typedef enum
{
	COMMAND_INTERFACE_ECHO,				//Returns echo packet including all data
	COMMAND_INTERFACE_NOTIFY_DEVICE		//Returns COMMAND_CLASS_*** byte when downstream USB device is connected
}
InterfaceCommandInterfaceTypeDef;


typedef enum
{
	COMMAND_MSC_TEST_UNIT_READY,	//Returns HAL_StatusTypeDef result
	COMMAND_MSC_GET_CAPACITY,		//Returns uint32_t blk_nbr, uint32_t blk_size
	COMMAND_MSC_READ,				//Returns HAL_StatusTypeDef result, then data stream
	COMMAND_MSC_WRITE,				//Returns HAL_OK, HAL_ERROR if medium not present, HAL_BUSY if write-protected result, then waits for data stream
}
InterfaceCommandMscTypeDef;


typedef enum
{
	COMMAND_ERROR_GENERIC,
	COMMAND_ERROR_DEVICE_DISCONNECTED,
}
InterfaceCommandErrorTypeDef;



#endif /* INC_UPSTREAM_INTERFACE_DEF_H_ */
