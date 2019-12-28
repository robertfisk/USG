/*
 * downstream_interface_def.h
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_DOWNSTREAM_INTERFACE_DEF_H_
#define INC_DOWNSTREAM_INTERFACE_DEF_H_


//***************
// Attention!
// Keep this file synchronised with upstream_interface_def.h
// in the Upstream project.
//***************



#define COMMAND_CLASS_DATA_FLAG     0x80
#define COMMAND_CLASS_MASK          ((uint8_t)(~COMMAND_CLASS_DATA_FLAG))


typedef enum
{
    COMMAND_CLASS_INTERFACE,
    COMMAND_CLASS_MASS_STORAGE,
    COMMAND_CLASS_HID_MOUSE,
    COMMAND_CLASS_HID_KEYBOARD,
    //...
    COMMAND_CLASS_ERROR
}
InterfaceCommandClassTypeDef;


typedef enum
{
    COMMAND_INTERFACE_ECHO,             //Returns echo packet including all data
    COMMAND_INTERFACE_NOTIFY_DEVICE     //Returns COMMAND_CLASS_*** byte when downstream USB device is connected
}
InterfaceCommandInterfaceTypeDef;


typedef enum
{
    COMMAND_MSC_TEST_UNIT_READY,        //Returns HAL_StatusTypeDef result
    COMMAND_MSC_GET_CAPACITY,           //Returns uint32_t blk_nbr, uint32_t blk_size
    COMMAND_MSC_READ,                   //Returns data stream or error packet
    COMMAND_MSC_WRITE,                  //Waits for data stream or returns error packet
    COMMAND_MSC_DISCONNECT              //Returns same packet after sending Stop command to device
}
InterfaceCommandMscTypeDef;


typedef enum
{
    COMMAND_HID_GET_REPORT,             //Returns HID report from device
    COMMAND_HID_SET_REPORT              //Sends HID report to device. Simple ack packet contains no data.
}
InterfaceCommandHidTypeDef;


typedef enum
{
    COMMAND_ERROR_GENERIC,              //Something went wrong, time to FREAKOUT
    COMMAND_ERROR_DEVICE_DISCONNECTED,  //Device unexpectedly disconnected
}
InterfaceCommandErrorTypeDef;



#endif /* INC_DOWNSTREAM_INTERFACE_DEF_H_ */
