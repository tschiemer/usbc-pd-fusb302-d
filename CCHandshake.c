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

#include "CCHandshake.h"

#include "HW_i2c.h"

static FUSB302_D_t Driver;


#if ONSEMI_LIBRARY==true

#include "Platform_ARM/app/HWIO.h"
#include "Platform_ARM/app/TimeDelay.h"
#include "Platform_ARM/app/Timing.h"
#include "Platform_ARM/app/PlatformI2C.h"
//#include "core/TypeC.h"

#ifdef PWR
#undef PWR
#endif
#ifdef COMP
#undef COMP
#endif

#include "core/fusb30X.h"

extern DeviceReg_t              Registers;

#else

// #include "main.h"

#include <string.h>
#include "PD.h"
#include "util-string.h"	// https://github.com/tschiemer/c-utils

typedef enum {
	PD_State_Disabled,
	PD_State_HardReset,
	PD_State_Reset,
	PD_State_Idle,
	PD_State_Rx,
	PD_State_AwaitGoodCRC,
	PD_State_Resend,
} PD_State_t;

//static uint8_t * regPtr( FUSB302_D_Register_t reg );

static bool read( FUSB302_D_Register_t reg, uint8_t * value );
static bool write( FUSB302_D_Register_t reg, uint8_t value );

static bool readAll( void );
static bool readStatus( void );
static bool configure( void );

static void typeC_core( void );

static bool readCCVoltageLevel( uint8_t * bc_lvl );
static CCHandshake_CC_t detectCCPinSink( void );
static bool enableSink( CCHandshake_CC_t cc );
static bool disableSink();


static void pd_core( void );

static void pd_init( void );
static void pd_deinit( void );

static bool pd_hasMessage( void );
static bool pd_getMessage( PD_Message_t * message );
static bool pd_sendMessage( PD_Message_t * message, PD_State_t (*onAcknowledged)( PD_Message_t * message) );

static PD_State_t pd_processMessage( PD_Message_t * message );
static PD_State_t pd_onSourceCapabilities( PD_Message_t * message );

static void pd_createRequest( PD_Message_t * message );


static void pd_hardreset( void );
static void pd_reset( void );

//static void pd_setRoles( CCHandshake_PD_Role_t powerRole, CCHandshake_PD_Role_t dataRole )
//static void pd_setAutoGoodCrc( bool enabled );

static void pd_flushRxFifo( void );
static void pd_flushTxFifo( void );

static void pd_startTx( void );



static FUSB302_D_Registers_st Registers;

static volatile CCHandshake_CC_t ConnectedCC;

static struct {
	volatile PD_State_t State;
	struct {
		uint8_t MessageId;
		PD_Message_t Message;
		bool HasData;
	} Rx;
	struct {
		uint8_t MessageId;
		PD_Message_t Message;
		PD_State_t (*OnAcknowledged)( PD_Message_t * message );
		TimerTime_t SentTs;
		uint8_t SendAttempts;
	} Tx;
	struct {
		uint8_t NSourceCapabilities;
		PD_DataObject_t SourceCapabilities[PD_MESSAGE_MAX_OBJECTS];

		uint8_t BestCapIndex;
//		PD_DataObject_t BestCap;
	} Power;

} PD;


static inline void pd_clearTx( void )
{
	memset( (uint8_t*)&PD.Tx.Message, 0, sizeof(PD_Message_t) );
}

static inline uint8_t pd_nextTxMessageId( void )
{
//	uint8_t mid = PD.Rx.MessageId + 1;
	uint8_t mid = PD.Tx.MessageId;

	PD.Tx.MessageId = (PD.Tx.MessageId + 1) % PD_MESSAGE_MAX_MID;

	return mid;
}

#endif

void CCHandshake_init( void )
{
	FUSB302_D_Init( &Driver, HW_I2C_Handle(), I2CX_IRQn, FUSB302_D_DEFAULT_ADDRESS );

	if (FUSB302_D_Probe( &Driver, 3, 100 ) == FUSB302_D_ERROR )
	{
		Error_Handler( ErrorCodeCCHandshakeFail );
	}

#if ONSEMI_LIBRARY==true

	//    InitializeBoard();
	//    InitializeDelay();

	InitializeCoreTimer();

	core_initialize();

	DBG( "CC pid=%d v=%d rev=%d\n", Registers.DeviceID.PRODUCT_ID, Registers.DeviceID.VERSION_ID, Registers.DeviceID.REVISION_ID);

	core_enable_typec( TRUE );  // Enable the state machine by default

//	SetStateUnattached();

#else
	ConnectedCC = CCHandshake_CC_None;

//	read( FUSB302_D_Register_Reset,  );
//	Registers.Reset |= FUSB302_D_Reset_SW_RES;
	write( FUSB302_D_Register_Reset, FUSB302_D_Reset_SW_RES );
//	Registers.Reset &= ~FUSB302_D_Reset_SW_RES;

	if (readAll() == false)
	{
		Error_Handler( ErrorCodeCCHandshakeFail );
	}
	if (configure() == false)
	{
		Error_Handler( ErrorCodeCCHandshakeFail );
	}

	PD.State = PD_State_Disabled;

#endif
}

void CCHandshake_deinit( void )
{
#if ONSEMI_LIBRARY==false
	ConnectedCC = CCHandshake_CC_None;
	PD.State = PD_State_Disabled;
#endif
}

CCHandshake_CC_t CCHandshake_getOrientation( void )
{
#if ONSEMI_LIBRARY==true
	return CCHandshake_CC_None;
#else
	return ConnectedCC;
#endif
}

void CCHandshake_core( void )
{
#if ONSEMI_LIBRARY==true
	core_state_machine();
#else
#if CCHANDSHAKE_AUTONOMOUS==true

#else

//	while(1){

	// read all essential registers
	readStatus();

//	if ( Registers.Interrupt != 0 ){
//		DBG("Interrupt  %02x\n", Registers.Interrupt);
//	}
//	if ( Registers.Interrupta != 0 ){
//		DBG("Interrupt  %02x\n", Registers.Interrupta);
//	}
//	if ( Registers.Interruptb != 0 ){
//		DBG("Interrupt  %02x\n", Registers.Interruptb);
//	}

	typeC_core();

	pd_core();

//	}
#endif
#endif /* ONSEMI_LIBRARY */
}

#if ONSEMI_LIBRARY==true

