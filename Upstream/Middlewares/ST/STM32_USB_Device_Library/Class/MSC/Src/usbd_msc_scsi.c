/**
  ******************************************************************************
  * @file    usbd_msc_scsi.c
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    04-November-2014
  * @brief   This file provides all the USBD SCSI layer functions.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include <upstream_interface_def.h>
#include <upstream_msc.h>
#include <upstream_spi.h>
#include "usbd_msc_bot.h"
#include "usbd_msc_scsi.h"
#include "usbd_msc.h"
#include "usbd_msc_data.h"
#include "usbd_descriptors.h"


/** @addtogroup STM32_USB_DEVICE_LIBRARY
  * @{
  */


/** @defgroup MSC_SCSI 
  * @brief Mass storage SCSI layer module
  * @{
  */ 

/** @defgroup MSC_SCSI_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup MSC_SCSI_Private_Defines
  * @{
  */ 

/**
  * @}
  */ 


/** @defgroup MSC_SCSI_Private_Macros
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup MSC_SCSI_Private_Variables
  * @{
  */ 

USBD_HandleTypeDef				*SCSI_ProcessCmd_pdev;
uint8_t							SCSI_ProcessCmd_lun;
uint8_t							*SCSI_ProcessCmd_params;
SCSI_ProcessCmdCallbackTypeDef	SCSI_ProcessCmd_callback;
USBD_MSC_BOT_HandleTypeDef		*SCSI_ProcessCmd_hmsc;


/**
  * @}
  */ 


/** @defgroup MSC_SCSI_Private_FunctionPrototypes
  * @{
  */ 
static void SCSI_TestUnitReady(void);
static void SCSI_Inquiry(void);
static void SCSI_ReadFormatCapacity(void);
static void SCSI_ReadCapacity10(void);
static void SCSI_RequestSense (void);
static void SCSI_StartStopUnit(void);
static void SCSI_ModeSense6 (void);
static void SCSI_ModeSense10 (void);
static void SCSI_Write10(void);
static void SCSI_Read10(void);
static void SCSI_Verify10(void);
static int8_t SCSI_CheckAddressRange (uint32_t blk_offset , uint16_t blk_nbr);

void SCSI_TestUnitReadyCallback(HAL_StatusTypeDef result);
void SCSI_ReadCapacity10Callback(HAL_StatusTypeDef result,
								 uint32_t result_uint[],
								 UpstreamPacketTypeDef* upstreamPacket);
void SCSI_ReadFormatCapacityCallback(HAL_StatusTypeDef result,
									 uint32_t result_uint[],
									 UpstreamPacketTypeDef* packetToUse);
void SCSI_Read10BeginCallback(HAL_StatusTypeDef result);
void SCSI_Read10ReplyCallback(HAL_StatusTypeDef result,
							  UpstreamPacketTypeDef* upstreamPacket,
							  uint16_t dataLength);
void SCSI_Write10BeginCallback(HAL_StatusTypeDef result);
void SCSI_Write10FreePacketCallback(UpstreamPacketTypeDef* freePacket);


/**
  * @}
  */ 


/** @defgroup MSC_SCSI_Private_Functions
  * @{
  */ 


