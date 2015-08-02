/*
 * upstream_interface_msc.h
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_INTERFACE_MSC_H_
#define INC_UPSTREAM_INTERFACE_MSC_H_


#include <upstream_spi.h>


typedef void (*UpstreamInterfaceMSCCallbackTypeDef)(HAL_StatusTypeDef result);
typedef void (*UpstreamInterfaceMSCCallbackPacketTypeDef)(HAL_StatusTypeDef result,
														  UpstreamPacketTypeDef* upstreamPacket,
														  uint16_t dataLength);
typedef void (*UpstreamInterfaceMSCCallbackUintPacketTypeDef)(HAL_StatusTypeDef result,
															  uint32_t result_uint[],
															  UpstreamPacketTypeDef* upstreamPacket);


HAL_StatusTypeDef UpstreamInterface_TestReady(UpstreamInterfaceMSCCallbackTypeDef callback);
HAL_StatusTypeDef UpstreamInterface_GetCapacity(UpstreamInterfaceMSCCallbackUintPacketTypeDef callback);
HAL_StatusTypeDef UpstreamInterface_BeginRead(UpstreamInterfaceMSCCallbackTypeDef callback,
											  uint64_t readBlockStart,
											  uint32_t readBlockCount,
											  uint32_t readByteCount);
HAL_StatusTypeDef UpstreamInterface_GetStreamDataPacket(UpstreamInterfaceMSCCallbackPacketTypeDef callback);
HAL_StatusTypeDef UpstreamInterface_BeginWrite(UpstreamInterfaceMSCCallbackTypeDef callback,
											   uint64_t readBlockStart,
											   uint32_t readBlockCount);
HAL_StatusTypeDef UpstreamInterface_PutStreamDataPacket(UpstreamPacketTypeDef* packetToSend,
														uint32_t dataLength);



#endif /* INC_UPSTREAM_INTERFACE_MSC_H_ */