FSC_BOOL platform_get_device_irq_state( void )
{
	uint8_t reg = 0;

	DeviceRead(regInterrupta, 2, &Registers.Status.InterruptAdv);
	DeviceRead(regInterrupt, 1, &Registers.Status.Interrupt1);

	if (Registers.Status.InterruptAdv != 0 || Registers.Status.Interrupt1 != 0)
	{
//		DBG("interruptAdv %04X \t interrupt1 %04X\n", Registers.Status.InterruptAdv, Registers.Status.Interrupt1 );
		if (Registers.Status.I_OCP_TEMP){ DBG("I_OCP_TEMP " ); }
		if (Registers.Status.I_TOGDONE){ DBG("I_TOGDONE " ); }
		if (Registers.Status.I_SOFTFAIL){ DBG("I_SOFTFAIL " ); }
		if (Registers.Status.I_RETRYFAIL){ DBG("I_RETRYFAIL " ); }
		if (Registers.Status.I_HARDSENT){ DBG("I_HARDSENT " ); }
		if (Registers.Status.I_TXSENT){ DBG("I_TXSENT " ); }
		if (Registers.Status.I_SOFTRST){ DBG("I_SOFTRST " ); }
		if (Registers.Status.I_HARDRST){ DBG("I_HARDRST " ); }

		if (Registers.Status.I_GCRCSENT){ DBG("I_GCRCSENT " ); }

		if (Registers.Status.I_VBUSOK){ DBG("I_VBUSOK " ); }
		if (Registers.Status.I_ACTIVITY){ DBG("I_ACTIVITY " ); }
		if (Registers.Status.I_COMP_CHNG){ DBG("I_COMP_CHNG " ); }
		if (Registers.Status.I_CRC_CHK){ DBG("I_CRC_CHK " ); }
		if (Registers.Status.I_ALERT){ DBG("I_ALERT " ); }
		if (Registers.Status.I_WAKE){ DBG("I_WAKE " ); }
		if (Registers.Status.I_COLLISION){ DBG("I_COLLISION " ); }
		if (Registers.Status.I_BC_LVL){ DBG("I_BC_LVL " ); }

		DBG("\n");
	}


	return (Registers.Status.Interrupt1 != 0 || Registers.Status.InterruptAdv != 0);
//
//	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interrupta, &reg) == FUSB302_D_OK)
//	{
//		if (reg != interrupta)
//		{
//			interrupta = reg;
//			return TRUE;
//		}
//	}
//
//	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interruptb, &reg) == FUSB302_D_OK)
//	{
//		if (reg != interruptb)
//		{
//			interruptb = reg;
//			return TRUE;
//		}
//	}
//
//	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Status0, &reg) == FUSB302_D_OK)
//	{
//		if (reg != status0)
//		{
//			status0 = reg;
//			return TRUE;
//		}
//	}
//
//	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Status1, &reg) == FUSB302_D_OK)
//	{
//		if (reg != status1)
//		{
//			status1 = reg;
//			return TRUE;
//		}
//	}
//
//	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interrupt, &reg) == FUSB302_D_OK)
//	{
//		if (reg != interrupt)
//		{
//			interrupt = reg;
//			return TRUE;
//		}
//	}
}

#else


static void typeC_core( void )
{
	// if not connected check if there is one
	if (ConnectedCC == CCHandshake_CC_None)
	{
		CCHandshake_CC_t detected = detectCCPinSink();

		// if none detected, just quit
		if (detected == CCHandshake_CC_None)
		{
			return;
		}

//		DBG("typeC Switches1 %02x\n", Registers.Switches1 );

		if (enableSink( detected ) == false)
		{
			return;
		}

//		DBG("typeC Switches1 %02x\n", Registers.Switches1 );
		pd_init();

		ConnectedCC = detected;

//		DBG("typeC Switches1 %02x\n", Registers.Switches1 );
		DBG("CC detected %d\n", detected);
	}
	else // check if it's still connected
	{
//		if (read( FUSB302_D_Register_Interrupt ) == false)
//		{
//			return;
//		}
		if ( (Registers.Interrupt & FUSB302_D_Interrupt_I_BC_LVL) != FUSB302_D_Interrupt_I_BC_LVL )
		{
			return;
		}

		uint8_t bc_lvl = 0;
//		DBG("typeC Switches1 %02x\n", Registers.Switches1 );

		if (readCCVoltageLevel( &bc_lvl ) == false)
		{
			return;
		}

//		DBG("typeC Switches1 %02x\n", Registers.Switches1 );
		// above threshold?
		if (bc_lvl >= CCHANDSHAKE_REQUIRE_BC_LVL)
		{
			return; // then we're still good
		}

		if ( disableSink() == false )
		{

		}

		pd_deinit();

		ConnectedCC = CCHandshake_CC_None;

		DBG("CC lost\n");
	}
}


//inline static uint8_t * regPtr( FUSB302_D_Register_t reg )
//{
//	switch (reg){
//		case FUSB302_D_Register_DeviceID: 	return &Registers.DeviceID;
//		case FUSB302_D_Register_Switches0: 	return &Registers.Switches0;
//		case FUSB302_D_Register_Switches1: 	return &Registers.Switches1;
//		case FUSB302_D_Register_Measure: 	return &Registers.Measure;
//		case FUSB302_D_Register_Slice: 		return &Registers.Slice;
//		case FUSB302_D_Register_Control0: 	return &Registers.Control0;
//		case FUSB302_D_Register_Control1: 	return &Registers.Control1;
//		case FUSB302_D_Register_Control2: 	return &Registers.Control2;
//		case FUSB302_D_Register_Control3: 	return &Registers.Control3;
//		case FUSB302_D_Register_Mask1: 		return &Registers.Mask1;
//		case FUSB302_D_Register_Power: 		return &Registers.Power;
//		case FUSB302_D_Register_Reset: 		return &Registers.Reset;
//		case FUSB302_D_Register_OCPreg: 	return &Registers.OCPreg;
//		case FUSB302_D_Register_Maska: 		return &Registers.Maska;
//		case FUSB302_D_Register_Maskb: 		return &Registers.Maskb;
//		case FUSB302_D_Register_Control4: 	return &Registers.Control4;
//		case FUSB302_D_Register_Status0a: 	return &Registers.Status0a;
//		case FUSB302_D_Register_Status1a: 	return &Registers.Status1a;
//		case FUSB302_D_Register_Interrupta: return &Registers.Interrupta;
//		case FUSB302_D_Register_Interruptb: return &Registers.Interruptb;
//		case FUSB302_D_Register_Status0: 	return &Registers.Status0;
//		case FUSB302_D_Register_Status1: 	return &Registers.Status1;
//		case FUSB302_D_Register_Interrupt: 	return &Registers.Interrupt;
//		case FUSB302_D_Register_FIFOs: 		return &Registers.FIFOs;
//
//		default: return NULL;
//	}
//}

