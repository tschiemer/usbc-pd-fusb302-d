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
  
#ifndef __HW_I2C_H__
#define __HW_I2C_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

#error TODO hw.h is ment to include any specific platform/STM32 HAL headers
#include "hw.h"

typedef void ( * I2C_AddressFound )( uint8_t addr );

void HW_I2C_Init( void );
void HW_I2C_DeInit( void );

uint8_t HW_I2C_Probe( uint8_t ntrials, uint32_t timeout, I2C_AddressFound callback );

I2C_HandleTypeDef * HW_I2C_Handle( void );


#ifdef DEBUG
void HW_I2C_test_detect_( uint8_t addr );
inline void HW_I2C_test_detect( void )
{
	HW_I2C_test_detect_(255);
}
#endif


#ifdef __cplusplus
}
#endif


#endif /* __HW_I2C_H__ */
