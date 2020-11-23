/**
  * usbc-pd-fusb302-d: Library for ONSEMI FUSB302-D (USB-C Controller) for PD negotiation
  * Copyright (C) 2020  Philip Tschiemer https://filou.se
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public
  * License as published by the Free Software Foundation; either
  * version 3 of the License, or (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public License
  * along with this program; if not, write to the Free Software Foundation,
  * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
  */
	
#include "hw_i2c.h"

static bool Initialized = false;
I2C_HandleTypeDef Hi2c;

void HW_I2C_Init( void )
{
	if (Initialized)
	{
		return;
	}

	Hi2c.Instance             = I2CX;
	Hi2c.Init.Timing          = I2C_TIMING;
	Hi2c.Init.OwnAddress1     = I2C_ADDRESS1;
	Hi2c.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
	Hi2c.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	Hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	Hi2c.Init.OwnAddress2     = I2C_ADDRESS2;
	Hi2c.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	Hi2c.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;//I2C_NOSTRETCH_ENABLE

	if(HAL_I2C_Init(&Hi2c) != HAL_OK)
	{
		DBG("i2c error\n");
		/* Initialization Error */
		Error_Handler( ErrorCodeI2CFail );
	}

	Initialized = true;
}

void HW_I2C_DeInit( void )
{
	if (!Initialized)
	{
		return;
	}

	HAL_I2C_DeInit(&Hi2c);

	Initialized = false;
}

uint8_t HW_I2C_Probe( uint8_t ntrials, uint32_t timeout, I2C_AddressFound callback )
{
	uint8_t found = 0;
	for (uint8_t i = 0; i < 128; i++)
	{
		if (HAL_I2C_IsDeviceReady( &Hi2c, i << 1, ntrials, timeout) == HAL_OK)
		{
			found++;
			callback( i );
		}

//		DelayMs(1);
	}
	return found;

}

I2C_HandleTypeDef * HW_I2C_Handle( void )
{
	return &Hi2c;
}


/**
  * @brief I2C MSP Initialization
  *        This function configures the hardware resources used in this example:
  *           - Peripheral's clock enable
  *           - Peripheral's GPIO Configuration
  *           - DMA configuration for transmission request by peripheral
  *           - NVIC configuration for DMA interrupt request enable
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef  GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef  RCC_PeriphCLKInitStruct;

  /*##-1- Configure the I2C clock source. The clock is derived from the SYSCLK #*/
  RCC_PeriphCLKInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2CX;
  RCC_PeriphCLKInitStruct.I2c1ClockSelection = RCC_I2CXCLKSOURCE_SYSCLK;
  HAL_RCCEx_PeriphCLKConfig(&RCC_PeriphCLKInitStruct);

  /*##-2- Enable peripherals and GPIO Clocks #################################*/
  /* Enable GPIO TX/RX clock */
  I2CX_SCL_GPIO_CLK_ENABLE();
  I2CX_SDA_GPIO_CLK_ENABLE();
  /* Enable I2Cx clock */
  I2CX_CLK_ENABLE();

  /*##-3- Configure peripheral GPIO ##########################################*/
  /* I2C TX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = I2CX_SCL_PIN;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull      = GPIO_PULLUP;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = I2CX_SCL_SDA_AF;
  HAL_GPIO_Init(I2CX_SCL_GPIO_PORT, &GPIO_InitStruct);

  /* I2C RX GPIO pin configuration  */
  GPIO_InitStruct.Pin       = I2CX_SDA_PIN;
  GPIO_InitStruct.Alternate = I2CX_SCL_SDA_AF;
  HAL_GPIO_Init(I2CX_SDA_GPIO_PORT, &GPIO_InitStruct);

  /*##-4- Configure the NVIC for I2C ########################################*/
  /* NVIC for I2Cx */
//  HAL_NVIC_SetPriority(I2Cx_IRQn, 0, 1);
//  HAL_NVIC_EnableIRQ(I2Cx_IRQn);
}

/**
  * @brief I2C MSP De-Initialization
  *        This function frees the hardware resources used in this example:
  *          - Disable the Peripheral's clock
  *          - Revert GPIO, DMA and NVIC configuration to their default state
  * @param hi2c: I2C handle pointer
  * @retval None
  */
void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
  /*##-1- Reset peripherals ##################################################*/
  I2CX_FORCE_RESET();
  I2CX_RELEASE_RESET();

  /*##-2- Disable peripherals and GPIO Clocks #################################*/
  /* Configure I2C Tx as alternate function  */
  HAL_GPIO_DeInit(I2CX_SCL_GPIO_PORT, I2CX_SCL_PIN);
  /* Configure I2C Rx as alternate function  */
  HAL_GPIO_DeInit(I2CX_SDA_GPIO_PORT, I2CX_SDA_PIN);

  /*##-3- Disable the NVIC for I2C ##########################################*/
//  HAL_NVIC_DisableIRQ(I2Cx_IRQn);

}

#ifdef DEBUG
void HW_I2C_test_detect_( uint8_t addr )
{
	if (addr == 255)
	{
		DBG(">> HW_I2C_test_detect\n");
		uint8_t found = HW_I2C_Probe( 3, 100, HW_I2C_test_detect_ );
		DBG("detected %d i2c addresses\n", found);
	}
	else
	{
		DBG("i2c device at %02X\n", addr);
	}
}
#endif