/**
* @brief  SCSI_ProcessCmd
*         Process SCSI commands
* @param  pdev: device instance
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
void SCSI_ProcessCmd(USBD_HandleTypeDef  *pdev,
                     uint8_t lun,
                     uint8_t *params,
					 SCSI_ProcessCmdCallbackTypeDef callback)
{
	//Save all our parameters for easy access in callback routines
	SCSI_ProcessCmd_pdev = pdev;
	SCSI_ProcessCmd_params = params;
	SCSI_ProcessCmd_lun = lun;
	SCSI_ProcessCmd_callback = callback;
	SCSI_ProcessCmd_hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData;

  switch (params[0])
  {
  case SCSI_TEST_UNIT_READY:
    SCSI_TestUnitReady();
    return;
    
  case SCSI_REQUEST_SENSE:
    SCSI_RequestSense();
    return;

  case SCSI_INQUIRY:
    SCSI_Inquiry();
    return;
    
  case SCSI_START_STOP_UNIT:
    SCSI_StartStopUnit();
    return;
    
  case SCSI_ALLOW_MEDIUM_REMOVAL:
    SCSI_StartStopUnit();
    return;
    
  case SCSI_MODE_SENSE6:
    SCSI_ModeSense6();
    return;
    
  case SCSI_MODE_SENSE10:
    SCSI_ModeSense10();
    return;
    
  case SCSI_READ_FORMAT_CAPACITIES:
    SCSI_ReadFormatCapacity();
    return;
    
  case SCSI_READ_CAPACITY10:
    SCSI_ReadCapacity10();
    return;
    
  case SCSI_READ10:
    SCSI_Read10();
    return;
    
  case SCSI_WRITE10:
    SCSI_Write10();
    return;
    
  case SCSI_VERIFY10:
    SCSI_Verify10();
    return;
    
  default:
    SCSI_SenseCode(pdev, 
                   lun,
                   ILLEGAL_REQUEST, 
                   INVALID_CDB);    
    SCSI_ProcessCmd_callback(-1);
  }
}


/**
* @brief  SCSI_TestUnitReady
*         Process SCSI Test Unit Ready Command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
void SCSI_TestUnitReady(void)
{
  /* case 9 : Hi > D0 */
  if (SCSI_ProcessCmd_hmsc->cbw.dDataLength != 0)
  {
    SCSI_SenseCode(SCSI_ProcessCmd_pdev,
    			   SCSI_ProcessCmd_lun,
                   ILLEGAL_REQUEST, 
                   INVALID_CDB);
    SCSI_ProcessCmd_callback(-1);
    return;
  }
  
  if (SCSI_ProcessCmd_lun >= UPSTREAM_LUN_NBR)
  {
	  SCSI_TestUnitReadyCallback(HAL_ERROR);
	  return;
  }

  if (Upstream_MSC_TestReady(SCSI_TestUnitReadyCallback) != HAL_OK)
  {
	  SCSI_TestUnitReadyCallback(HAL_ERROR);
  }
}


void SCSI_TestUnitReadyCallback(HAL_StatusTypeDef result)
{
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_hmsc->bot_state = USBD_BOT_NO_DATA;
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	//Success!
	SCSI_ProcessCmd_hmsc->bot_data_length = 0;
	SCSI_ProcessCmd_callback(0);
}




/**
* @brief  SCSI_Inquiry
*         Process Inquiry command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_Inquiry(void)
{
	uint8_t* pPage;
	uint16_t len;
	UpstreamPacketTypeDef* freePacket;

	freePacket = Upstream_GetFreePacketImmediately();
	SCSI_ProcessCmd_hmsc->bot_packet = freePacket;
	SCSI_ProcessCmd_hmsc->bot_data = freePacket->Data;

	if (SCSI_ProcessCmd_params[1] & 0x01)/*Evpd is set*/
	{
		pPage = (uint8_t *)MSC_Page00_Inquiry_Data;
		len = LENGTH_INQUIRY_PAGE00;
	}
	else
	{
		//Return the same info for any LUN requested
		pPage = (uint8_t *)&STORAGE_Inquirydata_FS;
		len = pPage[4] + 5;

		if (SCSI_ProcessCmd_params[4] <= len)
		{
			len = SCSI_ProcessCmd_params[4];
		}
	}
	SCSI_ProcessCmd_hmsc->bot_data_length = len;

	while (len)
	{
		len--;
		SCSI_ProcessCmd_hmsc->bot_data[len] = pPage[len];
	}
	SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_ReadCapacity10
*         Process Read Capacity 10 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_ReadCapacity10(void)
{
	if (Upstream_MSC_GetCapacity(SCSI_ReadCapacity10Callback) != HAL_OK)
	{
		SCSI_ReadCapacity10Callback(HAL_ERROR, NULL, NULL);
	}
}


