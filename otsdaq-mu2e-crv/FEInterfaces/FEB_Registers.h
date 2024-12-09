#ifndef FEB_REGISTERS_H
#define FEB_REGISTERS_H

// #include <functional>  // std::bind, std::function (if needed)

namespace FEB {
enum Register : uint16_t {
    // FPGA registers
    CR                 = 0x1000,
    Port               = 0x1014,
    Pipeline           = 0x1004,
    OnSpillGate        = 0x1005,
    OffSpillGate       = 0x1006,
    RdPtrHi            = 0x1010,
    RdPtrLo            = 0x1011,
    Addres             = 0x1014,
    Samples            = 0x100C,
    IntTrgEn           = 0x100E,
    BiasTrim           = 0x1030, // to 0x3f 
    Threshold          = 0x1090, // to 0x9f
    Bias               = 0x1044, // to 0x45

    HistInterval       = 0x1011,
    HistRun            = 0x1010,
    HistPointer        = 0x1014, // to 0x15
    HistMemory         = 0x1016, // to 0x17

    // prefix for all 4 fpga
    AllFPGA            = 0x1300,
    CSRBroadCast       = 0x1316,
    // broadcast to all FEBs on ROC
    AllFEB             = 0x3000,
    // uC functions
    Reset        = 0x9001,
    TRIG         = 0x900B,
    CMBENA       = 0x9106
}; // end ROC_Register enum

uint16_t FPGA[] = {0x000, 0x400, 0x800, 0xC00}; 



}  // namespace FEB

#endif  // FEB_REGISTERS_H
