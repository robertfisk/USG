/**
  ******************************************************************************
  * @file    usbh_msc_bot.c 
  * @author  MCD Application Team
  * @version V3.2.1
  * @date    26-June-2015
  * @brief   This file includes the BOT protocol related functions
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
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
  *
  * Modifications by Robert Fisk
  */


/* Includes ------------------------------------------------------------------*/
#include "usbh_msc_bot.h"
#include "usbh_msc.h"
#include "downstream_spi.h"
#include "downstream_msc.h"
#include "downstream_statemachine.h"



static USBH_StatusTypeDef USBH_MSC_BOT_Abort(USBH_HandleTypeDef *phost, uint8_t lun, uint8_t dir);
static BOT_CSWStatusTypeDef USBH_MSC_DecodeCSW(USBH_HandleTypeDef *phost);
void USBH_MSC_BOT_Read_Multipacket_FreePacketCallback(DownstreamPacketTypeDef* freePacket);
void USBH_MSC_BOT_Read_Multipacket_PrepareURB(USBH_HandleTypeDef *phost);
void USBH_MSC_BOT_Write_Multipacket_ReceivePacketCallback(DownstreamPacketTypeDef* receivedPacket,
                                              uint16_t dataLength);
void USBH_MSC_BOT_Write_Multipacket_PrepareURB(USBH_HandleTypeDef *phost);



USBH_HandleTypeDef *Callback_MSC_phost;


/**
  * @brief  USBH_MSC_BOT_REQ_Reset 
  *         The function the MSC BOT Reset request.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_MSC_BOT_REQ_Reset(USBH_HandleTypeDef *phost)
{
  
  phost->Control.setup.b.bmRequestType = USB_H2D | USB_REQ_TYPE_CLASS | \
                              USB_REQ_RECIPIENT_INTERFACE;
  
  phost->Control.setup.b.bRequest = USB_REQ_BOT_RESET;
  phost->Control.setup.b.wValue.w = 0;
  phost->Control.setup.b.wIndex.w = 0;
  phost->Control.setup.b.wLength.w = 0;           
  
  return USBH_CtlReq(phost, 0 , 0 );  
}

/**
  * @brief  USBH_MSC_BOT_REQ_GetMaxLUN 
  *         The function the MSC BOT GetMaxLUN request.
  * @param  phost: Host handle
  * @param  Maxlun: pointer to Maxlun variable
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_MSC_BOT_REQ_GetMaxLUN(USBH_HandleTypeDef *phost, uint8_t *Maxlun)
{
  phost->Control.setup.b.bmRequestType = USB_D2H | USB_REQ_TYPE_CLASS | \
                              USB_REQ_RECIPIENT_INTERFACE;
  
  phost->Control.setup.b.bRequest = USB_REQ_GET_MAX_LUN;
  phost->Control.setup.b.wValue.w = 0;
  phost->Control.setup.b.wIndex.w = 0;
  phost->Control.setup.b.wLength.w = 1;           
  
  return USBH_CtlReq(phost, Maxlun , 1 ); 
}



/**
  * @brief  USBH_MSC_BOT_Init 
  *         The function Initializes the BOT protocol.
  * @param  phost: Host handle
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_MSC_BOT_Init(USBH_HandleTypeDef *phost)
{
  
  MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  
  MSC_Handle->hbot.cbw.field.Signature = BOT_CBW_SIGNATURE;
  MSC_Handle->hbot.cbw.field.Tag = BOT_CBW_TAG;
  MSC_Handle->hbot.state = BOT_SEND_CBW;    
  MSC_Handle->hbot.cmd_state = BOT_CMD_SEND;   
  
  return USBH_OK;
}



/**
  * @brief  USBH_MSC_BOT_Process 
  *         The function handle the BOT protocol.
  * @param  phost: Host handle
  * @param  lun: Logical Unit Number
  * @retval USBH Status
  */
