/**
  ******************************************************************************
  * File Name          : stm32f4xx_hal_msp.c
  * Date               : 03/02/2015 20:27:00
  * Description        : This file provides code for the MSP Initialization 
  *                      and de-Initialization codes.
  ******************************************************************************
  *
  * COPYRIGHT(c) 2015 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  *
  * Modifications by Robert Fisk
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "interrupts.h"
#include "board_config.h"


DMA_HandleTypeDef   spiTxDmaHandle;
DMA_HandleTypeDef   spiRxDmaHandle;


/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
//  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
//
//  /* System interrupt init*/
///* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, INT_PRIORITY_SYSTICK, 0);

}


void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  if(hspi->Instance==SPI1)
  {
    /* Peripheral clock enable */
      __HAL_RCC_SPI1_CLK_ENABLE();
      __HAL_RCC_DMA2_CLK_ENABLE();

    /**SPI1 GPIO Configuration    
    PA4     ------> GPIO manual slave select
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    SPI1_NSS_DEASSERT;
    GPIO_InitStruct.Pin = SPI1_NSS_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SPI1_NSS_PORT, &GPIO_InitStruct);

    //Configure downstream request pin and interrupt
    GPIO_InitStruct.Pin = DOWNSTREAM_TX_OK_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT | GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DOWNSTREAM_TX_OK_PORT, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI3_IRQn, INT_PRIORITY_EXT3I, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    //Prepare Tx DMA stream
    hspi->hdmatx = &spiTxDmaHandle;
    spiTxDmaHandle.Instance = DMA2_Stream3;
    spiTxDmaHandle.Parent = hspi;
    spiTxDmaHandle.Init.Channel = DMA_CHANNEL_3;
    spiTxDmaHandle.Init.Direction = DMA_MEMORY_TO_PERIPH;
    spiTxDmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
    spiTxDmaHandle.Init.MemInc = DMA_MINC_ENABLE;
    spiTxDmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    spiTxDmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    spiTxDmaHandle.Init.Mode = DMA_NORMAL;
    spiTxDmaHandle.Init.Priority = DMA_PRIORITY_MEDIUM;
    spiTxDmaHandle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&spiTxDmaHandle);
    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, INT_PRIORITY_SPI_DMA, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

    //Prepare Rx DMA stream
    hspi->hdmarx = &spiRxDmaHandle;
    spiRxDmaHandle.Instance = DMA2_Stream2;
    spiRxDmaHandle.Parent = hspi;
    spiRxDmaHandle.Init.Channel = DMA_CHANNEL_3;
    spiRxDmaHandle.Init.Direction = DMA_PERIPH_TO_MEMORY;
    spiRxDmaHandle.Init.PeriphInc = DMA_PINC_DISABLE;
    spiRxDmaHandle.Init.MemInc = DMA_MINC_ENABLE;
    spiRxDmaHandle.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    spiRxDmaHandle.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    spiRxDmaHandle.Init.Mode = DMA_NORMAL;
    spiRxDmaHandle.Init.Priority = DMA_PRIORITY_MEDIUM;
    spiRxDmaHandle.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&spiRxDmaHandle);
    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, INT_PRIORITY_SPI_DMA, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  }
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{

  if(hspi->Instance==SPI1)
  {
    /* Peripheral clock disable */
    __HAL_RCC_SPI1_CLK_DISABLE();
    __HAL_RCC_DMA2_CLK_DISABLE();
  
    /**SPI1 GPIO Configuration    
    PA4     ------> SPI1_NSS
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI 
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);
    HAL_DMA_DeInit(&spiTxDmaHandle);
    HAL_DMA_DeInit(&spiRxDmaHandle);

    HAL_NVIC_DisableIRQ(DMA2_Stream3_IRQn);
    HAL_NVIC_DisableIRQ(DMA2_Stream2_IRQn);
  }

}




/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
