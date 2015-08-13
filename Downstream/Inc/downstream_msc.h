/*
 * downstream_msc.h
 *
 *  Created on: 8/08/2015
 *      Author: Robert Fisk
 */

#ifndef INC_DOWNSTREAM_MSC_H_
#define INC_DOWNSTREAM_MSC_H_


#include "downstream_spi.h"


HAL_StatusTypeDef Downstream_MSC_ApproveConnectedDevice(void);
void Downstream_MSC_PacketProcessor(DownstreamPacketTypeDef* receivedPacket);



#endif /* INC_DOWNSTREAM_MSC_H_ */