USBH_StatusTypeDef USBH_MSC_BOT_Process (USBH_HandleTypeDef *phost, uint8_t lun)
{
  USBH_StatusTypeDef   status = USBH_BUSY;
  USBH_StatusTypeDef   error  = USBH_BUSY;  
  BOT_CSWStatusTypeDef CSW_Status = BOT_CSW_CMD_FAILED;
  USBH_URBStateTypeDef URB_Status = USBH_URB_IDLE;
  MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  uint8_t toggle = 0;
  
  switch (MSC_Handle->hbot.state)
  {
  case BOT_SEND_CBW:
    MSC_Handle->hbot.cbw.field.LUN = lun;
    MSC_Handle->hbot.state = BOT_SEND_CBW_WAIT;    
    USBH_BulkSendData (phost,
                       MSC_Handle->hbot.cbw.data, 
                       BOT_CBW_LENGTH, 
                       MSC_Handle->OutPipe,
                       1);
    break;
    
  case BOT_SEND_CBW_WAIT:
    
    URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->OutPipe); 
    
    if(URB_Status == USBH_URB_DONE)
    { 
      if ( MSC_Handle->hbot.cbw.field.DataTransferLength != 0 )
      {
        /* If there is Data Transfer Stage */
        if (((MSC_Handle->hbot.cbw.field.Flags) & USB_REQ_DIR_MASK) == USB_D2H)
        {
          /* Data Direction is IN */
          MSC_Handle->hbot.state = BOT_DATA_IN;
        }
        else
        {
          /* Data Direction is OUT */
          MSC_Handle->hbot.state = BOT_DATA_OUT;
        } 
      }
      
      else
      {/* If there is NO Data Transfer Stage */
        MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
      }
    
    }   
    else if(URB_Status == USBH_URB_NOTREADY)
    {
      /* Re-send CBW */
      MSC_Handle->hbot.state = BOT_SEND_CBW;
    }     
    else if(URB_Status == USBH_URB_STALL)
    {
      MSC_Handle->hbot.state  = BOT_ERROR_OUT;
    }
    break;
    
  case BOT_DATA_IN:
    if (MSC_Handle->hbot.pbuf != NULL)
    {
        //Simple single-buffer operation
        MSC_Handle->hbot.state = BOT_DATA_IN_WAIT;
        MSC_Handle->hbot.this_URB_size = MIN(MSC_Handle->hbot.cbw.field.DataTransferLength,
                                             MSC_Handle->InEpSize);
        USBH_BulkReceiveData (phost,
                              MSC_Handle->hbot.pbuf,
                              MSC_Handle->hbot.this_URB_size,
                              MSC_Handle->InPipe);
    }
    else
    {
        //Asynchronous multi-packet operation: get first packet
        MSC_Handle->hbot.state = BOT_DATA_IN_WAIT_FREE_PACKET;
        Callback_MSC_phost = phost;
        if (Downstream_GetFreePacket(USBH_MSC_BOT_Read_Multipacket_FreePacketCallback) != HAL_OK)
        {
            MSC_Handle->hbot.state = BOT_ERROR_IN;
        }
    }
    break;
    
  case BOT_DATA_IN_WAIT_FREE_PACKET:
      break;

  case BOT_DATA_IN_WAIT:  
    URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->InPipe); 

    if (URB_Status == USBH_URB_DONE)
    {
        if (USBH_LL_GetLastXferSize(phost, MSC_Handle->InPipe) != MSC_Handle->hbot.this_URB_size)
        {
            while(1);
        }

        MSC_Handle->hbot.cbw.field.DataTransferLength -= MSC_Handle->hbot.this_URB_size;

        if (MSC_Handle->hbot.pbuf != NULL)
        {
            //Simple single-buffer operation: everything must fit in one URB
            MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
        }
        else
        {
            //Asynchronous multi-packet operation
            MSC_Handle->hbot.bot_packet_bytes_remaining -= MSC_Handle->hbot.this_URB_size;
            MSC_Handle->hbot.bot_packet_pbuf += MSC_Handle->hbot.this_URB_size;

            if (MSC_Handle->hbot.cbw.field.DataTransferLength == 0)
            {
                //End of reception: dispatch last packet
                MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
                Downstream_MSC_PutStreamDataPacket(MSC_Handle->hbot.bot_packet,
                                                   (BOT_PAGE_LENGTH - MSC_Handle->hbot.bot_packet_bytes_remaining));
            }
            else
            {
                //Still more data to receive
                if (MSC_Handle->hbot.bot_packet_bytes_remaining == 0)
                {
                    //Dispatch current bot_packet, then get a new one
                    if (Downstream_MSC_PutStreamDataPacket(MSC_Handle->hbot.bot_packet,
                                                           BOT_PAGE_LENGTH) != HAL_OK)
                    {
                        MSC_Handle->hbot.state  = BOT_ERROR_IN;
                        break;
                    }
                    MSC_Handle->hbot.state = BOT_DATA_IN_WAIT_FREE_PACKET;
                    if (Downstream_GetFreePacket(USBH_MSC_BOT_Read_Multipacket_FreePacketCallback) != HAL_OK)
                    {
                        MSC_Handle->hbot.state  = BOT_ERROR_IN;
                        break;
                    }
                }
                else
                {
                    //Continue filling the current bot_packet
                    USBH_MSC_BOT_Read_Multipacket_PrepareURB(phost);
                }
            }
        }
    }

    else if (URB_Status == USBH_URB_STALL)
    {
      /* This is Data IN Stage STALL Condition */
      MSC_Handle->hbot.state  = BOT_ERROR_IN;
      
      /* Refer to USB Mass-Storage Class : BOT (www.usb.org) 
      6.7.2 Host expects to receive data from the device
      3. On a STALL condition receiving data, then:
      The host shall accept the data received.
      The host shall clear the Bulk-In pipe.
      4. The host shall attempt to receive a CSW.*/
    }     
    break;  
    
  case BOT_DATA_OUT:
    if (MSC_Handle->hbot.pbuf != NULL)
    {
        //Simple single-buffer operation
        MSC_Handle->hbot.state = BOT_DATA_OUT_WAIT;
        MSC_Handle->hbot.this_URB_size = MIN(MSC_Handle->hbot.cbw.field.DataTransferLength,
                                             MSC_Handle->OutEpSize);
        USBH_BulkSendData (phost,
                           MSC_Handle->hbot.pbuf,
                           MSC_Handle->hbot.this_URB_size,
                           MSC_Handle->OutPipe,
                           1);
    }
    else
    {
        //Asynchronous multi-packet operation: get first packet
        MSC_Handle->hbot.state = BOT_DATA_OUT_WAIT_RECEIVE_PACKET;
        Callback_MSC_phost = phost;
        if (Downstream_MSC_GetStreamDataPacket(USBH_MSC_BOT_Write_Multipacket_ReceivePacketCallback) != HAL_OK)
        {
            MSC_Handle->hbot.state  = BOT_ERROR_OUT;
        }
    }
    break;

  case BOT_DATA_OUT_WAIT_RECEIVE_PACKET:
      break;
    
  case BOT_DATA_OUT_WAIT:
    URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->OutPipe);     
    
    if(URB_Status == USBH_URB_DONE)
    {
        MSC_Handle->hbot.cbw.field.DataTransferLength -= MSC_Handle->hbot.this_URB_size;

        if (MSC_Handle->hbot.pbuf != NULL)
        {
            //Simple single-buffer operation: everything must fit in one URB
            MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
        }
        else
        {
            //Asynchronous multi-packet operation
            MSC_Handle->hbot.bot_packet_bytes_remaining -= MSC_Handle->hbot.this_URB_size;
            MSC_Handle->hbot.bot_packet_pbuf += MSC_Handle->hbot.this_URB_size;
            if (MSC_Handle->hbot.cbw.field.DataTransferLength == 0)
            {
                //End of transmission
                Downstream_ReleasePacket(MSC_Handle->hbot.bot_packet);
                MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
            }
            else
            {
                //Still more data to send
                if (MSC_Handle->hbot.bot_packet_bytes_remaining == 0)
                {
                    //Get next bot_packet
                    Downstream_ReleasePacket(MSC_Handle->hbot.bot_packet);
                    MSC_Handle->hbot.state = BOT_DATA_OUT_WAIT_RECEIVE_PACKET;
                    if (Downstream_MSC_GetStreamDataPacket(USBH_MSC_BOT_Write_Multipacket_ReceivePacketCallback) != HAL_OK)
                    {
                        MSC_Handle->hbot.state = BOT_ERROR_OUT;
                    }
                }
                else
                {
                    //Continue writing the current bot_packet
                    USBH_MSC_BOT_Write_Multipacket_PrepareURB(phost);
                }
            }
        }
    }
    
    else if(URB_Status == USBH_URB_NOTREADY)
    {
      /* Resend same data */
        if (MSC_Handle->hbot.pbuf != NULL)
        {
            MSC_Handle->hbot.state = BOT_DATA_OUT;
        }
        else
        {
            USBH_MSC_BOT_Write_Multipacket_PrepareURB(phost);
        }
    }
    
    else if(URB_Status == USBH_URB_STALL)
    {
      MSC_Handle->hbot.state = BOT_ERROR_OUT;
      
      /* Refer to USB Mass-Storage Class : BOT (www.usb.org) 
      6.7.3 Ho - Host expects to send data to the device
      3. On a STALL condition sending data, then:
      " The host shall clear the Bulk-Out pipe.
      4. The host shall attempt to receive a CSW.
      */
    }
    break;
    
  case BOT_RECEIVE_CSW:
    
    USBH_BulkReceiveData (phost,
                          MSC_Handle->hbot.csw.data, 
                          BOT_CSW_LENGTH , 
                          MSC_Handle->InPipe);
    
    MSC_Handle->hbot.state  = BOT_RECEIVE_CSW_WAIT;
    break;
    
  case BOT_RECEIVE_CSW_WAIT:
    
    URB_Status = USBH_LL_GetURBState(phost, MSC_Handle->InPipe); 
    
    /* Decode CSW */
    if(URB_Status == USBH_URB_DONE)
    {
      MSC_Handle->hbot.state = BOT_SEND_CBW;    
      MSC_Handle->hbot.cmd_state = BOT_CMD_SEND;        
      CSW_Status = USBH_MSC_DecodeCSW(phost);
      
      if(CSW_Status == BOT_CSW_CMD_PASSED)
      {
        status = USBH_OK;
      }
      else
      {
        status = USBH_FAIL;
      }
    }
    else if(URB_Status == USBH_URB_STALL)     
    {
      MSC_Handle->hbot.state  = BOT_ERROR_IN;
    }
    break;
    
  case BOT_ERROR_IN: 
    error = USBH_MSC_BOT_Abort(phost, lun, BOT_DIR_IN);
    
    if (error == USBH_OK)
    {
      MSC_Handle->hbot.state = BOT_RECEIVE_CSW;
    }
    else if (error == USBH_UNRECOVERED_ERROR)
    {
      /* This means that there is a STALL Error limit, Do Reset Recovery */
      MSC_Handle->hbot.state = BOT_UNRECOVERED_ERROR;
    }
    break;
    
  case BOT_ERROR_OUT: 
    error = USBH_MSC_BOT_Abort(phost, lun, BOT_DIR_OUT);
    
    if ( error == USBH_OK)
    { 
      
      toggle = USBH_LL_GetToggle(phost, MSC_Handle->OutPipe); 
      USBH_LL_SetToggle(phost, MSC_Handle->OutPipe, 1- toggle);   
      USBH_LL_SetToggle(phost, MSC_Handle->InPipe, 0);  
      MSC_Handle->hbot.state = BOT_ERROR_IN;        
    }
    else if (error == USBH_UNRECOVERED_ERROR)
    {
      MSC_Handle->hbot.state = BOT_UNRECOVERED_ERROR;
    }
    break;
    
    
  case BOT_UNRECOVERED_ERROR: 
    status = USBH_MSC_BOT_REQ_Reset(phost);
    if ( status == USBH_OK)
    {
      MSC_Handle->hbot.state = BOT_SEND_CBW; 
    }
    break;
    
  default:      
    break;
  }
  return status;
}


