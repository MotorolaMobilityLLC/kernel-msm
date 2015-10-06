#ifndef __FSC_TYPEC_TYPES_H__
#define __FSC_TYPEC_TYPES_H__

typedef enum {
	USBTypeC_Sink = 0,
	USBTypeC_Source,
	USBTypeC_DRP
} USBTypeCPort;

typedef enum {
	Disabled = 0,
	ErrorRecovery,
	Unattached,
	AttachWaitSink,
	AttachedSink,
	AttachWaitSource,
	AttachedSource,
	TrySource,
	TryWaitSink,
	TrySink,
	TryWaitSource,
	AudioAccessory,
	DebugAccessory,
	AttachWaitAccessory,
	PoweredAccessory,
	UnsupportedAccessory,
	DelayUnattached,
	UnattachedSource
} ConnectionState;

typedef enum {
	CCTypeOpen = 0,
	CCTypeRa,
	CCTypeRdUSB,
	CCTypeRd1p5,
	CCTypeRd3p0,
	CCTypeUndefined
} CCTermType;

typedef enum {
	TypeCPin_None = 0,
	TypeCPin_GND1,
	TypeCPin_TXp1,
	TypeCPin_TXn1,
	TypeCPin_VBUS1,
	TypeCPin_CC1,
	TypeCPin_Dp1,
	TypeCPin_Dn1,
	TypeCPin_SBU1,
	TypeCPin_VBUS2,
	TypeCPin_RXn2,
	TypeCPin_RXp2,
	TypeCPin_GND2,
	TypeCPin_GND3,
	TypeCPin_TXp2,
	TypeCPin_TXn2,
	TypeCPin_VBUS3,
	TypeCPin_CC2,
	TypeCPin_Dp2,
	TypeCPin_Dn2,
	TypeCPin_SBU2,
	TypeCPin_VBUS4,
	TypeCPin_RXn1,
	TypeCPin_RXp1,
	TypeCPin_GND4
} TypeCPins_t;

typedef enum {
	utccNone = 0,
	utccDefault,
	utcc1p5A,
	utcc3p0A
} USBTypeCCurrent;

#endif // __FSC_TYPEC_TYPES_H__