void SCSI_ReadCapacity10Callback(HAL_StatusTypeDef result,
								 uint32_t result_uint[],
								 UpstreamPacketTypeDef* upstreamPacket)
{
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	SCSI_ProcessCmd_hmsc->bot_packet = upstreamPacket;
	SCSI_ProcessCmd_hmsc->bot_data = upstreamPacket->Data;

	SCSI_ProcessCmd_hmsc->scsi_blk_nbr = result_uint[0];
	SCSI_ProcessCmd_hmsc->scsi_blk_size = result_uint[1];

	SCSI_ProcessCmd_hmsc->bot_data[0] = (uint8_t)((SCSI_ProcessCmd_hmsc->scsi_blk_nbr - 1) >> 24);
	SCSI_ProcessCmd_hmsc->bot_data[1] = (uint8_t)((SCSI_ProcessCmd_hmsc->scsi_blk_nbr - 1) >> 16);
	SCSI_ProcessCmd_hmsc->bot_data[2] = (uint8_t)((SCSI_ProcessCmd_hmsc->scsi_blk_nbr - 1) >>  8);
	SCSI_ProcessCmd_hmsc->bot_data[3] = (uint8_t)(SCSI_ProcessCmd_hmsc->scsi_blk_nbr - 1);
    
	SCSI_ProcessCmd_hmsc->bot_data[4] = (uint8_t)(SCSI_ProcessCmd_hmsc->scsi_blk_size >>  24);
	SCSI_ProcessCmd_hmsc->bot_data[5] = (uint8_t)(SCSI_ProcessCmd_hmsc->scsi_blk_size >>  16);
	SCSI_ProcessCmd_hmsc->bot_data[6] = (uint8_t)(SCSI_ProcessCmd_hmsc->scsi_blk_size >>  8);
	SCSI_ProcessCmd_hmsc->bot_data[7] = (uint8_t)(SCSI_ProcessCmd_hmsc->scsi_blk_size);
    
	SCSI_ProcessCmd_hmsc->bot_data_length = 8;
	SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_ReadFormatCapacity
*         Process Read Format Capacity command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_ReadFormatCapacity(void)
{
	if (Upstream_MSC_GetCapacity(SCSI_ReadFormatCapacityCallback) != HAL_OK)
	{
		SCSI_ReadFormatCapacityCallback(HAL_ERROR, NULL, NULL);
	}
}


void SCSI_ReadFormatCapacityCallback(HAL_StatusTypeDef result,
									 uint32_t result_uint[],
									 UpstreamPacketTypeDef* packetToUse)
{
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	SCSI_ProcessCmd_hmsc->bot_packet = packetToUse;
	SCSI_ProcessCmd_hmsc->bot_data = packetToUse->Data;

	SCSI_ProcessCmd_hmsc->bot_data[0] = 0;
	SCSI_ProcessCmd_hmsc->bot_data[1] = 0;
	SCSI_ProcessCmd_hmsc->bot_data[2] = 0;
	SCSI_ProcessCmd_hmsc->bot_data[3] = 0x08;
	SCSI_ProcessCmd_hmsc->bot_data[4] = (uint8_t)((result_uint[0] - 1) >> 24);
	SCSI_ProcessCmd_hmsc->bot_data[5] = (uint8_t)((result_uint[0] - 1) >> 16);
	SCSI_ProcessCmd_hmsc->bot_data[6] = (uint8_t)((result_uint[0] - 1) >>  8);
	SCSI_ProcessCmd_hmsc->bot_data[7] = (uint8_t)(result_uint[0] - 1);

	SCSI_ProcessCmd_hmsc->bot_data[8] = 0x02;
	SCSI_ProcessCmd_hmsc->bot_data[9] = (uint8_t)(result_uint[1] >>  16);
	SCSI_ProcessCmd_hmsc->bot_data[10] = (uint8_t)(result_uint[1] >>  8);
	SCSI_ProcessCmd_hmsc->bot_data[11] = (uint8_t)(result_uint[1]);

	SCSI_ProcessCmd_hmsc->bot_data_length = 12;
    SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_ModeSense6
*         Process Mode Sense6 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_ModeSense6 (void)
{
	uint16_t len = 8;
	UpstreamPacketTypeDef* freePacket;

	freePacket = Upstream_GetFreePacketImmediately();
	SCSI_ProcessCmd_hmsc->bot_packet = freePacket;
	SCSI_ProcessCmd_hmsc->bot_data = freePacket->Data;

	SCSI_ProcessCmd_hmsc->bot_data_length = len;

	while (len)
	{
		len--;
		SCSI_ProcessCmd_hmsc->bot_data[len] = MSC_Mode_Sense6_data[len];
	}
	SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_ModeSense10
*         Process Mode Sense10 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_ModeSense10(void)
{
	uint16_t len = 8;
	UpstreamPacketTypeDef* freePacket;

	freePacket = Upstream_GetFreePacketImmediately();
	SCSI_ProcessCmd_hmsc->bot_packet = freePacket;
	SCSI_ProcessCmd_hmsc->bot_data = freePacket->Data;

	SCSI_ProcessCmd_hmsc->bot_data_length = len;

	while (len)
	{
		len--;
		SCSI_ProcessCmd_hmsc->bot_data[len] = MSC_Mode_Sense10_data[len];
	}
	SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_RequestSense
*         Process Request Sense command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/

static void SCSI_RequestSense(void)
{
	uint8_t i;
	UpstreamPacketTypeDef* freePacket;

	freePacket = Upstream_GetFreePacketImmediately();
	SCSI_ProcessCmd_hmsc->bot_packet = freePacket;
	SCSI_ProcessCmd_hmsc->bot_data = freePacket->Data;

	for (i=0 ; i < REQUEST_SENSE_DATA_LEN ; i++)
	{
	  SCSI_ProcessCmd_hmsc->bot_data[i] = 0;
	}

	SCSI_ProcessCmd_hmsc->bot_data[0]	= 0x70;
	SCSI_ProcessCmd_hmsc->bot_data[7]	= REQUEST_SENSE_DATA_LEN - 6;

	if((SCSI_ProcessCmd_hmsc->scsi_sense_head != SCSI_ProcessCmd_hmsc->scsi_sense_tail))
	{
	  SCSI_ProcessCmd_hmsc->bot_data[2]     = SCSI_ProcessCmd_hmsc->scsi_sense[SCSI_ProcessCmd_hmsc->scsi_sense_head].Skey;
	  SCSI_ProcessCmd_hmsc->bot_data[12]    = SCSI_ProcessCmd_hmsc->scsi_sense[SCSI_ProcessCmd_hmsc->scsi_sense_head].w.b.ASCQ;
	  SCSI_ProcessCmd_hmsc->bot_data[13]    = SCSI_ProcessCmd_hmsc->scsi_sense[SCSI_ProcessCmd_hmsc->scsi_sense_head].w.b.ASC;
	  SCSI_ProcessCmd_hmsc->scsi_sense_head++;
      if (SCSI_ProcessCmd_hmsc->scsi_sense_head == SENSE_LIST_DEPTH)
	  {
    	SCSI_ProcessCmd_hmsc->scsi_sense_head = 0;
	  }
	}
	SCSI_ProcessCmd_hmsc->bot_data_length = REQUEST_SENSE_DATA_LEN;

	if (SCSI_ProcessCmd_params[4] <= REQUEST_SENSE_DATA_LEN)
	{
	  SCSI_ProcessCmd_hmsc->bot_data_length = SCSI_ProcessCmd_params[4];
	}
	SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_SenseCode
*         Load the last error code in the error list
* @param  lun: Logical unit number
* @param  sKey: Sense Key
* @param  ASC: Additional Sense Key
* @retval none

*/
void SCSI_SenseCode(USBD_HandleTypeDef  *pdev, uint8_t lun, uint8_t sKey, uint8_t ASC)
{
  USBD_MSC_BOT_HandleTypeDef  *hmsc = (USBD_MSC_BOT_HandleTypeDef*)pdev->pClassData; 
  
  hmsc->scsi_sense[hmsc->scsi_sense_tail].Skey  = sKey;
  hmsc->scsi_sense[hmsc->scsi_sense_tail].w.ASC = ASC << 8;
  hmsc->scsi_sense_tail++;
  if (hmsc->scsi_sense_tail == SENSE_LIST_DEPTH)
  {
    hmsc->scsi_sense_tail = 0;
  }
}
/**
* @brief  SCSI_StartStopUnit
*         Process Start Stop Unit command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_StartStopUnit(void)
{
  SCSI_ProcessCmd_hmsc->bot_data_length = 0;
  SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_Read10
*         Process Read10 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/
static void SCSI_Read10(void)
{
	if (SCSI_ProcessCmd_hmsc->bot_state == USBD_BOT_IDLE)  /* Idle */
	{
		/* case 10 : Ho <> Di */
		if ((SCSI_ProcessCmd_hmsc->cbw.bmFlags & 0x80) != 0x80)
		{
			SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						 SCSI_ProcessCmd_hmsc->cbw.bLUN,
						 ILLEGAL_REQUEST,
						 INVALID_CDB);
			SCSI_ProcessCmd_callback(-1);
			return;
		}

		SCSI_ProcessCmd_hmsc->scsi_blk_addr = (SCSI_ProcessCmd_params[2] << 24) | \
											  (SCSI_ProcessCmd_params[3] << 16) | \
											  (SCSI_ProcessCmd_params[4] <<  8) | \
											  SCSI_ProcessCmd_params[5];

		SCSI_ProcessCmd_hmsc->scsi_blk_len =  (SCSI_ProcessCmd_params[7] <<  8) | \
											  SCSI_ProcessCmd_params[8];

		if (SCSI_CheckAddressRange(SCSI_ProcessCmd_hmsc->scsi_blk_addr,
								   SCSI_ProcessCmd_hmsc->scsi_blk_len) < 0)
		{
			SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						   SCSI_ProcessCmd_hmsc->cbw.bLUN,
						   ILLEGAL_REQUEST,
						   INVALID_CDB);
			SCSI_ProcessCmd_callback(-1); /* error */
			return;
		}

		/* cases 4,5 : Hi <> Dn */
		if (SCSI_ProcessCmd_hmsc->cbw.dDataLength != (uint32_t)(SCSI_ProcessCmd_hmsc->scsi_blk_len * SCSI_ProcessCmd_hmsc->scsi_blk_size))
		{
		  SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						 SCSI_ProcessCmd_hmsc->cbw.bLUN,
						 ILLEGAL_REQUEST,
						 INVALID_CDB);
		  SCSI_ProcessCmd_callback(-1);
		  return;
		}

		if (Upstream_MSC_BeginRead(SCSI_Read10BeginCallback,
										  SCSI_ProcessCmd_hmsc->scsi_blk_addr,
										  SCSI_ProcessCmd_hmsc->scsi_blk_len,
										  SCSI_ProcessCmd_hmsc->cbw.dDataLength) != HAL_OK)
		{
			SCSI_Read10BeginCallback(HAL_ERROR);
		}
		return;
	}

	//hmsc->bot_state is already USBD_BOT_DATA_IN
	if (Upstream_MSC_GetStreamDataPacket(SCSI_Read10ReplyCallback) != HAL_OK)
	{
		SCSI_Read10ReplyCallback(HAL_ERROR, NULL, 0);
	}
}


void SCSI_Read10BeginCallback(HAL_StatusTypeDef result)
{
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
		return;
	}
	SCSI_ProcessCmd_hmsc->bot_state = USBD_BOT_DATA_IN;

	if (Upstream_MSC_GetStreamDataPacket(SCSI_Read10ReplyCallback) != HAL_OK)
	{
		SCSI_Read10ReplyCallback(HAL_ERROR, NULL, 0);
	}
}