static bool read( FUSB302_D_Register_t reg, uint8_t * value )
{
//	uint8_t * r = regPtr( reg );
//
//	if (r == NULL)
//	{
//		return false;
//	}

	uint8_t r[2] = {0,0};

	if (FUSB302_D_Read( &Driver, reg, &r[0] ) == FUSB302_D_ERROR)
	{
		return false;
	}

	*value = r[0];

	return true;
}

static bool write( FUSB302_D_Register_t reg, uint8_t value )
{
//	uint8_t * r = regPtr( reg );
//
//	if (r == NULL)
//	{
//		return false;
//	}

	if (FUSB302_D_Write( &Driver, reg, value ) == FUSB302_D_ERROR)
	{
		return false;
	}

	return true;
}

static bool readAll( void )
{
	if (read(FUSB302_D_Register_DeviceID, &Registers.DeviceID) == false) return false;
	if (read(FUSB302_D_Register_Switches0, &Registers.Switches0 ) == false) return false;
	if (read(FUSB302_D_Register_Switches1, &Registers.Switches1) == false) return false;
	if (read(FUSB302_D_Register_Measure, &Registers.Measure ) == false) return false;
	if (read(FUSB302_D_Register_Slice, &Registers.Slice) == false) return false;
	if (read(FUSB302_D_Register_Control0, &Registers.Control0) == false) return false;
	if (read(FUSB302_D_Register_Control1, &Registers.Control1) == false) return false;
	if (read(FUSB302_D_Register_Control2, &Registers.Control2 ) == false) return false;
	if (read(FUSB302_D_Register_Control3, &Registers.Control3) == false) return false;
	if (read(FUSB302_D_Register_Mask1, &Registers.Mask1) == false) return false;
	if (read(FUSB302_D_Register_Power, &Registers.Power) == false) return false;
//	if (read(FUSB302_D_Register_Reset, &Registers.Reset) == false) return false; // no point in reading this
	if (read(FUSB302_D_Register_OCPreg, &Registers.OCPreg) == false) return false;
	if (read(FUSB302_D_Register_Maska, &Registers.Maska) == false) return false;
	if (read(FUSB302_D_Register_Maskb, &Registers.Maskb) == false) return false;
	if (read(FUSB302_D_Register_Control4, &Registers.Control4) == false) return false;
	if (read(FUSB302_D_Register_Status0a, &Registers.Status0a) == false) return false;
	if (read(FUSB302_D_Register_Status1a, &Registers.Status1a) == false) return false;
	if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false) return false;
	if (read(FUSB302_D_Register_Status1, &Registers.Status1) == false) return false;
	if (read(FUSB302_D_Register_FIFOs, &Registers.FIFOs) == false) return false;
	return true;
}

static bool readStatus( void )
{
	uint8_t buf[5] = {0,0,0,0,0};


	if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_Interrupta, &buf[0], 5 ) == FUSB302_D_ERROR)
	{
		DBG("failed read\n");
		return false;
	}
	Registers.Interrupta = buf[0];
	Registers.Interruptb = buf[1];
	Registers.Status0 = buf[2];
	Registers.Status1 = buf[3];
	Registers.Interrupt = buf[4];

//	if (read(FUSB302_D_Register_Interrupta, &Registers.Interrupta) == false) return false;
//	if (read(FUSB302_D_Register_Interruptb, &Registers.Interruptb) == false) return false;
//	if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false) return false;
//	if (read(FUSB302_D_Register_Status1, &Registers.Status1) == false) return false;
//	if (read(FUSB302_D_Register_Interrupt, &Registers.Interrupt) == false) return false;

	return true;
}

static bool configure( void )
{
#if CCHANDSHAKE_AUTONOMOUS==true

#else

	// enable all power except internal oscillator
	Registers.Power = FUSB302_D_Power_PWR_MASK;// & ~FUSB302_D_Power_PWR_InternalOscillator;
	if (write( FUSB302_D_Register_Power, Registers.Power ) == false) return false;

	// enable high current mode
	// if we were using the interrupt pin, also set FUSB302_D_Control0_INT_MASK (don't forget to optionally set TOG_RD_ONLY)
//	Registers.Control0 = (Registers.Control0 & ~FUSB302_D_Control0_HOST_CUR_MASK) | FUSB302_D_Control0_HOST_CUR_HighCurrentMode;

	//	Registers.Control0 &= ~FUSB302_D_Control0_AUTO_PRE; // is 0 by default
//	if (write( FUSB302_D_Register_Control0, Registers.Control0 ) == false) return false;

	// ON SEMI also sets TOC_USRC_EXIT of "undocumented control 4"

	// enable sink polling (NOTE, we're not using it right now, disabled by default)
//	read( FUSB302_D_Register_Control2 );
//	Registers.Control2 = FUSB302_D_Control2_MODE_SnkPolling | FUSB302_D_Control2_TOGGLE;
//	if (write( FUSB302_D_Register_Control2 );

	Registers.Switches1 = FUSB302_D_Switches1_POWERROLE_Sink | FUSB302_D_Switches1_DATAROLE_Sink | FUSB302_D_Switches1_SPECREV_Rev2_0 | FUSB302_D_Switches1_AUTO_CRC;
	if (write( FUSB302_D_Register_Switches1, Registers.Switches1 ) == false) return false;
//	DBG("configure Switches1 %02x\n", Registers.Switches1 );

	Registers.Control3 |= FUSB302_D_Control3_AUTO_HARDRESET | FUSB302_D_Control3_AUTO_SOFTRESET | FUSB302_D_Control3_AUTO_RETRY | (0xFF & FUSB302_D_Control3_N_RETRIES_MASK);
	if (write( FUSB302_D_Register_Control3, Registers.Control3 ) == false) return false;

	// disable all interrupts
	Registers.Mask1 = FUSB302_D_Mask1_ALL;
	if (write( FUSB302_D_Register_Mask1, Registers.Mask1 ) == false) return false;
	Registers.Maska = FUSB302_D_Maska_ALL;
	if (write( FUSB302_D_Register_Maska, Registers.Maska ) == false) return false;
	Registers.Maskb = FUSB302_D_Maskb_ALL;
	if (write( FUSB302_D_Register_Maskb, Registers.Maskb ) == false) return false;

//	DBG("configure Switches1 %02x\n", Registers.Switches1 );
#endif

	return true;
}


