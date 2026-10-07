#ifndef FEBII_REGISTERS_H
#define FEBII_REGISTERS_H

// #include <functional>  // std::bind, std::function (if needed)
#include <cstdint>  // uint16_t

// Source: https://github.com/Mu2e/cRV_FEB2

namespace FEBII
{
enum Register : uint16_t
{
	// FPGA registers
	CR = 0x1000,
	// bit 8 = clear DDR FIFO nearly full warning
	// bit 7 = clear encoded FM clock parity error
	// bit 6 = hard reset AFE chips
	// bit 5 = general soft reset for FPGA logic (does not stop clocks)
	// bit 2 = reset AFE front end (force front end recalibration and realignment)
	// bit 1 = AFE1 power down
	// bit 0 = AFE0 power down
	Status          = 0x1001,
	FirmwareLO      = 0x1002,
	FirmwareHI      = 0x1003,
	OutFIFI         = 0x100c,
	OutFIFOStatus   = 0x1017,
	CntHI           = 0x1022,
	CntLO           = 0x1023,
	ThresholdGlobal = 0x1026,
	Port            = 0x1029,
	UptimeHI        = 0x106C,
	UptimeLo        = 0x106D,
	EWT             = 0x106E,
	EWTCount        = 0x106F,
	ThresholdBase   = 0x1070,  // to 0x107F
	ChannelMapBase  = 0x1080,  // to 0x108F
	                           // implemented in uC
	BaselineBase = 0x1090,
	TrimBase     = 0x10b0,  // to 0x1090
	LEDBias      = 0x10a0,  // to 0x10a3
	BiasBase     = 0x10a4,  // to 0x10a5
	VGABase      = 0x10a6,  // to 0x10a7
	AlignScore   = 0x106a,
	DDRStatus    = 0x1025,  // DDR memory controller status (per-FPGA): bit 15 init OK, 14 almost full,
	                        // 13 full; temperature in 11..0 as raw XADC (degC = raw*0.123-273) on
	                        // FEB2 firmware before 2026-09-18, integer degC in 7..0 afterwards
	ErrorCounts  = 0x1711,  // MDIO error counts (read resets counter)
	// Error counter reset moved from FPGA register 0x1712 to the uC function
	// MDIO (0x9109) with ROC firmware >= 637, see FEBII::MDIO below.

	// AFE reads
	AFE0_base = 0x1100,
	AFE1_base = 0x1200,

	// registers that effect all FPGAs
	FlashGateEn     = 0x1300,
	FlashGateOn     = 0x1301,  // 6.25ns, default 1
	FlashGateOff    = 0x1302,  // 6.25ns, default 112
	EWTFakeMode     = 0x1303,
	GateOnOnSpill   = 0x1305,  // 6.25ns, default 16
	GateOffOnSpill  = 0x1306,  // 6.25ns, default 255
	GateOnOffSpill  = 0x1307,  // 6.25ns, default 16
	GateOffOffSpill = 0x1308,  // 6.25ns, default 1792
	LEDOn           = 0x1318,  // 6.25ns, default
	PortAll         = 0x1329,  // set port for all FPGAs

	// broadcast to all FEBs on ROC
	AllFEB = 0x3000,
	// uC functions: 0x9xxx = "LC <cmd> <value>" to the selected FEB,
	//               0xBxxx = "LCB <cmd> <value>" broadcast to all FEBs on the ROC
	Reset         = 0x9001,
	TRIG          = 0x900B,
	MUX           = 0x9103,
	GAIN          = 0x9104,
	CMBENA        = 0x9106,
	MDIO          = 0x9109,  // write MDIOClearErrorCounters to clear error counters in FRAM
	MDIOBroadcast = 0xB109   // same, all FEBs on the ROC in one write
};  // end ROC_Register enum

// Value written to FEBII::MDIO to clear the FEB error counters.
constexpr uint16_t MDIOClearErrorCounters = 5;

enum AFERegister : uint16_t
{
	// AFE registers, write only (read is a multi step process)
	Offset_en  = 0x3,  // bit 8
	Offset_ch1 = 0x0D,
	Offset_ch2 = 0x0F,
	Offset_ch3 = 0x11,
	Offset_ch4 = 0x13,
	Offset_ch5 = 0x1F,
	Offset_ch6 = 0x1D,
	Offset_ch7 = 0x1B,
	Offset_ch8 = 0x19,
};

uint16_t FPGA[] = {0x000, 0x400, 0x800, 0xC00};

}  // namespace FEBII

#endif  // FEBII_REGISTERS_H
