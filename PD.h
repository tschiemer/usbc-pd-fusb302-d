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

#ifndef PD_H_
#define PD_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include <assert.h>


#define PD_MESSAGE_MAX_OBJECTS 	7
#define PD_MESSAGE_MAX_MID 		7

#define PD_isValidNumberOfDataObjects(__n__) 	( 0 <= (__n__) && (__n__) <= PD_MESSAGE_MAX_OBJECTS )
#define PD_isValidMessageId(__n__) 				( 0 <= (__n__) && (__n__) <= PD_MESSAGE_MAX_MID )

#define PD_HeaderWord_NumberOfDataObjects_MASK 	0b0111000000000000
#define PD_HeaderWord_MessageId_MASK			0b0000111000000000
#define PD_HeaderWord_PowerRole_MASK			0b0000000100000000
#define PD_HeaderWord_SpecRev_MASK				0b0000000011000000
#define PD_HeaderWord_DataRole_MASK				0b0000000000100000
#define PD_HeaderWord_CommandCode_MASK			0b0000000000001111

#define PD_HeaderWord_NumberOfDataObjects_OFFSET	12
#define PD_HeaderWord_MessageId_OFFSET				9
#define PD_HeaderWord_PowerRole_OFFSET				8
#define PD_HeaderWord_SpecRev_OFFSET				6
#define PD_HeaderWord_DataRole_OFFSET				5
#define PD_HeaderWord_CommandCode_OFFSET			0

#define PD_HeaderWord_PowerRole_Source			0b0000000100000000
#define PD_HeaderWord_PowerRole_Sink			0b0000000000000000
#define PD_HeaderWord_SpecRev_1_0				0b0000000000000000
#define PD_HeaderWord_SpecRev_2_0				0b0000000001000000
#define PD_HeaderWord_DataRole_Source			0b0000000000100000
#define PD_HeaderWord_DataRole_Sink				0b0000000000000000

#define PD_isValidPowerRole( __r__ ) ( (__r__) == PD_HeaderWord_PowerRole_Source || (__r__) == PD_HeaderWord_PowerRole_Sink )
#define PD_isValidSpecRev( __r__ ) ( (__r__) == PD_HeaderWord_SpecRev_1_0 || (__r__) == PD_HeaderWord_SpecRev_2_0 )
#define PD_isValidDataRole( __r__ ) ( (__r__) == PD_HeaderWord_DataRole_Source || (__r__) == PD_HeaderWord_DataRole_Sink )

#define PD_HeaderWord_getNumberOfDataObjects( __h__ )				( ((__h__) & PD_HeaderWord_NumberOfDataObjects_MASK) >> PD_HeaderWord_NumberOfDataObjects_OFFSET )
#define PD_HeaderWord_getMessageId( __h__ )							( ((__h__) & PD_HeaderWord_MessageId_MASK) >> PD_HeaderWord_MessageId_OFFSET )
#define PD_HeaderWord_getPowerRole( __h__ )							( ((__h__) & PD_HeaderWord_PowerRole_MASK) >> PD_HeaderWord_PowerRole_OFFSET )
#define PD_HeaderWord_getSpecRev( __h__ )							( ((__h__) & PD_HeaderWord_SpecRev_MASK) >> PD_HeaderWord_SpecRev_OFFSET )
#define PD_HeaderWord_getDataRole( __h__ )							( ((__h__) & PD_HeaderWord_DataRole_MASK) >> PD_HeaderWord_DataRole_OFFSET )
#define PD_HeaderWord_getCommandCode( __h__ )						( ((__h__) & PD_HeaderWord_CommandCode_MASK) >> PD_HeaderWord_CommandCode_OFFSET )