/**
 * Tries to get fresh value of measurement (the currently selected cc pin)
 */
static bool readCCVoltageLevel( uint8_t * bc_lvl )
{
	if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false)
	{
		return false;
	}

	*bc_lvl = Registers.Status0 & FUSB302_D_Status0_BC_LVL_MASK;

	return true;
}

/**
 * Tries to detect which cc pin has voltage
 * WARNING: resets Switches0 register
 */
static CCHandshake_CC_t detectCCPinSink( void )
{
	uint8_t BC_LVL;

//	read( FUSB302_D_Register_Switches0 );

	// enable measurement on cc1
	Registers.Switches0 = (Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC1;// | FUSB302_D_Switches0_PDWN2 | FUSB302_D_Switches0_PDWN1;
	write( FUSB302_D_Register_Switches0, Registers.Switches0 );

	// wait a bit
	DelayMs(250);

	// get measurement
	if (readCCVoltageLevel( &BC_LVL ) == false)
	{
		return CCHandshake_CC_None;
	}
	if ( BC_LVL > FUSB302_D_Status0_BC_LVL_LessThan200mV )
	{
		return CCHandshake_CC_1;
	}

	// enable measurement on cc2
	Registers.Switches0 = (Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC2;// | FUSB302_D_Switches0_PDWN2 | FUSB302_D_Switches0_PDWN1;
	write( FUSB302_D_Register_Switches0, Registers.Switches0 );

	// wait a bit
	DelayMs(250);

	// get measurement
	if (readCCVoltageLevel( &BC_LVL ) == false)
	{
		return CCHandshake_CC_None;
	}
	if ( BC_LVL > FUSB302_D_Status0_BC_LVL_LessThan200mV )
	{
		return CCHandshake_CC_2;
	}

	return CCHandshake_CC_None;
}

static bool enableSink( CCHandshake_CC_t cc )
{
	if (cc == CCHandshake_CC_None)
	{
		DBG("Trying to enable none!");
		return false;
	}

//	DBG("enableSink Switches1 %02x\n", Registers.Switches1 );
//	read( FUSB302_D_Register_Switches1 );
//	read( FUSB302_D_Register_Power );

//	DBG("switches1 %02x\n", Registers.Switches1 );

	// enable meas and txcc
	if (cc == CCHandshake_CC_1)
	{
		Registers.Switches0 = ((Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC1);
		Registers.Switches1 = (Registers.Switches1 & ~FUSB302_D_Switches1_TXCC_MASK) | FUSB302_D_Switches1_TXCC1;
	}
	else if (cc == CCHandshake_CC_2)
	{
		Registers.Switches0 = ((Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC2);
		Registers.Switches1 = (Registers.Switches1 & ~(FUSB302_D_Switches1_TXCC_MASK)) | FUSB302_D_Switches1_TXCC2;
	}

	if (write( FUSB302_D_Register_Switches0, Registers.Switches0 ) == false) return false;
	if (write( FUSB302_D_Register_Switches1, Registers.Switches1 ) == false) return false;

//	read( FUSB302_D_Register_Switches1, &Registers.Switches1 );
//	DBG("enableSink Switches1 %02x\n", Registers.Switches1 );

	// enable oscillator for PD
//	if ( (Registers.Power & FUSB302_D_Power_PWR_InternalOscillator) != FUSB302_D_Power_PWR_InternalOscillator )
//	{
//		DBG("activating internal oscillator\n");
//		Registers.Power |= FUSB302_D_Power_PWR_InternalOscillator;
//		if (write( FUSB302_D_Register_Power, Registers.Power ) == false) return false;
//	}


//	Registers.Switches
	// TXCCx = 1
	// MEAS_CCx = 1
	// TXCCy = 0
	// MEAS_CCy = 0

//	Registers.Switches1 // by default we don't have to set this
	// POWERROLE = 0
	// DATAROLE = 0

// Registers.Control
	// ENSOP1 = 0
	// ENSOP1DP = 0
	// ENSOP2 = 0
	// ENSOP2DP = 0

	// Registers.Power enable internal oscillator

	return true;
}

static bool disableSink()
{
//	DBG("disableSink()\n");

//	read( FUSB302_D_Register_Switches1 );
//	read( FUSB302_D_Register_Power );

//	DBG("disableSink Switches1 %02x\n", Registers.Switches1 );

	// disable TXCCx
	Registers.Switches1 &= ~FUSB302_D_Switches1_TXCC_MASK;
	write( FUSB302_D_Register_Switches1, Registers.Switches1 );

	// disable CC
	Registers.Switches0 &= ~FUSB302_D_Switches0_MEAS_CC_MASK;
	write( FUSB302_D_Register_Switches0, Registers.Switches0 );

//	DBG("disableSink Switches1 %02x\n", Registers.Switches1 );
	// disable oscillator for PD
//	Registers.Power &= ~FUSB302_D_Power_PWR_InternalOscillator;

	return true;
}


static void pd_core( void )
{
	if (PD.State == PD_State_Disabled)
	{
//		DBG("PD State Disabled\n");
		return;
	}

//	static bool first = true;
//	if (first){
//		DBG("FIRST\n");
//		first = false;
//	}

	if ( (Registers.Status1 & FUSB302_D_Status1_RX_EMPTY ) == FUSB302_D_Status1_RX_EMPTY){
//		DBG("RX_EMPTY\n");
//		PD.Rx.HasData |= PD.Rx.HasData;
//		PD.State = PD_State_Rx;
	}
	if ( (Registers.Status1 & FUSB302_D_Status1_RX_FULL ) == FUSB302_D_Status1_RX_FULL){
		DBG("RX_FULL\n");
	}
	if ( (Registers.Status1 & FUSB302_D_Status1_TX_EMPTY ) == FUSB302_D_Status1_TX_EMPTY){
//		DBG("TX_EMPTY\n");
	}
	if ( (Registers.Status1 & FUSB302_D_Status1_TX_FULL ) == FUSB302_D_Status1_TX_FULL){
		DBG("TX_FULL\n");
	}

	if ( (Registers.Interrupt & FUSB302_D_Interrupt_I_COLLISION) == FUSB302_D_Interrupt_I_COLLISION )
	{
		DBG("I_COLLISION\n");
	}
	if ( (Registers.Interrupt & FUSB302_D_Interrupt_I_ACTIVITY) == FUSB302_D_Interrupt_I_ACTIVITY )
	{
//		DBG("I_ACTIVITY\n");
	}
	if ( (Registers.Interrupt & FUSB302_D_Interrupt_I_ALERT) == FUSB302_D_Interrupt_I_ALERT )
	{
		DBG("I_ALERT\n");
	}

	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_SOFTFAIL ) == FUSB302_D_Interrupta_I_SOFTFAIL){
		DBG("I_SOFTFAIL\n");
	}
	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_RETRYFAIL ) == FUSB302_D_Interrupta_I_RETRYFAIL){
		DBG("I_RETRYFAIL\n");
	}
	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_HARDSENT ) == FUSB302_D_Interrupta_I_HARDSENT){
		DBG("I_HARDSENT\n");
	}
	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_TXSENT ) == FUSB302_D_Interrupta_I_TXSENT){
		DBG("I_TXSENT\n");
	}
	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_SOFTRST ) == FUSB302_D_Interrupta_I_SOFTRST){
		DBG("I_SOFTRST\n");
	}
	if ( (Registers.Interrupta & FUSB302_D_Interrupta_I_HARDRST ) == FUSB302_D_Interrupta_I_HARDRST){
		DBG("I_HARDRST\n");
//		PD.State = PD_State_Reset;
	}

	if ( (Registers.Interruptb & FUSB302_D_Interruptb_I_GCRCSENT ) == FUSB302_D_Interruptb_I_GCRCSENT){
		DBG("I_GCRCSENT\n");
		PD.State = PD_State_Rx;
	}



	PD_DataObject_t request = { .Value = 0 };

	switch( PD.State )
	{
		// this is only here to suppress the compiler warning
		case PD_State_Disabled:
		{
			// do nothing
			break;
		}

		case PD_State_HardReset:
		{
			DBG("PD State HardReset\n");
			pd_hardreset();

			DelayMs(1);

			PD.State = PD_State_Reset;
			break;
		}

		case PD_State_Reset:
		{
			DBG("PD State Reset\n");

			pd_reset();

			pd_flushRxFifo();
			pd_flushTxFifo();

			// try to get source capabilities
			pd_clearTx();

			PD_newMessage( &PD.Tx.Message, 0, pd_nextTxMessageId(), PD_HeaderWord_PowerRole_Sink, PD_HeaderWord_SpecRev_2_0, PD_HeaderWord_DataRole_Sink, PD_ControlCommand_GetSourceCap, NULL );

			pd_sendMessage( &PD.Tx.Message, NULL );

			PD.Tx.SendAttempts = 0;
			break;
		}

		case PD_State_Idle:
		{
//			DBG("PD State Idle\n");

			// if neither empty nor full there is data pending
			if ((Registers.Status1 & FUSB302_D_Status1_TX_EMPTY ) != FUSB302_D_Status1_TX_EMPTY &&
					(Registers.Status1 & FUSB302_D_Status1_TX_FULL ) != FUSB302_D_Status1_TX_FULL )
//					(Registers.Interrupt & FUSB302_D_Interrupt_I_ACTIVITY) != FUSB302_D_Interrupt_I_ACTIVITY)
			{
//				DBG("Resending\n");

//				pd_startTx();

//				pd_flushTxFifo();
//				pd_sendMessage( &PD.Tx.Message, NULL );
			}

			break;
		}

		case PD_State_Rx:
		{
//			DBG("PD State Rx\n");

//			if (pd_hasMessage() == false)
//			{
//				PD.State = PD_State_Idle;
//				return;
//			}
//			DBG("PD has message\n");

//			memset( &PD.Rx.Message, 0, sizeof(PD_Message_t) );

			if (pd_getMessage( &PD.Rx.Message ) == false)
			{
				PD.State = PD_State_Idle;
				return;
			}

//			DBG("PD process\n");
			PD.State = pd_processMessage( &PD.Rx.Message );

			break;
		}

//		case PD_State_AwaitGoodCRC:
//		{
////			DBG("PD State AwaitGoodCRC\n");
//
//			if (TimerGetElapsedTime(PD.Tx.SentTs) > 1000){
//				if (PD.Tx.SendAttempts > 3)
//				{
//					PD.State = PD_State_HardReset;
//				}
//				else
//				{
//					PD.State = PD_State_Resend;
//				}
//				break;
//			}
//
//			if ( (Registers.Interruptb & FUSB302_D_Interruptb_I_GCRCSENT ) == FUSB302_D_Interruptb_I_GCRCSENT){
//				PD.State = PD_State_Idle;
//				break;
//			}
////
////			if (pd_hasMessage() == false)
////			{
////				return;
////			}
////
////			memset( &PD.Rx.Message, 0, sizeof(PD_Message_t) );
////
////			if (pd_getMessage( &PD.Rx.Message ) == false)
////			{
////				return;
////			}
//
//			break;
//		}
//
//		case PD_State_Resend:
//		{
//			DBG("PD State Resend\n");
//
//			pd_sendMessage( &PD.Tx.Message, PD.Tx.OnAcknowledged );
//			PD.Tx.SentTs = TimerGetCurrentTime();
//			PD.Tx.SendAttempts++;
//
//			PD.State = PD_State_AwaitGoodCRC;
//
//			break;
//		}

		default:
			PD.State = PD_State_Reset;
	}
}



