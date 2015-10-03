/**
  ******************************************************************************
  * File Name          : stm32f4xx_hal_msp.c
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


DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;



void HAL_MspInit(void)
{

  HAL_NVIC_SetPriority(SysTick_IRQn, INT_PRIORITY_SYSTICK, 0);
}


void HAL_SPI_MspInit(SPI_HandleTypeDef* hspi)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  if(hspi->Instance==SPI1)
  {
    __SPI1_CLK_ENABLE();
//    __DMA2_CLK_ENABLE();
  
    /**SPI1 GPIO Configuration
    PA4     ------> SPI_NSS
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI 
    */
    GPIO_InitStruct.Pin = GPIO_PIN_4;			//NSS is active low so pull up
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;		//SCK & data are active high so pull down
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    UPSTREAM_TX_REQUEST_DEASSERT;
    GPIO_InitStruct.Pin = UPSTREAM_TX_REQUEST_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(UPSTREAM_TX_REQUEST_PORT, &GPIO_InitStruct);

    //Interrupt-based SPI now!
    HAL_NVIC_SetPriority(SPI1_IRQn, INT_PRIORITY_SPI, 0);
    HAL_NVIC_EnableIRQ(SPI1_IRQn);

//    /* Peripheral DMA init*/
//    hdma_spi1_rx.Instance = DMA2_Stream2;
//    hdma_spi1_rx.Init.Channel = DMA_CHANNEL_3;
//    hdma_spi1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
//    hdma_spi1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
//    hdma_spi1_rx.Init.MemInc = DMA_MINC_ENABLE;
//    hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//    hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//    hdma_spi1_rx.Init.Mode = DMA_NORMAL;
//    hdma_spi1_rx.Init.Priority = DMA_PRIORITY_MEDIUM;
//    hdma_spi1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
//    HAL_DMA_Init(&hdma_spi1_rx);
//    __HAL_LINKDMA(hspi,hdmarx,hdma_spi1_rx);
//
//    hdma_spi1_tx.Instance = DMA2_Stream3;
//    hdma_spi1_tx.Init.Channel = DMA_CHANNEL_3;
//    hdma_spi1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
//    hdma_spi1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
//    hdma_spi1_tx.Init.MemInc = DMA_MINC_ENABLE;
//    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
//    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
//    hdma_spi1_tx.Init.Mode = DMA_NORMAL;
//    hdma_spi1_tx.Init.Priority = DMA_PRIORITY_MEDIUM;
//    hdma_spi1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
//	HAL_DMA_Init(&hdma_spi1_tx);
//    __HAL_LINKDMA(hspi,hdmatx,hdma_spi1_tx);
//
//    /* DMA interrupt init */
//    HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, INT_PRIORITY_SPI_DMA, 0);
//    HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
//    HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, INT_PRIORITY_SPI_DMA, 0);
//    HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  }
}


void HAL_SPI_MspDeInit(SPI_HandleTypeDef* hspi)
{

  if(hspi->Instance==SPI1)
  {
    __SPI1_CLK_DISABLE();
  
    /**SPI1 GPIO Configuration    
    PA5     ------> SPI1_SCK
    PA6     ------> SPI1_MISO
    PA7     ------> SPI1_MOSI 
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);

    /* Peripheral DMA DeInit*/
//    HAL_DMA_DeInit(hspi->hdmarx);
//    HAL_DMA_DeInit(hspi->hdmatx);
  }

}


/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
