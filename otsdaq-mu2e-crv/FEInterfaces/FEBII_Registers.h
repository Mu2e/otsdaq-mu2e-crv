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
	CR               = 0x1000,
    // bit 8 = clear DDR FIFO nearly full warning
	// bit 7 = clear encoded FM clock parity error
	// bit 6 = hard reset AFE chips
	// bit 5 = general soft reset for FPGA logic (does not stop clocks)
	// bit 2 = reset AFE front end (force front end recalibration and realignment)
	// bit 1 = AFE1 power down
	// bit 0 = AFE0 power down
    Status           = 0x1001,
    FirmwareLO       = 0x1002,
    FirmwareHI       = 0x1003,
    OutFIFI          = 0x100c,
    OutFIFOStatus    = 0x1017,
    CntHI            = 0x1022,
    CntLO            = 0x1023,
    ThresholdGlobal  = 0x1026,
    UptimeHI         = 0x106C,
    UptimeLo         = 0x106D,
    EWT              = 0x106E,
    EWTCount         = 0x106F,
    ThresholdBase    = 0x1070, // to 0x107F
    ChannelMapBase   = 0x1080, // to 0x108F
    // implemented in uC
    TrimBase         = 0x1090, // to 0x1090
    LEDBias          = 0x10a0, // to 0x10a3
    BiasBase         = 0x10a4, // to 0x10a5
    VGABase          = 0x10a6, // to 0x10a7

	// registers that effect all FPGAs
    FlashGateEn     = 0x1300,
    FlashGateOn     = 0x1301, // 6.25ns, default 1
    FlashGateOff    = 0x1302, // 6.25ns, default 112
    EWTFakeMode     = 0x1303, 
    GateOnOnSpill   = 0x1305, // 6.25ns, default 16
    GateOffOnSpill  = 0x1306, // 6.25ns, default 255
    GateOnOffSpill  = 0x1307, // 6.25ns, default 16
    GateOffOffSpill = 0x1308, // 6.25ns, default 1792
    LEDOn           = 0x1318,  // 6.25ns, default 
	//Port         = 0x1314,
	//Pipeline     = 0x1304,
	//OnSpillGate  = 0x1305,
	//OffSpillGate = 0x1306,
	//Samples      = 0x130C,
	//IntTrgEn     = 0x130E,

	// prefix for all 4 fpga
	//AllFPGA      = 0x1300,
	//CSRBroadCast = 0x1316,
	// broadcast to all FEBs on ROC
	AllFEB = 0x3000,
	// uC functions
	Reset  = 0x9001,
	TRIG   = 0x900B,
	CMBENA = 0x9106
};  // end ROC_Register enum

uint16_t FPGA[] = {0x000, 0x400, 0x800, 0xC00};

}  // namespace FEB

#endif  // FEBII_REGISTERS_H