static void pd_init( void )
{
//	DBG("pd_init Switches1 %02x\n", Registers.Switches1 );

	PD.Rx.HasData = false;

	PD.Tx.MessageId = 2;
	PD.State = PD_State_Idle;

	PD.Power.NSourceCapabilities = 0;
	memset( &PD.Power.SourceCapabilities[0], 0, PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t) );
	PD.Power.BestCapIndex = 0;


	// enable auto goodCRC
//	DBG("Switches1 %d\n", Registers.Switches1 );
//	read( FUSB302_D_Register_Switches1 );
//	DBG("Switches1 %d\n", Registers.Switches1 );
//	Registers.Switches1 = FUSB302_D_Switches1_SPECREV_Rev2_0 | FUSB302_D_Switches1_AUTO_CRC;
//	Registers.Switches1 = (Registers.Switches1 & ~FUSB302_D_Switches1_SPECREV_MASK) | FUSB302_D_Switches1_SPECREV_Rev2_0;
//	Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;

	// enable meas and txcc
//	if (ConnectedCC == CCHandshake_CC_1)
//	{
////		Registers.Switches0 = (Registers.Switches0 & ~)
//		Registers.Switches1 |= FUSB302_D_Switches1_TXCC1;
//	}
//	else if (ConnectedCC == CCHandshake_CC_2)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_TXCC2;
//
//	}

