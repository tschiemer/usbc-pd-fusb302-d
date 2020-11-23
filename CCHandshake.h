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

#ifndef CCHANDSHAKE_H_
#define CCHANDSHAKE_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "FUSB302-D_Driver.h"

#if !defined(CCHANDSHAKE_REQUIRE_BC_LVL) && defined(__FUSB302_D_DRIVER_H_)
#define CCHANDSHAKE_REQUIRE_BC_LVL FUSB302_D_Status0_BC_LVL_200mV_to_660mV
#endif

#define ONSEMI_LIBRARY false
#define CCHANDSHAKE_AUTONOMOUS false



typedef enum {
	CCHandshake_CC_None = 0,
	CCHandshake_CC_1 = 1,
	CCHandshake_CC_2 = 2
} CCHandshake_CC_t;

typedef enum {
	CCHandshake_PD_Role_Source,
	CCHandshake_PD_Role_Sink
} CCHandshake_PD_Role_t;


#if ONSEMI_LIBRARY==false

#endif /* ONSEMILIBRARY == false */

void CCHandshake_init( void );
void CCHandshake_deinit( void );

CCHandshake_CC_t CCHandshake_getOrientation( void );

void CCHandshake_core( void );

#if ONSEMI_LIBRARY==true
bool CCHandshake_hasInterrupt( void );
#else

#endif

#ifdef __cplusplus
 }
#endif

#endif /* CCHANDSHAKE_H_ */