void SCSI_Read10ReplyCallback(HAL_StatusTypeDef result,
							  UpstreamPacketTypeDef* upstreamPacket,
							  uint16_t dataLength)
{
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
	                   HARDWARE_ERROR,
	                   UNRECOVERED_READ_ERROR);
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	if (SCSI_ProcessCmd_hmsc->bot_packet != NULL)
		while (1);			/////////////////////////////////////////!

	SCSI_ProcessCmd_hmsc->bot_packet = upstreamPacket;
	SCSI_ProcessCmd_hmsc->bot_data = upstreamPacket->Data;
	USBD_LL_Transmit (SCSI_ProcessCmd_pdev,
					  MSC_EPIN_ADDR,
					  SCSI_ProcessCmd_hmsc->bot_data,
					  dataLength);

	/* case 6 : Hi = Di */
	SCSI_ProcessCmd_hmsc->csw.dDataResidue -= dataLength;

	if (SCSI_ProcessCmd_hmsc->csw.dDataResidue == 0)
	{
		SCSI_ProcessCmd_hmsc->bot_state = USBD_BOT_LAST_DATA_IN;
	}
	SCSI_ProcessCmd_callback(0);
}


/**
* @brief  SCSI_Write10
*         Process Write10 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/

static void SCSI_Write10(void)
{
	uint32_t dataLength;

	if (SCSI_ProcessCmd_hmsc->bot_state == USBD_BOT_IDLE) /* Idle */
	{
		/* case 8 : Hi <> Do */
		if ((SCSI_ProcessCmd_hmsc->cbw.bmFlags & 0x80) == 0x80)
		{
			SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						   SCSI_ProcessCmd_hmsc->cbw.bLUN,
						   ILLEGAL_REQUEST,
						   INVALID_CDB);
			SCSI_ProcessCmd_callback(-1);
			return;
		}

		SCSI_ProcessCmd_hmsc->scsi_blk_addr = (SCSI_ProcessCmd_params[2] << 24) | \
											  (SCSI_ProcessCmd_params[3] << 16) | \
											  (SCSI_ProcessCmd_params[4] <<  8) | \
											  SCSI_ProcessCmd_params[5];
		SCSI_ProcessCmd_hmsc->scsi_blk_len = (SCSI_ProcessCmd_params[7] <<  8) | \
											  SCSI_ProcessCmd_params[8];

		if (SCSI_CheckAddressRange(SCSI_ProcessCmd_hmsc->scsi_blk_addr,
								   SCSI_ProcessCmd_hmsc->scsi_blk_len) < 0)
		{
			SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						   SCSI_ProcessCmd_hmsc->cbw.bLUN,
						   ILLEGAL_REQUEST,
						   INVALID_CDB);
			SCSI_ProcessCmd_callback(-1); /* error */
			return;
		}

		/* cases 3,11,13 : Hn,Ho <> D0 */
		if (SCSI_ProcessCmd_hmsc->cbw.dDataLength != (uint32_t)(SCSI_ProcessCmd_hmsc->scsi_blk_len * SCSI_ProcessCmd_hmsc->scsi_blk_size))
		{
			SCSI_SenseCode(SCSI_ProcessCmd_pdev,
						   SCSI_ProcessCmd_hmsc->cbw.bLUN,
						   ILLEGAL_REQUEST,
						   INVALID_CDB);
			SCSI_ProcessCmd_callback(-1);
			return;
		}

		if (Upstream_MSC_BeginWrite(SCSI_Write10BeginCallback,
										   SCSI_ProcessCmd_hmsc->scsi_blk_addr,
										   SCSI_ProcessCmd_hmsc->scsi_blk_len) != HAL_OK)
		{
			SCSI_Write10BeginCallback(HAL_ERROR);
		}
		return;
	}


	//hmsc->bot_state is already USBD_BOT_DATA_OUT
	dataLength = MIN(SCSI_ProcessCmd_hmsc->csw.dDataResidue, MSC_MEDIA_PACKET);
	if (Upstream_MSC_PutStreamDataPacket(SCSI_ProcessCmd_hmsc->bot_packet,
												dataLength) != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   HARDWARE_ERROR,
					   WRITE_FAULT);
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	SCSI_ProcessCmd_hmsc->csw.dDataResidue -= dataLength;
	if (SCSI_ProcessCmd_hmsc->csw.dDataResidue == 0)
	{
		MSC_BOT_SendCSW (SCSI_ProcessCmd_pdev, USBD_CSW_CMD_PASSED);
		SCSI_ProcessCmd_callback(0);
		return;
	}

	/* Prepare EP to Receive next packet */
	if (Upstream_GetFreePacket(SCSI_Write10FreePacketCallback) != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
	}
}