//	DBG("Switches1 %d\n", Registers.Switches1 );
//	write( FUSB302_D_Register_Switches1 );
//
//	DBG("Switches1 %d\n", Registers.Switches1 );
//
//	read( FUSB302_D_Register_Switches1 );
//	DBG("Switches1 %d\n", Registers.Switches1 );

//	read( FUSB302_D_Register_Control0, Registers.Control0 );
//	Registers.Control0 &= ~FUSB302_D_Control0_AUTO_PRE;
//	write( FUSB302_D_Register_Control0, Registers.Control0 );


	// enable interrupts
//	read( FUSB302_D_Register_Maska );
//	Registers.Maska |= FUSB302_D_Maska_M_TXSENT;
//	write( FUSB302_D_Register_Maska, Registers.Maska );

//	read( FUSB302_D_Register_Maskb );
//	Registers.Maskb |= FUSB302_D_Maskb_M_GCRCSENT;
//	write( FUSB302_D_Register_Maskb, Registers.Maskb );

//	pd_reset();
//	pd_flushRxFifo();
//	pd_flushTxFifo();

//	DBG("pd_init Switches1 %02x\n", Registers.Switches1 );
}

static void pd_deinit( void )
{
//	DBG("pd_deinit Switches1 %02x\n", Registers.Switches1 );

	PD.State = PD_State_Disabled;

	PD.Tx.MessageId = 2;

	PD.Power.NSourceCapabilities = 0;

	pd_reset();
	pd_flushRxFifo();
	pd_flushTxFifo();

	// disable auto goodCRC
//	read( FUSB302_D_Register_Switches1 );
//	Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;
//	write( FUSB302_D_Register_Switches1, Registers.Switches1 );

	// disable interrupts
//	read( FUSB302_D_Register_Maska );
	Registers.Maska &= ~FUSB302_D_Maska_M_TXSENT;
	write( FUSB302_D_Register_Maska, Registers.Maska );

//	read( FUSB302_D_Register_Maskb );
	Registers.Maskb &= ~FUSB302_D_Maskb_M_GCRCSENT;
	write( FUSB302_D_Register_Maskb, Registers.Maskb );

//	DBG("pd_deinit Switches1 %02x\n", Registers.Switches1 );
}

static void pd_hardreset( void )
{
	PD.Tx.MessageId = 2;

//	read(FUSB302_D_Register_Control3);
	Registers.Control3 |= FUSB302_D_Control3_SEND_HARD_RESET;
	write(FUSB302_D_Register_Control3, Registers.Control3);
	Registers.Control3 &= ~FUSB302_D_Control3_SEND_HARD_RESET;
}

static void pd_reset( void )
{
//	PD.Tx.MessageId = 0;

//	read(FUSB302_D_Register_Reset);

	Registers.Reset |= FUSB302_D_Reset_PD_RESET;
	write(FUSB302_D_Register_Reset, Registers.Reset);
	Registers.Reset &= ~FUSB302_D_Reset_PD_RESET;
}

static void pd_flushRxFifo( void )
{
//	read( FUSB302_D_Register_Control1 );

	Registers.Control1 |= FUSB302_D_Control1_RX_FLUSH;

	write( FUSB302_D_Register_Control1, Registers.Control1 );

	// clear bit again
	Registers.Control1 &= ~FUSB302_D_Control1_RX_FLUSH;
}

static void pd_flushTxFifo( void )
{
//	read( FUSB302_D_Register_Control0 );

	Registers.Control0 |= FUSB302_D_Control0_TX_FLUSH;

	write( FUSB302_D_Register_Control0, Registers.Control0 );

	// clear bit again
	Registers.Control0 &= ~FUSB302_D_Control0_TX_FLUSH;
}

static bool pd_hasMessage( void )
{
//	read( FUSB302_D_Register_Status1, &Registers.Status1 );

	return (Registers.Status1 & FUSB302_D_Status1_RX_EMPTY) != FUSB302_D_Status1_RX_EMPTY;
}

static void pd_startTx( void )
{
//	read( FUSB302_D_Register_Control0 );
	Registers.Control0 |= FUSB302_D_Control0_TX_START;
	write( FUSB302_D_Register_Control0, Registers.Control0 );
	Registers.Control0 &= ~FUSB302_D_Control0_TX_START;
}
//static void pd_setRoles( CCHandshake_PD_Role_t powerRole, CCHandshake_PD_Role_t dataRole )
//{
//	read( FUSB302_D_Register_Switches1 );
//
//	// clear bits
//	Registers.Switches1 &= ~(FUSB302_D_Switches1_POWERROLE | FUSB302_D_Switches1_DATAROLE);
//
//	if (powerRole == CCHandshake_PD_Role_Source)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_POWERROLE_Source;
//	}
//	else
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_POWERROLE_Sink;
//	}
//
//	if (dataRole == CCHandshake_PD_Role_Source)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_DATAROLE_Source;
//	}
//	else
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_DATAROLE_Sink;
//	}
//
//	write( FUSB302_D_Register_Switches1 );
//}
//
//static void pd_setAutoGoodCrc( bool enabled )
//{
//	read( FUSB302_D_Register_Switches1 );
//
//	Registers.Switches1 &= ~FUSB302_D_Switches1_AUTO_CRC;
//
//	if (enabled)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;
//	}
//
//	write( FUSB302_D_Register_Switches1 );
//}


