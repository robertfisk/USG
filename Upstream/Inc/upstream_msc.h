/*
 * upstream_msc.h
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 */

#ifndef INC_UPSTREAM_MSC_H_
#define INC_UPSTREAM_MSC_H_


#include <upstream_spi.h>


typedef void (*UpstreamMSCCallbackTypeDef)(HAL_StatusTypeDef result);
typedef void (*UpstreamMSCCallbackPacketTypeDef)(HAL_StatusTypeDef result,
												 UpstreamPacketTypeDef* upstreamPacket,
												 uint16_t dataLength);
typedef void (*UpstreamMSCCallbackUintPacketTypeDef)(HAL_StatusTypeDef result,
													 uint32_t result_uint[],
													 UpstreamPacketTypeDef* upstreamPacket);


HAL_StatusTypeDef Upstream_MSC_TestReady(UpstreamMSCCallbackTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_GetCapacity(UpstreamMSCCallbackUintPacketTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_BeginRead(UpstreamMSCCallbackTypeDef callback,
										 uint64_t readBlockStart,
										 uint32_t readBlockCount,
										 uint32_t readByteCount);
HAL_StatusTypeDef Upstream_MSC_GetStreamDataPacket(UpstreamMSCCallbackPacketTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_BeginWrite(UpstreamMSCCallbackTypeDef callback,
										  uint64_t readBlockStart,
										  uint32_t readBlockCount);
HAL_StatusTypeDef Upstream_MSC_PutStreamDataPacket(UpstreamPacketTypeDef* packetToSend,
												   uint32_t dataLength);



#endif /* INC_UPSTREAM_MSC_H_ */