#define PD_HeaderWord_setNumberOfDataObjectsBits( __v__ ) 			(  ((__v__) << PD_HeaderWord_NumberOfDataObjects_OFFSET ) & PD_HeaderWord_NumberOfDataObjects_MASK )
#define PD_HeaderWord_setMessageIdBits( __v__ ) 					( ((__v__) << PD_HeaderWord_MessageId_OFFSET ) & PD_HeaderWord_MessageId_MASK )
#define PD_HeaderWord_setPowerRoleBits( __v__ ) 					( ((__v__) << PD_HeaderWord_PowerRole_OFFSET ) & PD_HeaderWord_PowerRole_MASK )
#define PD_HeaderWord_setSpecRevBits( __v__ ) 						( ((__v__) << PD_HeaderWord_SpecRev_OFFSET ) & PD_HeaderWord_SpecRev_MASK )
#define PD_HeaderWord_setDataRoleBits( __v__ ) 						( ((__v__) << PD_HeaderWord_DataRole_OFFSET ) & PD_HeaderWord_DataRole_MASK )
#define PD_HeaderWord_setCommandCodeBits( __v__ ) 					( ((__v__) << PD_HeaderWord_CommandCode_OFFSET ) & PD_HeaderWord_CommandCode_MASK )

typedef enum {
	PD_ControlCommand_GoodCRC			= 0b0001, // 1
	PD_ControlCommand_GotoMin			= 0b0010, // 2
	PD_ControlCommand_Accept			= 0b0011, // 3
	PD_ControlCommand_Reject			= 0b0100, // 4
	PD_ControlCommand_Ping				= 0b0101, // 5
	PD_ControlCommand_PSRDY				= 0b0110, // 6
	PD_ControlCommand_GetSourceCap		= 0b0111, // 7
	PD_ControlCommand_GetSinkCap		= 0b1000, // 8
	PD_ControlCommand_DRSwap			= 0b1001, // 9
	PD_ControlCommand_PRSwap			= 0b1010, // 10
	PD_ControlCommand_VCONNSwap			= 0b1011, // 11
	PD_ControlCommand_Wait				= 0b1100, // 12
	PD_ControlCommand_SoftReset			= 0b1101  // 13
} PD_ControlCommand_t;

#define PD_isValidControlCommand( __c__ ) ( \
		(__c__) == PD_ControlCommand_GoodCRC || \
		(__c__) == PD_ControlCommand_GotoMin || \
		(__c__) == PD_ControlCommand_Accept || \
		(__c__) == PD_ControlCommand_Reject || \
		(__c__) == PD_ControlCommand_Ping || \
		(__c__) == PD_ControlCommand_PSRDY || \
		(__c__) == PD_ControlCommand_GetSourceCap || \
		(__c__) == PD_ControlCommand_GetSinkCap || \
		(__c__) == PD_ControlCommand_DRSwap || \
		(__c__) == PD_ControlCommand_PRSwap || \
		(__c__) == PD_ControlCommand_VCONNSwap || \
		(__c__) == PD_ControlCommand_Wait || \
		(__c__) == PD_ControlCommand_SoftReset \
)

typedef enum {
	PD_DataCommand_SourceCapabilities	= 0b0001, // 1
	PD_DataCommand_Request				= 0b0010, // 2
	PD_DataCommand_BIST					= 0b0011, // 3
	PD_DataCommand_SinkCapabilities		= 0b0100, // 4
} PD_DataCommand_t;

#define PD_isValidDataCommand( __c__ ) ( \
		(__c__) == PD_DataCommand_SourceCapabilities || \
		(__c__) == PD_DataCommand_Request || \
		(__c__) == PD_DataCommand_BIST || \
		(__c__) == PD_DataCommand_SinkCapabilities \
)

#define PD_isValidCommand( __c__ ) (PD_isValidControlCommand(__c__) || PD_isValidDataCommand(__c__))

typedef enum {
	PD_SupplyType_Battery 	= 0,
	PD_SupplyType_Fixed		= 1,
	PD_SupplyType_Variable	= 2,
	PD_SupplyType_Reserved 	= 3
} PD_SupplyType_t;

typedef union {
	uint32_t Value;
	uint8_t Bytes[4];
} PD_DataObject_t;

#define PDO_SrcCap_SupplyType_MASK				0b11000000000000000000000000000000
#define PDO_SrcCap_SupplyType_Fixed				0b00000000000000000000000000000000
#define PDO_SrcCap_SupplyType_Battery			0b01000000000000000000000000000000
#define PDO_SrcCap_SupplyType_Variable			0b10000000000000000000000000000000
#define PDO_SrcCap_SupplyType_Reserved			0b11000000000000000000000000000000