void SCSI_Write10BeginCallback(HAL_StatusTypeDef result)
{
	if (result == HAL_BUSY)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   WRITE_PROTECTED);
		SCSI_ProcessCmd_callback(-1);
		return;
	}
	if (result != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
		return;
	}

	/* Prepare EP to receive first data packet */
	SCSI_ProcessCmd_hmsc->bot_state = USBD_BOT_DATA_OUT;
	if (Upstream_GetFreePacket(SCSI_Write10FreePacketCallback) != HAL_OK)
	{
		SCSI_SenseCode(SCSI_ProcessCmd_pdev,
					   SCSI_ProcessCmd_lun,
					   NOT_READY,
					   MEDIUM_NOT_PRESENT);
		SCSI_ProcessCmd_callback(-1);
	}
}


void SCSI_Write10FreePacketCallback(UpstreamPacketTypeDef* freePacket)
{
	SCSI_ProcessCmd_hmsc->bot_packet = freePacket;
	SCSI_ProcessCmd_hmsc->bot_data = freePacket->Data;

	USBD_LL_PrepareReceive (SCSI_ProcessCmd_pdev,
							MSC_EPOUT_ADDR,
							SCSI_ProcessCmd_hmsc->bot_data,
							MIN(SCSI_ProcessCmd_hmsc->csw.dDataResidue, MSC_MEDIA_PACKET));
	SCSI_ProcessCmd_callback(0);		//Report eventual success!
}


