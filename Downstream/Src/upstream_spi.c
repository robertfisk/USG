/*
 * upstream_spi.c
 *
 *  Created on: 24/07/2015
 *      Author: Robert Fisk
 */


#include "upstream_spi.h"
#include "upstream_interface_def.h"


SPI_HandleTypeDef		Hspi1;

InterfaceStateTypeDef	UpstreamInterfaceState;



void SPI1_Init(void);



void Upstream_InitInterface(void)
{
	UpstreamInterfaceState = INTERFACE_STATE_RESET;

	SPI1_Init();

}


void SPI1_Init(void)
{
	  Hspi1.Instance = SPI1;
	  Hspi1.Init.Mode = SPI_MODE_SLAVE;
	  Hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	  Hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	  Hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	  Hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	  Hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
	  Hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
	  Hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	  Hspi1.Init.TIMode = SPI_TIMODE_DISABLED;
	  Hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_ENABLE;
	  Hspi1.Init.CRCPolynomial = SPI_CRC_DEFAULTPOLYNOMIAL;
	  HAL_SPI_Init(&Hspi1);
}


