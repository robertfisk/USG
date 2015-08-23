/*
 * downstream_interface_def.h
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_INTERFACE_DEF_H_
#define INC_DOWNSTREAM_INTERFACE_DEF_H_


//***************
// Attention!
// Keep this file synchronised with downstream_interface_def.h
// in the Upstream project.
//***************



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
	COMMAND_MSC_BEGIN_READ,			//Returns HAL_StatusTypeDef result, then data stream
	COMMAND_MSC_BEGIN_WRITE,		//Returns HAL_OK, HAL_ERROR if medium not present, HAL_BUSY if write-protected result, then waits for data stream
}
InterfaceCommandMscTypeDef;


typedef enum
{
	COMMAND_ERROR_GENERIC,
	COMMAND_ERROR_DEVICE_DISCONNECTED,
}
InterfaceCommandErrorTypeDef;



#endif /* INC_DOWNSTREAM_INTERFACE_DEF_H_ */
