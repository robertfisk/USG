/*
 * upstream_msc.h
 *
 *  Created on: 4/07/2015
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_UPSTREAM_MSC_H_
#define INC_UPSTREAM_MSC_H_


#include <upstream_spi.h>


typedef void (*UpstreamMSCCallbackTypeDef)(HAL_StatusTypeDef result);
typedef void (*UpstreamMSCCallbackPacketTypeDef)(UpstreamPacketTypeDef* upstreamPacket,
												 uint16_t dataLength8);
typedef void (*UpstreamMSCCallbackUintPacketTypeDef)(UpstreamPacketTypeDef* upstreamPacket,
													 uint32_t result_uint1,
													 uint32_t result_uint2);


HAL_StatusTypeDef Upstream_MSC_TestReady(UpstreamMSCCallbackTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_GetCapacity(UpstreamMSCCallbackUintPacketTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_BeginRead(UpstreamMSCCallbackTypeDef callback,
										 uint64_t readBlockStart,
										 uint32_t readBlockCount,
										 uint32_t readByteCount);
HAL_StatusTypeDef Upstream_MSC_GetStreamDataPacket(UpstreamMSCCallbackPacketTypeDef callback);
HAL_StatusTypeDef Upstream_MSC_BeginWrite(UpstreamMSCCallbackTypeDef callback,
										  uint64_t writeBlockStart,
										  uint32_t writeBlockCount);
HAL_StatusTypeDef Upstream_MSC_PutStreamDataPacket(UpstreamPacketTypeDef* packetToSend,
												   uint32_t dataLength8);



#endif /* INC_UPSTREAM_MSC_H_ */