void USBH_MSC_BOT_Read_Multipacket_FreePacketCallback(DownstreamPacketTypeDef* freePacket)
{
    MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) Callback_MSC_phost->pActiveClass->pData;

    if (MSC_Handle->hbot.state != BOT_DATA_IN_WAIT_FREE_PACKET)
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    MSC_Handle->hbot.state = BOT_DATA_IN_WAIT;
    MSC_Handle->hbot.bot_packet = freePacket;
    MSC_Handle->hbot.bot_packet_pbuf = freePacket->Data;
    MSC_Handle->hbot.bot_packet_bytes_remaining = BOT_PAGE_LENGTH;
    USBH_MSC_BOT_Read_Multipacket_PrepareURB(Callback_MSC_phost);
}


void USBH_MSC_BOT_Read_Multipacket_PrepareURB(USBH_HandleTypeDef *phost)
{
    uint32_t temp_URB_size;
    MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;

    temp_URB_size = MSC_Handle->hbot.cbw.field.DataTransferLength;
    if (temp_URB_size > MSC_Handle->hbot.bot_packet_bytes_remaining)
    {
        temp_URB_size = MSC_Handle->hbot.bot_packet_bytes_remaining;
    }
    if (temp_URB_size > MSC_Handle->InEpSize)
    {
        temp_URB_size = MSC_Handle->InEpSize;
    }
    MSC_Handle->hbot.this_URB_size = (uint16_t)temp_URB_size;

    USBH_BulkReceiveData(phost,
                         MSC_Handle->hbot.bot_packet_pbuf,
                         MSC_Handle->hbot.this_URB_size,
                         MSC_Handle->InPipe);
}


