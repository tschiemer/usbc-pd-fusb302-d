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

#ifndef __FUSB302_D_DRIVER_H_
#define __FUSB302_D_DRIVER_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "FUSB302-D.h"

#include "hw_i2c.h"


#define FUSB302_D_DEFAULT_ADDRESS 0b0100010 // 0x22

#define FUSB302_D_WRITE 0
#define FUSB302_D_READ 1

 // Bus wait after each command (1 microsec)
 #define FUSB302_D_BUS_FREE_TIME 0

 typedef enum {
	 FUSB302_D_OK = 0,
	 FUSB302_D_ERROR = !FUSB302_D_OK
 } FUSB302_D_Error_t;

 typedef struct {
 	I2C_HandleTypeDef * Hi2c;
 	uint16_t Addr;
	IRQn_Type IrqN;
 } FUSB302_D_t;

 typedef struct {
	 uint8_t DeviceID;
	 uint8_t Switches0;
	 uint8_t Switches1;
	 uint8_t Measure;
	 uint8_t Slice;
	 uint8_t Control0;
	 uint8_t Control1;
	 uint8_t Control2;
	 uint8_t Control3;
	 uint8_t Mask1;
	 uint8_t Power;
	 uint8_t Reset;
	 uint8_t OCPreg;
	 uint8_t Maska;
	 uint8_t Maskb;
	 uint8_t Control4;
	 uint8_t Status0a;
	 uint8_t Status1a;
	 uint8_t Interrupta;
	 uint8_t Interruptb;
	 uint8_t Status0;
	 uint8_t Status1;
	 uint8_t Interrupt;
	 uint8_t FIFOs;
 } FUSB302_D_Registers_st;


 FUSB302_D_Error_t FUSB302_D_Init( FUSB302_D_t * fusb, I2C_HandleTypeDef * hi2c, IRQn_Type irqN, uint16_t i2cAddr );
 FUSB302_D_Error_t FUSB302_D_DeInit( FUSB302_D_t * fusb );

 FUSB302_D_Error_t FUSB302_D_Probe( FUSB302_D_t * fusb, uint8_t ntrials, uint32_t timeout  );

 FUSB302_D_Error_t FUSB302_D_Read( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data );
 FUSB302_D_Error_t FUSB302_D_ReadN( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data, uint8_t n );
 FUSB302_D_Error_t FUSB302_D_Write( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t data );
 FUSB302_D_Error_t FUSB302_D_WriteN( FUSB302_D_t * fusb, FUSB302_D_Register_t reg, uint8_t * data, uint8_t n);


 void FUSB302_D_Test( I2C_HandleTypeDef * hi2c, IRQn_Type irqN, uint16_t i2cAddr );

#ifdef __cplusplus
 }
#endif



#endif /* __FUSB302_D_DRIVER_H_ */
