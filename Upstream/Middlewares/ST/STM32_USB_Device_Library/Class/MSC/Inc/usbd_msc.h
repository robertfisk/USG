/**
  ******************************************************************************
  * @file    usbd_msc.h
  * @author  MCD Application Team
  * @version V2.3.0
  * @date    04-November-2014
  * @brief   Header for the usbd_msc.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_H
#define __USBD_MSC_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc_bot.h"
#include "usbd_msc_scsi.h"
#include "usbd_ioreq.h"
#include "downstream_spi.h"

/** @addtogroup USBD_MSC_BOT
  * @{
  */
  
/** @defgroup USBD_MSC
  * @brief This file is the Header file for usbd_msc.c
  * @{
  */ 


/** @defgroup USBD_BOT_Exported_Defines
  * @{
  */ 
#define MSC_MAX_FS_PACKET            0x40
#define MSC_MAX_HS_PACKET            0x200

#define BOT_GET_MAX_LUN              0xFE
#define BOT_RESET                    0xFF
#define USB_MSC_CONFIG_DESC_SIZ      32
 

#define MSC_EPIN_ADDR                0x81 
#define MSC_EPOUT_ADDR               0x01 

/**
  * @}
  */ 

/** @defgroup USB_CORE_Exported_Types
  * @{
  */ 


typedef struct
{
  uint32_t                   max_lun;
  uint32_t                   interface;
  uint8_t                    bot_state;
  uint8_t                    bot_status;
  uint16_t                   bot_data_length;
  uint8_t*                   bot_data;
  DownstreamPacketTypeDef*	 bot_packet;			//Not NULL indicates we currently own a downstream packet buffer, and should free it when we are done.
  USBD_MSC_BOT_CBWTypeDef    cbw;
  USBD_MSC_BOT_CSWTypeDef    csw;
  
  USBD_SCSI_SenseTypeDef   scsi_sense [SENSE_LIST_DEPTH];
  uint8_t                  scsi_sense_head;
  uint8_t                  scsi_sense_tail;
  
  uint16_t                 scsi_blk_size;			//LOGICAL BLOCK LENGTH IN BYTES: Number of bytes of user data in a logical block [SBC-4]
  uint32_t                 scsi_blk_nbr;			//This is total block count = LOGICAL BLOCK ADDRESS + 1. LOGICAL BLOCK ADDRESS: LBA of the last logical block on the direct access block device [SBC-4]

  uint32_t                 scsi_blk_addr;			//LOGICAL BLOCK ADDRESS: Starting with the logical block referenced [SBC-4]
  uint16_t                 scsi_blk_len;			//TRANSFER LENGTH: Number of contiguous logical blocks of data that shall be read [SBC-4]

}
USBD_MSC_BOT_HandleTypeDef; 

/* Structure for MSC process */
extern USBD_ClassTypeDef  USBD_MSC;
#define USBD_MSC_CLASS    &USBD_MSC

/**
  * @}
  */ 

/**
  * @}
  */ 

#ifdef __cplusplus
}
#endif

#endif  /* __USBD_MSC_H */
/**
  * @}
  */ 
  
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