void USBH_MSC_BOT_Write_Multipacket_ReceivePacketCallback(DownstreamPacketTypeDef* receivedPacket,
                                                          uint16_t dataLength)
{
    MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) Callback_MSC_phost->pActiveClass->pData;

    if (MSC_Handle->hbot.state != BOT_DATA_OUT_WAIT_RECEIVE_PACKET)
    {
        Downstream_PacketProcessor_FreakOut();
        return;
    }

    MSC_Handle->hbot.state = BOT_DATA_OUT_WAIT;
    MSC_Handle->hbot.bot_packet = receivedPacket;
    MSC_Handle->hbot.bot_packet_pbuf = receivedPacket->Data;
    MSC_Handle->hbot.bot_packet_bytes_remaining = dataLength;
    USBH_MSC_BOT_Write_Multipacket_PrepareURB(Callback_MSC_phost);
}


void USBH_MSC_BOT_Write_Multipacket_PrepareURB(USBH_HandleTypeDef *phost)
{
    uint32_t temp_URB_size;
    MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;

    temp_URB_size = MSC_Handle->hbot.cbw.field.DataTransferLength;
    if (temp_URB_size > MSC_Handle->hbot.bot_packet_bytes_remaining)
    {
        temp_URB_size = MSC_Handle->hbot.bot_packet_bytes_remaining;
    }
    if (temp_URB_size > MSC_Handle->OutEpSize)
    {
        temp_URB_size = MSC_Handle->OutEpSize;
    }
    MSC_Handle->hbot.this_URB_size = (uint16_t)temp_URB_size;

    USBH_BulkSendData (phost,
                       MSC_Handle->hbot.bot_packet_pbuf,
                       MSC_Handle->hbot.this_URB_size,
                       MSC_Handle->OutPipe,
                       1);
}