/**
* @brief  SCSI_Verify10
*         Process Verify10 command
* @param  lun: Logical unit number
* @param  params: Command parameters
* @retval status
*/

static void SCSI_Verify10(void)
{
  if ((SCSI_ProcessCmd_params[1]& 0x02) == 0x02)
  {
    SCSI_SenseCode (SCSI_ProcessCmd_pdev,
    				SCSI_ProcessCmd_lun,
                    ILLEGAL_REQUEST, 
                    INVALID_FIELD_IN_COMMAND);
    SCSI_ProcessCmd_callback(-1);		/* Error, Verify Mode Not supported*/
    return;
  }
  
  if(SCSI_CheckAddressRange(SCSI_ProcessCmd_hmsc->scsi_blk_addr,
							SCSI_ProcessCmd_hmsc->scsi_blk_len) < 0)
  {
	  SCSI_ProcessCmd_callback(-1); /* error */
	  return;
  }
  SCSI_ProcessCmd_hmsc->bot_data_length = 0;
  SCSI_ProcessCmd_callback(0);
}

/**
* @brief  SCSI_CheckAddressRange
*         Check address range
* @param  lun: Logical unit number
* @param  blk_offset: first block address
* @param  blk_nbr: number of block to be processed
* @retval status
*/
static int8_t SCSI_CheckAddressRange (uint32_t blk_offset , uint16_t blk_nbr)
{
  if ((blk_offset + blk_nbr) > SCSI_ProcessCmd_hmsc->scsi_blk_nbr )
  {
    SCSI_SenseCode(SCSI_ProcessCmd_pdev,
    			   SCSI_ProcessCmd_lun,
                   ILLEGAL_REQUEST, 
                   ADDRESS_OUT_OF_RANGE);
    return -1;
  }
  return 0;
}

/**
  * @}
  */ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
