/*
 * db.h
 *
 * Created: 09.07.2012 11:30:33
 *  Author: user
 */
#ifndef DB_H_
#define DB_H_

//#pragma pack(1)

// max length of description fields
#define MAX_DESC_LENGTH 50


/*
 *	Files of DB.
 */

// ======================================================================================
//
//				DATA BLOCKS FOR DATA EXCHANGE WITH THE CONFIGURATION TERMINAL
//
// ======================================================================================

/************************************************************************
 * CLASSES
 ************************************************************************/
typedef struct _Classes {
	u32 id;
	char sClassName[MAX_DESC_LENGTH];
} Classes;

/************************************************************************
 * COMPATIBILITY
 ************************************************************************/
typedef struct _Compatibilities {
	u32 id;
	u32 className;
	u32 intName;
} Compatibilities;

/************************************************************************
 * Description
 ************************************************************************/
typedef union _Descriptions {
	struct _d{
		u32 id;
		u32 typeName;
		u32 paramName;
		char sDescription[MAX_DESC_LENGTH];
		char sDefaultValue[MAX_DESC_LENGTH];
		bool bMultiplace;
		bool bRequired;
	} ;
	u8 buf[sizeof(struct _d)];
} Descriptions;

/************************************************************************
 * Interface
 ************************************************************************/
typedef struct _Interfaces {
	u32 id;
	char sIntName[MAX_DESC_LENGTH];
	u32 typeName;
	u32 function;
	bool bLeftsided;
	bool bSingleConnection;
} Interfaces;

/************************************************************************
 * IOs
 ************************************************************************/
typedef struct _IOs {
	u32 id;
	char sFunction[MAX_DESC_LENGTH];
	bool bLeftsided;
} IOs;

/************************************************************************
 * Objects
 ************************************************************************/
typedef struct _Objects {
	u32 id;
	char sObjectName[MAX_DESC_LENGTH];
	u32 typeName;
	u32 uin;
	char sState[MAX_DESC_LENGTH];
	bool bEnabled;
	u32 x;
	u32 y;
	u32 parentId;
} Objects;

/************************************************************************
 * Params
 ************************************************************************/
typedef struct _Params {
	u32 id;
	char sParamName[MAX_DESC_LENGTH];
} Params;

/************************************************************************
 * Pins
 ************************************************************************/
typedef struct _Pins {
	u32 id;
	char sPinName[MAX_DESC_LENGTH];
	u32 object_name;
	u32 intName;
	bool bLeftsided;
} Pins;

/************************************************************************
 * Relations
 ************************************************************************/
typedef struct _Relations {
	u32 id;
	u32 pinNameA;
	u32 pinNameB;
	bool bEnabled;
} Relations;

/************************************************************************
 * Types
 ************************************************************************/
typedef struct _Types {
	u32 id;
	char sTypeName[MAX_DESC_LENGTH];
	u32 class_name;
	bool bVisibility;
} Types;

/************************************************************************
 * Values
 ************************************************************************/
typedef struct _Values {
	u32 id;
	u32 object_name;
	u32 description;
	char sValue[MAX_DESC_LENGTH];
} Values;



// ======================================================================================
//
//				OBJECTS FOR USAGE IN THE RUNTIME ALGORITHM
//
// ======================================================================================

typedef enum _ObjType {
	T_Timer,
	T_Maximum,
	T_Minimum,
	T_Source,
	T_Comparator,
	T_Trigger,
	T_Led,
	T_Alarm,
	T_Buffer,
	T_Converter,
	T_BearingBoard,
	T_Router,
	T_Cpu,
	T_SimCard,
	T_Gprs,
	T_Dtmf,
	T_Vpn,
	T_TcpIpStack,
	T_TcpIpSocket,

	T_Max
} ObjType;


typedef enum _PinType {
	Pin_In,
	Pin_Out,
	Pib_Control,
	Pin_State,
	Pin_Address,

	Pin_Max
} PinType;


typedef enum _ParamType {
	P_WorkTime,
	P_State,
	P_CodeYES,
	P_CodeNO,
	P_RowOfList,
	P_CodeON,
	P_CodeOFF,
	P_CodeSwitchON,
	P_CodeSwitchOFF,
	P_CodeSwitch,
	P_DefaultValue,
	P_CodeMultivibrator,
	P_TimeON,
	P_TimeOFF,
	P_CodeNormal,
	P_CodeEmergency,
	P_CodeReset,
	P_CodeStart,
	P_CodeStop,

	P_Max
} ParamType;


typedef enum _ObjClass {
	C_Motherboard,
	C_Logic,
	C_Router,
	C_GsmModule,
	C_CellularOperator,
	C_GprsBulkhead,
	C_DtmfCodec,
	C_VpnEnvironment,
	C_TcpIpStack,
	C_TcpIpSocket,

	C_Max
} ObjClass;



typedef struct _Object Object;
typedef struct _PinsTbl PinsTbl;

typedef struct _Interface {
	ObjType objType;
	PinType pinType;
	bool bSingleConnection;
} Interface;


typedef struct _Pin {
	Object *pOwnerObj;
	Interface *pInterface;
} Pin;


typedef struct _Relation {
	Pin *pPinA;
	Pin *pPinB;
	bool bEnabled;
} Relation;


typedef struct _Object {
	ObjType type;
	u32 uin;
	bool bState;
	bool bEnabled;
	Object *pParent;

	PinsTbl *pMyPins;	// all pins of the object
} Object;


typedef struct _Description {
	ObjType objType;
	ParamType paramType;
	char defaultValue[MAX_DESC_LENGTH];
	bool bMultiplace;
	bool bRequired;
} Description;


typedef struct _Value {
	Object *pOwnerObj;
	Description *pDeskription;
	char value[MAX_DESC_LENGTH];
} Value;


typedef struct _Compatibility {
	ObjClass objClass;
	Interface *pInterface;
} Compatibility;



// ======================================================================================
//
//				RUNTIME DB TABLES
//
// ======================================================================================

typedef struct _PinsTbl {
	u16 amount;
	Pin *pArray;
} PinsTbl;


typedef struct _InterfacesTbl {
	u16 amount;
	Interface *pArray;
} InterfacesTbl;


typedef struct _RelationsTbl {
	u16 amount;
	Relation *pArray;
} RelationsTbl;


typedef struct _ObjectsTbl {
	u16 amount;
	Object *pArray;
} ObjectsTbl;


typedef struct _ValuesTbl {
	u16 amount;
	Value *pArray;
} ValuesTbl;


typedef struct _CompatibilitiesTbl {
	u16 amount;
	Compatibility *pArray;
} CompatibilitiesTbl;


typedef struct _DescriptionsTbl {
	u16 amount;
	Description *pArray;
} DescriptionsTbl;




#endif /* DB_H_ */