static bool pd_getMessage( PD_Message_t * message )
{
	uint8_t token;

	do {

		token = 0;

		if (FUSB302_D_Read( &Driver, FUSB302_D_Register_FIFOs, &token ) == FUSB302_D_ERROR)
		{
			DBG("failed read 1\n");
			return false;
		}

//		DBG(" %02x \n", addr);

		// discard and abort if not right type
		if ( (token & FUSB302_D_RxFIFOToken_MASK) != FUSB302_D_RxFIFOToken_SOP)
		{
			DBG("%02x", token);
//			return false;
			read( FUSB302_D_Register_Status1, &Registers.Status1 );
			if ((Registers.Status1 & FUSB302_D_Status1_RX_EMPTY) == FUSB302_D_Status1_RX_EMPTY)
			{
				DBG("\nnothing left\n");
				return false;
			}
		}
		else
		{
//			DBG("- %02x\n", token);
			break;
		}

//		read( FUSB302_D_Register_Status1 );

	} while (1);

	uint8_t header[2] = {0,0};

	if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_FIFOs, &header[0], 2 ) == FUSB302_D_ERROR)
	{
		DBG("failed read 2\n");
		return false;
	}

	le_to_u16( &message->Header.Word, header );

	PD.Rx.MessageId = PD_HeaderWord_getMessageId( message->Header.Word );
//	PD.Tx.MessageId = PD.Rx.MessageId + 1;

//	DBG("RX tkn %02x hdr %02x%02x\n", token, message->Header.Bytes[0], message->Header.Bytes[1] );

	uint8_t N = PD_HeaderWord_getNumberOfDataObjects( message->Header.Word );

	if (N > 0)
	{
		uint8_t buf[PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t)];
		memset( buf, 0, PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t) );

		if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_FIFOs, &buf[0], sizeof(PD_DataObject_t) * N ) == FUSB302_D_ERROR)
//		if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_FIFOs, &message->DataObjects[0], sizeof(PD_DataObject_t) * N ) == FUSB302_D_ERROR)
		{
			DBG("failed read 3\n");
			return false;
		}

		for(uint8_t n = 0; n < N; n++)
		{
//			message->DataObjects[n] = buf[n*sizeof(PD_DataObject_t)];
			le_to_u32( &message->DataObjects[n], &buf[n*sizeof(PD_DataObject_t)] );

//			DBG("%08x vs %02x%02x%02x%02x\n",
//					message->DataObjects[n],
//					buf[n*sizeof(PD_DataObject_t)],
//					buf[n*sizeof(PD_DataObject_t)+1],
//					buf[n*sizeof(PD_DataObject_t)+2],
//					buf[n*sizeof(PD_DataObject_t)+3]);
		}


	}


	uint8_t crc32[4] = {0,0,0,0};

	if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_FIFOs, &crc32[0], 4 ) == FUSB302_D_ERROR)
	{
		DBG("failed read 4\n");
		return false;
	}

	le_to_u32( &message->Crc32, crc32 );

	return true;
}


static bool pd_sendMessage( PD_Message_t * message, PD_State_t (*onAcknowledged)( PD_Message_t * message) )
{
	uint8_t N = PD_HeaderWord_getNumberOfDataObjects( message->Header.Word );

	DBG( "---- PD tx\n");

	DBG("mid = %d prole = %d spec = %d drole = %d cmd = %d\n" ,
			PD_HeaderWord_getMessageId(message->Header.Word),
			PD_HeaderWord_getPowerRole(message->Header.Word),
			PD_HeaderWord_getSpecRev(message->Header.Word),
			PD_HeaderWord_getDataRole(message->Header.Word),
			PD_HeaderWord_getCommandCode(message->Header.Word)
			);

	DBG("data (%d) ", N);
	for ( uint8_t n = 0; n < N; n++)
	{
		DBG("%08x", message->DataObjects[n].Value);
	}

	DBG( "\n----\n" );

	PD.Tx.OnAcknowledged = onAcknowledged;

	uint8_t buf[64];

	memset( (uint8_t*)&buf[0], 0, sizeof(buf) );

	uint8_t i = 0;

	// I don't get it, but the arduino code does it like this
	buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
	buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
	buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
	buf[i++] = FUSB302_D_TxFIFOToken_SOP2;


	// set packet size
	buf[i++] = FUSB302_D_TxFIFOToken_PACKSYM | (2 + N * sizeof(PD_DataObject_t));

//	DBG("%d\n", (2 + N * sizeof(PD_DataObject_t)));

	// immediately followed up by data

	// add header
	u16_to_le( &buf[i], message->Header.Word );
	i += 2;

	for (uint8_t n = 0; n < N; n++)
	{
//		u32_to_bigendian( &buf[i], &message->DataObjects[n] );
		u32_to_le( &buf[i], message->DataObjects[n].Value );
		i += sizeof(PD_DataObject_t);
	}

	// just some
	buf[i++] = FUSB302_D_TxFIFOToken_JAM_CRC;
	buf[i++] = FUSB302_D_TxFIFOToken_EOP;
	buf[i++] = FUSB302_D_TxFIFOToken_TXOFF;

	// command to actually start TX
	buf[i++] = FUSB302_D_TxFIFOToken_TXON;

	if (FUSB302_D_WriteN( &Driver, FUSB302_D_Register_FIFOs, &buf[0], i ) == FUSB302_D_ERROR)
	{
		DBG("WRite FAIL\n");
		return false;
	}


	DBG("Tx (%d)\n", i);

//	pd_startTx();

	PD.Tx.SentTs = TimerGetCurrentTime();
	PD.State = PD_State_Idle;
//	PD.State = PD_State_AwaitGoodCRC;

	return true;
}