/**
  * @brief  USBH_MSC_BOT_Abort 
  *         The function handle the BOT Abort process.
  * @param  phost: Host handle
  * @param  lun: Logical Unit Number
  * @param  dir: direction (0: out / 1 : in)
  * @retval USBH Status
  */
static USBH_StatusTypeDef USBH_MSC_BOT_Abort(USBH_HandleTypeDef *phost, uint8_t lun, uint8_t dir)
{
  USBH_StatusTypeDef status = USBH_FAIL;
  MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  
  switch (dir)
  {
  case BOT_DIR_IN :
    /* send ClrFeture on Bulk IN endpoint */
    status = USBH_ClrFeature(phost, MSC_Handle->InEp);
    
    break;
    
  case BOT_DIR_OUT :
    /*send ClrFeature on Bulk OUT endpoint */
    status = USBH_ClrFeature(phost, MSC_Handle->OutEp);
    break;
    
  default:
    break;
  }
  return status;
}

/**
  * @brief  USBH_MSC_BOT_DecodeCSW
  *         This function decodes the CSW received by the device and updates the
  *         same to upper layer.
  * @param  phost: Host handle
  * @retval USBH Status
  * @notes
  *     Refer to USB Mass-Storage Class : BOT (www.usb.org)
  *    6.3.1 Valid CSW Conditions :
  *     The host shall consider the CSW valid when:
  *     1. dCSWSignature is equal to 53425355h
  *     2. the CSW is 13 (Dh) bytes in length,
  *     3. dCSWTag matches the dCBWTag from the corresponding CBW.
  */

