#ifndef ROC_REGISTERS_H
#define ROC_REGISTERS_H

// #include <functional>  // std::bind, std::function (if needed)

namespace ROC {
enum Register : uint16_t 
{
    // FPGA1 registers
    CR                 = 0x00,
    GTP_CRC            = 0x02,
    ActivePortsHigh    = 0x08,
    ActivePortsLow     = 0x09,
    ID                 = 0x0a,
    PLLMuxHigh         = 0x17,
    PLLMuxHLow         = 0x18,
    PLLStat            = 0x19, // bit 0 is pwr dwn, bit 4 is lock
    GTPRxRead          = 0x20, // GTP0 input trace
    CRS                = 0x27,
    TestCounter        = 0x35,
    MarkerCnt          = 0x41,
    HeartBeat          = 0x42,
    LastEventLength    = 0x43,
    InjectionTS        = 0x44,
    LoopbackMode       = 0x47,
    LoopbackMarkerCnt  = 0x4A,
    sendGR             = 0x58,
    InjectionCnt       = 0x59,
    InjectionLength    = 0x5A,
    UpTimeHigh         = 0x6C,
    UpTimeLow          = 0x6D,
    LinkErrors         = 0x80,
    DRCntHigh          = 0x82,
    DRCnLow            = 0x83,
    GTPTxRead          = 0x85,
    GitHashHigh        = 0x96,
    GitHashLow         = 0x97,
    Version            = 0x99,

    // FPGA2 (data-FPGAs) registers
    Data_Broadcast     = 0x300,
    Data_CRC           = 0x00,
    Data_DDR_WriteHigh = 0x02,
    Data_DDR_WriteLow  = 0x03,
    Data_DDR_ReadHigh  = 0x04,
    Data_DDR_ReadLow   = 0x05,

    // FEB registers
    FEB                = 0x1000,
    FEB_Broadcast      = 0x3000, // superseeds FEB

    // uC functions
    LP           = 0x8000,
    Reset        = 0x8001,
    PWRRST       = 0x800A,
    TRIG         = 0x800B,
    POOLENA      = 0x8107,
}; // end ROC_Register enum

uint16_t Data[] = {0x400, 0x800, 0xC00}; 


}  // namespace ROC

#endif  // ROC_REGISTERS_H