static PD_State_t pd_processMessage( PD_Message_t * message )
{
	uint8_t N = PD_HeaderWord_getNumberOfDataObjects( message->Header.Word );

	DBG( "---- PD rx\n");

	DBG("mid = %d prole = %d spec = %d drole = %d cmd = %d\n" ,
			PD_HeaderWord_getMessageId(message->Header.Word),
			PD_HeaderWord_getPowerRole(message->Header.Word),
			PD_HeaderWord_getSpecRev(message->Header.Word),
			PD_HeaderWord_getDataRole(message->Header.Word),
			PD_HeaderWord_getCommandCode(message->Header.Word)
			);

	DBG("data (%d) ", N);
	for ( uint8_t n = 0; n < N; n++)
	{
		DBG("%08x", message->DataObjects[n].Value);
//		DBG("%08x", message->DataObjects[n].object );
	}

	DBG( "\n----\n" );


	if (PD_isControlMessage( message ))
	{

		switch(PD_HeaderWord_getCommandCode(message->Header.Word))
		{
			case PD_ControlCommand_GoodCRC:
			{
				if (PD.Tx.OnAcknowledged != NULL){
					return PD.Tx.OnAcknowledged( message );
				}
				break;
			}

			case PD_ControlCommand_Accept:
			{
				DBG("Accept!\n");
				break;
			}

			case PD_ControlCommand_Reject:
			{
				DBG("Reject!\n");
				break;
			}

			case PD_ControlCommand_SoftReset:
			{
				return PD_State_Reset;
			}

			default:
				DBG("Ignoring control %d\n", PD_HeaderWord_getCommandCode(message->Header.Word));
		}
	}
	else
	{
		switch(PD_HeaderWord_getCommandCode(message->Header.Word))
		{
			case PD_DataCommand_SourceCapabilities:
			{
//				PD_newMessage( &PD.Tx.Message, 0, pd_nextTxMessageId(), )
				return pd_onSourceCapabilities( message );
//				DBG("sourceCaps\n");
//				break;
			}
			default:
				DBG("Ignoring data %d\n", PD_HeaderWord_getCommandCode(message->Header.Word));
		}
	}

	return PD_State_Idle;
}

static PD_State_t pd_onSourceCapabilities( PD_Message_t * message )
{
//	DBG("sourceCaps\n");

	uint8_t N = PD_HeaderWord_getNumberOfDataObjects( message->Header.Word );

	// a source capabilities message must always have
	if (N == 0)
	{
		DBG("no data\n");
		return PD_State_Reset;
	}

	PD.Power.NSourceCapabilities = N;
	memcpy( &PD.Power.SourceCapabilities[0], &message->DataObjects[0], N * sizeof(PD_DataObject_t) );

	// if there is only one offer, then that is the default 5V, no need for a request or anything else, because there is nothing else we can do
	if (N == 1)
	{
		return PD_State_Idle;
	}


	// find best offer

//	uint8_t best = 0;
//	PD.Power.BestCap.Value = PD.Power.SourceCapabilities[0].Value;

	PD.Power.BestCapIndex = 0; // note: 0 is an invalid value (according to PD spec)

	uint32_t caps;
	uint32_t bestPower = 0;
	uint32_t v,p,c;

	for (uint8_t i = 0; i < N; i++)
	{
		// criteria for better
//		switch (PD.Power.SourceCapabilities[i].PDO.SupplyType)
		caps = PD.Power.SourceCapabilities[i].Value;
		switch (caps & PDO_SrcCap_SupplyType_MASK)
		{
			case PDO_SrcCap_SupplyType_Battery:
			{
				DBG("%d battery  %d mV - %d mV  max %d mW\n", i,
						50 * PDO_SrcCap_Battery_getMinVoltage_50mV(caps),
						50 * PDO_SrcCap_Battery_getMaxVoltage_50mV(caps),
						250 * PDO_SrcCap_Battery_getMaxPower_250mW(caps) );
//				DBG("%d battery %d mW %d mV - %d mV\n", i, 250 * PD.Power.SourceCapabilities[i].BPDO.MaxPower, 50 * PD.Power.SourceCapabilities[i].BPDO.MinVoltage, 50 * PD.Power.SourceCapabilities[i].BPDO.MaxVoltage );
				break;
			}
			case PDO_SrcCap_SupplyType_Fixed:
			{
				v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);
				c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps);

				DBG("%d fixed  %d mV max %d mA\n", i, 50 * v, 10 * c );

				p = v * c;
				if ( (50 * v <= PD_REQUEST_MAX_MILLIVOLT) && p > bestPower )
				{
					PD.Power.BestCapIndex = i + 1; // Note: it's count starting by 1
//					PD.Power.Requested.Value = PD.Power.SourceCapabilities[i].Value;

//					bestVolt = v;
//					bestCur = c;
					bestPower = p;
				}

				break;
			}
			case PDO_SrcCap_SupplyType_Variable:
			{
				DBG("%d variable  %d mV - %d mV  max %d mA\n", i,
						50 * PDO_SrcCap_Variable_getMinVoltage_50mV(caps),
						50 * PDO_SrcCap_Variable_getMaxVoltage_50mV(caps),
						10 * PDO_SrcCap_Variable_getMaxCurrent_10mA(caps) );
				break;
			}
//			case PD_SupplyType_Reserved:
//			{
//				// ignore
//				break;
//			}
		}


	}

	// abort if we did not get a valid capability
	if (PD.Power.BestCapIndex == 0)
	{
		return PD_State_Idle;
	}


	pd_createRequest( &PD.Tx.Message );

//	DelayMs(1000);
	pd_sendMessage( &PD.Tx.Message, NULL );

	PD.Tx.SendAttempts = 0;

//	return PD_State_AwaitGoodCRC;

	return PD_State_Idle;
}

static void pd_createRequest( PD_Message_t * message )
{

	// abort if we did not get a valid capability
	if (PD.Power.BestCapIndex == 0)
	{
		return;
	}


	uint32_t caps;
	uint32_t bestPower = 0;
	uint32_t v,p,c;

	// make request

	PD_DataObject_t request = { .Value = 0 };

	caps = PD.Power.SourceCapabilities[ PD.Power.BestCapIndex - 1 ].Value;

	switch( caps & PDO_SrcCap_SupplyType_MASK )
	{
		case PDO_SrcCap_SupplyType_Fixed:
		{

			v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);
			c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps);

			if ( 10 * c > PD_REQUEST_MAX_MILLIAMP )
			{
				c = PD_REQUEST_MAX_MILLIAMP / 10;
			}

			request.Value = PDO_Req_Fixed_NoUSBSuspend;
			request.Value |= PDO_Req_Fixed_setObjectPosBits( PD.Power.BestCapIndex );
			request.Value |= PDO_Req_Fixed_setOperatingCurrent_10mABits( c );
			request.Value |= PDO_Req_Fixed_setMaxOpCur_10mABits( c );
			break;
		}
	}


	PD_newMessage( message, 1, pd_nextTxMessageId(), PD_HeaderWord_PowerRole_Sink, PD_HeaderWord_SpecRev_2_0, PD_HeaderWord_DataRole_Sink, PD_DataCommand_Request, &request );
}

#endif
