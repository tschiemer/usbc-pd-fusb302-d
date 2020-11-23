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

#include "FUSB302-D_Driver.h"

#include "delay.h"


#ifndef DBG
#define DBG(format, ...)
#endif



static FUSB302_D_Error_t FUSB302_D_WriteReg( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len );
static FUSB302_D_Error_t FUSB302_D_ReadReg( FUSB302_D_t * fusb, uint8_t offset, uint8_t * buf, uint16_t len );
//static FUSB302_D_Error_t FUSB302_D_ReadReg( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len );

static FUSB302_D_Error_t FUSB302_D_StartSequence( FUSB302_D_t * fusb );
static FUSB302_D_Error_t FUSB302_D_EndSequence( FUSB302_D_t * fusb );
static FUSB302_D_Error_t FUSB302_D_SequentialWrite( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len, uint32_t XferOptions );
static FUSB302_D_Error_t FUSB302_D_SequentialRead( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len, uint32_t XferOptions );


static FUSB302_D_Error_t FUSB302_D_WriteReg( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len )
{
	uint16_t addr = (fusb->Addr << 1) + FUSB302_D_WRITE;

	HAL_StatusTypeDef status = HAL_I2C_Master_Transmit( fusb->Hi2c, addr, buf, len, 1000 );

	if (HAL_OK != status){

		DBG("write error %d\n", status);
		return FUSB302_D_ERROR;
	}

//	DBG("wrote (%d) = ", len );
//	for (uint8_t i = 0; i < len; i++)
//	{
//		DBG("%02X", buf[i]);
//	}
//	DBG("\n");

	return FUSB302_D_OK;
}

static FUSB302_D_Error_t FUSB302_D_ReadReg( FUSB302_D_t * fusb, uint8_t registerAddress, uint8_t * buf, uint16_t len )
{
//	uint16_t addr = (fusb->Addr << 1) + FUSB302_D_READ;

	if (FUSB302_D_StartSequence( fusb ))
	{
		return FUSB302_D_ERROR;
	}

	FUSB302_D_Error_t result = FUSB302_D_SequentialWrite( fusb, &registerAddress, 1, I2C_FIRST_FRAME );
	if ( result == FUSB302_D_OK )
	{
		result = FUSB302_D_SequentialRead( fusb, buf, len, I2C_LAST_FRAME );
	}

	// make sure to always call EndSequence
	FUSB302_D_EndSequence( fusb );

//	HAL_StatusTypeDef status = HAL_I2C_Master_Receive(bq->Hi2c, addr, buf, len, 1000);
//
//	if (HAL_OK != status){
//		return FUSB302_D_ERROR;
//	}
//
//	DBG("read (%d) = ", len );
//	for (uint8_t i = 0; i < len; i++)
//	{
//		DBG("%02X", buf[i]);
//	}
//	DBG("\n");

	return result;
}

static FUSB302_D_Error_t FUSB302_D_StartSequence( FUSB302_D_t * fusb )
{
	  HAL_NVIC_SetPriority( fusb->IrqN, 0, 1);
	  HAL_NVIC_EnableIRQ( fusb->IrqN );

	  return FUSB302_D_OK;
}

static FUSB302_D_Error_t FUSB302_D_EndSequence( FUSB302_D_t * fusb )
{
	  HAL_NVIC_DisableIRQ( fusb->IrqN );

	  return FUSB302_D_OK;
}

static FUSB302_D_Error_t FUSB302_D_SequentialWrite( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len, uint32_t XferOptions )
{
	uint16_t addr = (fusb->Addr << 1) + FUSB302_D_WRITE;

	HAL_StatusTypeDef status =HAL_I2C_Master_Sequential_Transmit_IT(fusb->Hi2c, addr, buf, len, XferOptions);

	if (HAL_OK != status){

		return FUSB302_D_ERROR;
	}

	// wait for end of transmission
	while ( HAL_I2C_GetState( fusb->Hi2c ) != HAL_I2C_STATE_READY);

	if (HAL_I2C_GetError( fusb->Hi2c ) == HAL_I2C_ERROR_AF)
	{
		DBG("slave didn't acknowledge\n");
		return FUSB302_D_ERROR;
	}

//	DBG("wrote (%d) = ", len );
//	for (uint8_t i = 0; i < len; i++)
//	{
//		DBG("%02X", buf[i]);
//	}
//	DBG("\n");

	return FUSB302_D_OK;
}

static FUSB302_D_Error_t FUSB302_D_SequentialRead( FUSB302_D_t * fusb, uint8_t * buf, uint16_t len, uint32_t XferOptions )
{
	uint16_t addr = (fusb->Addr << 1) + FUSB302_D_READ;

	HAL_StatusTypeDef status = HAL_I2C_Master_Sequential_Receive_IT( fusb->Hi2c, addr, buf, len, XferOptions);

	if (HAL_OK != status){
		return FUSB302_D_ERROR;
	}
	// wait for end of reception
	while (HAL_I2C_GetState( fusb->Hi2c ) != HAL_I2C_STATE_READY);

	if (HAL_I2C_GetError( fusb->Hi2c ) == HAL_I2C_ERROR_AF)
	{
		DBG("slave didn't acknowledge\n");
		return FUSB302_D_ERROR;
	}

//	DBG("read (%d) = ", len );
//	for (uint8_t i = 0; i < len; i++)
//	{
//		DBG("%02X", buf[i]);
//	}
//	DBG("\n");

	return FUSB302_D_OK;
}


