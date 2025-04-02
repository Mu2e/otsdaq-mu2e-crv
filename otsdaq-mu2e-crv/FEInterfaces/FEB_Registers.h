#ifndef FEB_REGISTERS_H
#define FEB_REGISTERS_H

#include <cstdint>
// #include <functional>  // std::bind, std::function (if needed)
#include <cstdint>  // uint16_t

namespace FEB
{
enum Register : uint16_t
{
	// FPGA registers
	CR           = 0x1000,
	WrPtrHi      = 0x1002,
	WrPtrLo      = 0x1003,
	RdPtrHi      = 0x1004,
	RdPtrLo      = 0x1005,
	BiasTrim     = 0x1030,  // to 0x3f
	Threshold    = 0x1090,  // to 0x9f
	Bias         = 0x1044,  // to 0x45
	uBHi         = 0x1064,
	uBLo         = 0x1065,
	uBBuffHi     = 0x1066,
	uBBuffLo     = 0x1067,
	DebugVersion = 0x1076,
	onSpillCnt   = 0x1078,
	offSpillCnt  = 0x1079,
	Pedestal     = 0x1080,  // to 88
	HistInterval = 0x1011,
	HistRun      = 0x1010,
	HistPointer  = 0x1014,  // to 0x15
	HistMemory   = 0x1016,  // to 0x17
	InputMaskReg = 0x1021,
	
	// registers that effect all FPGAs
	Port         = 0x1314,
	Pipeline     = 0x1304,
	OnSpillGate  = 0x1305,
	OffSpillGate = 0x1306,
	Samples      = 0x130C,
	IntTrgEn     = 0x130E,

	// prefix for all 4 fpga
	AllFPGA      = 0x1300,
	CSRBroadCast = 0x1316,
	// broadcast to all FEBs on ROC
	AllFEB = 0x1000, // TODO! 0x3000 doesn't seem to work. Needs to be fixed.
	// uC functions
	Reset  = 0x9001,
	TRIG   = 0x900B,
	CMBENA = 0x9106

	// Switch FGPAs on/off
	
};  // end ROC_Register enum

uint16_t FPGA[] = {0x000, 0x400, 0x800, 0xC00};

}  // namespace FEB

#endif  // FEB_REGISTERS_H
