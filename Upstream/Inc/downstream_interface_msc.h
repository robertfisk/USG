/*
 * downstream_interface_msc.h
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_INTERFACE_MSC_H_
#define INC_DOWNSTREAM_INTERFACE_MSC_H_


#include "downstream_spi.h"


typedef void (*DownstreamInterfaceMSCCallbackTypeDef)(HAL_StatusTypeDef result);
typedef void (*DownstreamInterfaceMSCCallbackPacketTypeDef)(HAL_StatusTypeDef result,
															DownstreamPacketTypeDef* downstreamPacket,
															uint16_t dataLength);
typedef void (*DownstreamInterfaceMSCCallbackUintPacketTypeDef)(HAL_StatusTypeDef result,
																uint32_t result_uint[],
																DownstreamPacketTypeDef* downstreamPacket);


HAL_StatusTypeDef DownstreamInterface_TestReady(DownstreamInterfaceMSCCallbackTypeDef callback);
HAL_StatusTypeDef DownstreamInterface_GetCapacity(DownstreamInterfaceMSCCallbackUintPacketTypeDef callback);
HAL_StatusTypeDef DownstreamInterface_BeginRead(DownstreamInterfaceMSCCallbackTypeDef callback,
												uint64_t readBlockStart,
												uint32_t readBlockCount,
												uint32_t readByteCount);
HAL_StatusTypeDef DownstreamInterface_GetStreamDataPacket(DownstreamInterfaceMSCCallbackPacketTypeDef callback);
HAL_StatusTypeDef DownstreamInterface_BeginWrite(DownstreamInterfaceMSCCallbackTypeDef callback,
												 uint64_t readBlockStart,
												 uint32_t readBlockCount);
HAL_StatusTypeDef DownstreamInterface_PutStreamDataPacket(DownstreamPacketTypeDef* packetToSend,
														  uint32_t dataLength);



#endif /* INC_DOWNSTREAM_INTERFACE_MSC_H_ */
