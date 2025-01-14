#ifndef ROC_REGISTERS_H
#define ROC_REGISTERS_H

#include <cstdint>
// #include <functional>  // std::bind, std::function (if needed)

namespace ROC {
enum Register : uint16_t 
{
    // FPGA1 registers
    CR                 = 0x00, // bit 5: 
    GTP_CRC            = 0x02,
    LinkWdCnt0         = 0x04,
    LinkWdCnt1         = 0x05,
    LinkWdCnt2         = 0x06,
    EvBuffStat         = 0x07,
    ActivePortsHigh    = 0x08,
    ActivePortsLow     = 0x09,
    ID                 = 0x0a,
    //TRigReqWdUsed      = 0x0e, // same as DreqBuffStat
    PLLMuxHigh         = 0x17,
    PLLMuxHLow         = 0x18,
    PLLStat            = 0x19, // bit 0 is pwr dwn (1 is powered down), bit 4 is lock
    GTPRxRead          = 0x20, // GTP0 input trace
    CRS                = 0x27,
    TestCounter        = 0x35,
    DreqBuffStat       = 0x3C,
    HrtBtBuffStat      = 0x3D,
    MarkerCnt          = 0x41, // not used
    HeartBeat          = 0x42, // markers sent out by FPGA 1
    LastEventLength    = 0x43,
    InjectionTS        = 0x44,
    Clk80MHz           = 0x45, // enable the 80MHz clock alignment
    LoopbackMode       = 0x47,
    LoopbackMarkerCnt  = 0x4A,
    HeartBeatCn        = 0x4F, // EWT from fibers, bad name!
    sendGR             = 0x58,
    InjectionCnt       = 0x59,
    InjectionLength    = 0x5A,
    DRTimeout          = 0x5D,
    UpTimeHigh         = 0x6C,
    UpTimeLow          = 0x6D,
    LastUbSent         = 0x70,
    MarkerDelay        = 0x78,
    LinkErrors         = 0x80,
    uBOffset           = 0x81,
    DRCntHigh          = 0x82,
    DRCnLow            = 0x83,
    GTPTxRead          = 0x85,
    GitHashHigh        = 0x96,
    GitHashLow         = 0x97,
    Version            = 0x99,

    // FPGA2 (data-FPGAs) registers
    Data_Broadcast     = 0x300,
    Data_CRC           = 0x00,
    Data_LinkCtrl      = 0x01,
    Data_DDR_WriteHigh = 0x02,
    Data_DDR_WriteLow  = 0x03,
    Data_DDR_ReadHigh  = 0x04,
    Data_DDR_ReadLow   = 0x05,
    Data_TestCounter   = 0x44,
    Data_UpTimeHigh    = 0x6C,
    Data_UpTimeLow     = 0x6D,

    // FEB registers
    FEB                = 0x1000,
    FEB_Broadcast      = 0x3000, // superseeds FEB

    // uC functions
    LP           = 0x8000,
    Reset        = 0x8001,
    PWRRST       = 0x800A,
    TRIG         = 0x800B,
    POOLENA      = 0x8107,

    // POOLPARA
    // 0-6: see below
    // 7-10: port number
    POOLPARA               = 0x8800,
    POOLPARA_Serial        = 0x0000,
    POOLPARA_SpillCycleCnt = 0x0001,
    POOLPARA_FebTemp       = 0x0002,
    POOLPARA_1_2V          = 0x0003,
    POOLPARA_1_8V          = 0x0004,
    POOLPARA_5V            = 0x0005,
    POOLPARA_10V           = 0x0006,
    POOLPARA_2_5V          = 0x0007,
    POOLPARA_n5V           = 0x0008,
    POOLPARA_15V           = 0x0009,
    POOLPARA_3_3V          = 0x000a,
    POOLPARA_Bias          = 0x000b, // up to 0x0012
    POOLPARA_BiasADC       = 0x0013, // 0x00014, +n*6: (0x00019, 0x0001a),  
    //POOLPARA_BiasBus       = 0x0015, // 0x00016, +n*6: (0x0001b, 0x0001c),
    POOLPARA_CMB_Temp      = 0x0015, // 0x00018, +n*6: (0x0001b, 0x0001e), 
    POOLPARA_TrigCntrl     = 0x002b,
    POOLPARA_Pipeline      = 0x002c,
    POOLPARA_Sample        = 0x002d,
    POOLPARA_RESERVED      = 0x002e, // to 0x3f
}; // end ROC_Register enum

uint16_t Data[] = {0x400, 0x800, 0xC00}; 


}  // namespace ROC

#endif  // ROC_REGISTERS_H