#define PDO_SrcCap_Fixed_DualRolePower			0b00100000000000000000000000000000
#define PDO_SrcCap_Fixed_USBSuspendSupport		0b00010000000000000000000000000000
#define PDO_SrcCap_Fixed_UnconstrainedPower		0b00001000000000000000000000000000
#define PDO_SrcCap_Fixed_USBComCapable			0b00000100000000000000000000000000
#define PDO_SrcCap_Fixed_DualRoleData			0b00000010000000000000000000000000
#define PDO_SrcCap_Fixed_Reserved_MASK			0b00000001110000000000000000000000
#define PDO_SrcCap_Fixed_PeakCurrent_MASK		0b00000000001100000000000000000000
#define PDO_SrcCap_Fixed_Voltage_50mV_MASK		0b00000000000011111111110000000000
#define PDO_SrcCap_Fixed_MaxCurrent_10mA_MASK	0b00000000000000000000001111111111

#define PDO_SrcCap_Fixed_PeakCurrent_EqualIoC	0b00000000000000000000000000000000
#define PDO_SrcCap_Fixed_PeakCurrent_Overload1	0b00000000000100000000000000000000
#define PDO_SrcCap_Fixed_PeakCurrent_Overload2	0b00000000001000000000000000000000
#define PDO_SrcCap_Fixed_PeakCurrent_Overload3	0b00000000001100000000000000000000

#define PDO_SrcCap_Fixed_Voltage_50mV_OFFSET	10
#define PDO_SrcCap_Fixed_MaxCurrent_10mA_OFFSET	0

#define PDO_SrcCap_Fixed_getVoltage_50mV( __v__ ) 		( ( (__v__) & PDO_SrcCap_Fixed_Voltage_50mV_MASK ) >> PDO_SrcCap_Fixed_Voltage_50mV_OFFSET )
#define PDO_SrcCap_Fixed_getMaxCurrent_10mA( __v__ ) 	( ( (__v__) & PDO_SrcCap_Fixed_MaxCurrent_10mA_MASK ) >> PDO_SrcCap_Fixed_MaxCurrent_10mA_OFFSET )

#define PDO_SrcCap_Variable_MaxVoltage_50mV_MASK	0b00111111111100000000000000000000
#define PDO_SrcCap_Variable_MinVoltage_50mV_MASK	0b00000000000011111111110000000000
#define PDO_SrcCap_Variable_MaxCurrent_10mA_MASK	0b00000000000000000000001111111111

#define PDO_SrcCap_Variable_MaxVoltage_50mV_OFFSET	20
#define PDO_SrcCap_Variable_MinVoltage_50mV_OFFSET 	10
#define PDO_SrcCap_Variable_MaxCurrent_10mA_OFFSET	0

#define PDO_SrcCap_Variable_getMaxVoltage_50mV( __v__ ) ( ( (__v__) & PDO_SrcCap_Variable_MaxVoltage_50mV_MASK ) >> PDO_SrcCap_Variable_MaxVoltage_50mV_OFFSET )
#define PDO_SrcCap_Variable_getMinVoltage_50mV( __v__ ) ( ( (__v__) & PDO_SrcCap_Variable_MinVoltage_50mV_MASK ) >> PDO_SrcCap_Variable_MinVoltage_50mV_OFFSET )
#define PDO_SrcCap_Variable_getMaxCurrent_10mA( __v__ ) ( (__v__) & PDO_SrcCap_Variable_MaxCurrent_10mA_MASK )

#define PDO_SrcCap_Battery_MaxVoltage_50mV_MASK		0b00111111111100000000000000000000
#define PDO_SrcCap_Battery_MinVoltage_50mV_MASK		0b00000000000011111111110000000000
#define PDO_SrcCap_Battery_MaxPower_250mW_MASK		0b00000000000000000000001111111111

#define PDO_SrcCap_Battery_MaxVoltage_50mV_OFFSET	20
#define PDO_SrcCap_Battery_MinVoltage_50mV_OFFSET 	10
#define PDO_SrcCap_Battery_MaxPower_250mW_OFFSET	0

#define PDO_SrcCap_Battery_getMaxVoltage_50mV( __v__ ) ( ( (__v__) & PDO_SrcCap_Battery_MaxVoltage_50mV_MASK ) >> PDO_SrcCap_Battery_MaxVoltage_50mV_OFFSET )
#define PDO_SrcCap_Battery_getMinVoltage_50mV( __v__ ) ( ( (__v__) & PDO_SrcCap_Battery_MinVoltage_50mV_MASK ) >> PDO_SrcCap_Battery_MinVoltage_50mV_OFFSET )
#define PDO_SrcCap_Battery_getMaxPower_250mW( __v__ ) ( (__v__) & PDO_SrcCap_Battery_MaxPower_250mW_MASK )