FUSB302_D_Error_t FUSB302_D_Init( FUSB302_D_t * fusb, I2C_HandleTypeDef * hi2c, IRQn_Type irqN, uint16_t i2cAddr )
{
	fusb->Hi2c = hi2c;
	fusb->IrqN = irqN;
	fusb->Addr = i2cAddr;

    return FUSB302_D_OK;
}

FUSB302_D_Error_t FUSB302_D_DeInit( FUSB302_D_t * fusb )
{
	fusb->Hi2c = NULL;

    return FUSB302_D_OK;
}

FUSB302_D_Error_t FUSB302_D_Probe( FUSB302_D_t * fusb, uint8_t ntrials, uint32_t timeout  )
{
	HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady( fusb->Hi2c, fusb->Addr << 1, ntrials, timeout);

	if (status != HAL_OK)
	{
		return FUSB302_D_ERROR;
	}

	return FUSB302_D_OK;
}

FUSB302_D_Error_t FUSB302_D_Read( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data )
{
	HAL_StatusTypeDef status = FUSB302_D_ReadReg( fusb, reg, data, 1 );

	#if FUSB302_D_BUS_FREE_TIME > 0
	DelayMs( FUSB302_D_BUS_FREE_TIME );
	#endif

	if (status != HAL_OK)
	{
		return FUSB302_D_ERROR;
	}

	return FUSB302_D_OK;
}


FUSB302_D_Error_t FUSB302_D_ReadN( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data, uint8_t n )
{
	HAL_StatusTypeDef status = FUSB302_D_ReadReg( fusb, reg, data, n );

	#if FUSB302_D_BUS_FREE_TIME > 0
	DelayMs( FUSB302_D_BUS_FREE_TIME );
	#endif

	if (status != HAL_OK)
	{
		return FUSB302_D_ERROR;
	}

	return FUSB302_D_OK;
}



FUSB302_D_Error_t FUSB302_D_Write( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t data )
{
	uint8_t buf[2];

	buf[0] = reg;
	buf[1] = data;

	HAL_StatusTypeDef status = FUSB302_D_WriteReg( fusb, buf, 2 );

	#if FUSB302_D_BUS_FREE_TIME > 0
	DelayMs( FUSB302_D_BUS_FREE_TIME );
	#endif

	if (status != HAL_OK)
	{
		return FUSB302_D_ERROR;
	}

	return FUSB302_D_OK;
}
FUSB302_D_Error_t FUSB302_D_WriteN( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data, uint8_t n)
{
	uint8_t buf[64];

	buf[0] = reg;

	// copy
	for (uint8_t i = 0; i < n; i++)
	{
		buf[i+1] = data[i];
	}

	HAL_StatusTypeDef status = FUSB302_D_WriteReg( fusb, buf, n+1 );

	#if FUSB302_D_BUS_FREE_TIME > 0
	DelayMs( FUSB302_D_BUS_FREE_TIME );
	#endif

	if (status != HAL_OK)
	{
		return FUSB302_D_ERROR;
	}

	return FUSB302_D_OK;
}

void FUSB302_D_Test( I2C_HandleTypeDef * hi2c, IRQn_Type irqN, uint16_t i2cAddr )
{
	FUSB302_D_t fusb;
//	FUSB302_D_Error_t error;

	FUSB302_D_Init( &fusb, hi2c, irqN, i2cAddr );

	if (FUSB302_D_Probe( &fusb, 3, 100 ) == FUSB302_D_ERROR)
	{
		DBG( "Probe error\n" );
	}
	else
	{
		uint8_t buf = 0;

		if (FUSB302_D_Read( &fusb, FUSB302_D_Register_DeviceID, &buf) == FUSB302_D_ERROR)
		{
			DBG( "Read device id error\n");
		}
		else
		{
			DBG( "deviceid = v%d rev%d\n", FUSB302_D_DeviceID_Version_get(buf), FUSB302_D_DeviceID_Revision_get(buf) );

			if (FUSB302_D_Read( &fusb, FUSB302_D_Register_Control0, &buf) == FUSB302_D_ERROR)
			{
				DBG( "Read device id error\n");
			}
			else
			{
				DBG( "control0 = %02X\n", buf );

				buf = (buf & ~FUSB302_D_Control0_HOST_CUR_MASK) | FUSB302_D_Control0_HOST_CUR_HighCurrentMode;

				if (FUSB302_D_Write( &fusb, FUSB302_D_Register_Control0, buf) == FUSB302_D_ERROR)
				{
					DBG( "write err\n");
				}
				else
				{

					if (FUSB302_D_Read( &fusb, FUSB302_D_Register_Control0, &buf) == FUSB302_D_ERROR)
					{
						DBG( "Read device id error\n");
					}
					else
					{
						DBG( "control0 = %02X\n", buf );
					}
				}
			}
		}
	}


	while(1);
}