static BOT_CSWStatusTypeDef USBH_MSC_DecodeCSW(USBH_HandleTypeDef *phost)
{
  MSC_HandleTypeDef *MSC_Handle =  (MSC_HandleTypeDef *) phost->pActiveClass->pData;
  BOT_CSWStatusTypeDef status = BOT_CSW_CMD_FAILED;
  
    /*Checking if the transfer length is different than 13*/    
    if(USBH_LL_GetLastXferSize(phost, MSC_Handle->InPipe) != BOT_CSW_LENGTH)
    {
      /*(4) Hi > Dn (Host expects to receive data from the device,
      Device intends to transfer no data)
      (5) Hi > Di (Host expects to receive data from the device,
      Device intends to send data to the host)
      (9) Ho > Dn (Host expects to send data to the device,
      Device intends to transfer no data)
      (11) Ho > Do  (Host expects to send data to the device,
      Device intends to receive data from the host)*/
      
      
      status = BOT_CSW_PHASE_ERROR;
    }
    else
    { /* CSW length is Correct */
      
      /* Check validity of the CSW Signature and CSWStatus */
      if(MSC_Handle->hbot.csw.field.Signature == BOT_CSW_SIGNATURE)
      {/* Check Condition 1. dCSWSignature is equal to 53425355h */
        
        if(MSC_Handle->hbot.csw.field.Tag == MSC_Handle->hbot.cbw.field.Tag)
        {
          /* Check Condition 3. dCSWTag matches the dCBWTag from the 
          corresponding CBW */

          if(MSC_Handle->hbot.csw.field.Status == 0) 
          {
            /* Refer to USB Mass-Storage Class : BOT (www.usb.org) 
            
            Hn Host expects no data transfers
            Hi Host expects to receive data from the device
            Ho Host expects to send data to the device
            
            Dn Device intends to transfer no data
            Di Device intends to send data to the host
            Do Device intends to receive data from the host
            
            Section 6.7 
            (1) Hn = Dn (Host expects no data transfers,
            Device intends to transfer no data)
            (6) Hi = Di (Host expects to receive data from the device,
            Device intends to send data to the host)
            (12) Ho = Do (Host expects to send data to the device, 
            Device intends to receive data from the host)
            
            */
            
            status = BOT_CSW_CMD_PASSED;
          }
          else if(MSC_Handle->hbot.csw.field.Status == 1)
          {
            status = BOT_CSW_CMD_FAILED;
          }
          
          else if(MSC_Handle->hbot.csw.field.Status == 2)
          { 
            /* Refer to USB Mass-Storage Class : BOT (www.usb.org) 
            Section 6.7 
            (2) Hn < Di ( Host expects no data transfers, 
            Device intends to send data to the host)
            (3) Hn < Do ( Host expects no data transfers, 
            Device intends to receive data from the host)
            (7) Hi < Di ( Host expects to receive data from the device, 
            Device intends to send data to the host)
            (8) Hi <> Do ( Host expects to receive data from the device, 
            Device intends to receive data from the host)
            (10) Ho <> Di (Host expects to send data to the device,
            Di Device intends to send data to the host)
            (13) Ho < Do (Host expects to send data to the device, 
            Device intends to receive data from the host)
            */
            
            status = BOT_CSW_PHASE_ERROR;
          }
        } /* CSW Tag Matching is Checked  */
      } /* CSW Signature Correct Checking */
      else
      {
        /* If the CSW Signature is not valid, We sall return the Phase Error to
        Upper Layers for Reset Recovery */
        
        status = BOT_CSW_PHASE_ERROR;
      }
    } /* CSW Length Check*/
    
  return status;
}


/**
* @}
*/ 

/**
* @}
*/ 

/**
* @}
*/

/**
* @}
*/ 

/**
* @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/