#define PDO_Req_Fixed_Reserved_MASK					0b10000000111100000000000000000000
#define PDO_Req_Fixed_ObjectPos_MASK				0b01110000000000000000000000000000
#define PDO_Req_Fixed_GiveBack						0b00001000000000000000000000000000
#define PDO_Req_Fixed_CapabilityMismatch			0b00000100000000000000000000000000
#define PDO_Req_Fixed_USBComCapable					0b00000010000000000000000000000000
#define PDO_Req_Fixed_NoUSBSuspend					0b00000001000000000000000000000000
#define PDO_Req_Fixed_OperatingCurrent_10mA_MASK	0b00000000000011111111110000000000
#define PDO_Req_Fixed_MaxOpCur_10mA_MASK			0b00000000000000000000001111111111

#define PDO_Req_Fixed_ObjectPos_OFFSET				28
#define PDO_Req_Fixed_OperatingCurrent_10mA_OFFSET	10
#define PDO_Req_Fixed_MaxOpCur_10mA_OFFSET			0

#define PDO_Req_Fixed_setObjectPosBits( __v__ ) 			( ( (__v__) << PDO_Req_Fixed_ObjectPos_OFFSET ) & PDO_Req_Fixed_ObjectPos_MASK )
#define PDO_Req_Fixed_setOperatingCurrent_10mABits(__v__)	( ( (__v__) << PDO_Req_Fixed_OperatingCurrent_10mA_OFFSET ) & PDO_Req_Fixed_OperatingCurrent_10mA_MASK )
#define PDO_Req_Fixed_setMaxOpCur_10mABits(__v__)			( (__v__) & PDO_Req_Fixed_MaxOpCur_10mA_MASK )

//__attribute__ ((packed))
typedef struct {
	uint16_t  Address;
	union {
		uint16_t Word;
		uint8_t Bytes[2];
	} Header;
	PD_DataObject_t DataObjects[PD_MESSAGE_MAX_OBJECTS];
	uint32_t Crc32;
}  PD_Message_t;
//__attribute__ ((packed))

#define PD_isControlMessage( __msg__ ) 		( PD_HeaderWord_getNumberOfDataObjects((__msg__)->Header.Word) == 0 )
#define PD_isDataMessage( __msg__ ) 		( PD_HeaderWord_getNumberOfDataObjects((__msg__)->Header.Word) > 0 )


inline void PD_newMessage( PD_Message_t * message, uint8_t numberOfDataObjects, uint8_t messageId, uint16_t powerRole, uint16_t specRev, uint16_t dataRole, uint16_t commandCode, PD_DataObject_t * dataObjects )
{
	assert( PD_isValidNumberOfDataObjects(numberOfDataObjects) );
	assert( PD_isValidMessageId(messageId) );
	assert( PD_isValidPowerRole(powerRole) );
	assert( PD_isValidSpecRev(specRev) );
	assert( PD_isValidDataRole(dataRole) );
	assert( numberOfDataObjects == 0 || dataObjects != NULL );
	assert( numberOfDataObjects == 0 || PD_isValidDataCommand(commandCode) );
	assert( numberOfDataObjects > 0 || PD_isValidControlCommand(commandCode) );

	message->Header.Word = PD_HeaderWord_setNumberOfDataObjectsBits(numberOfDataObjects);
	message->Header.Word |= PD_HeaderWord_setMessageIdBits(numberOfDataObjects);
	message->Header.Word |= powerRole | specRev | dataRole | commandCode;

	for(uint8_t i = 0; i < numberOfDataObjects; i++)
	{
		message->DataObjects[i].Value = dataObjects[i].Value;
//		message->DataObjects[i].byte[0] = dataObjects[i].byte[0];
//		message->DataObjects[i].byte[1] = dataObjects[i].byte[1];
//		message->DataObjects[i].byte[2] = dataObjects[i].byte[2];
//		message->DataObjects[i].byte[3] = dataObjects[i].byte[3];
	}
}


#ifdef __cplusplus
 }
#endif


#endif /* PD_H_ */
