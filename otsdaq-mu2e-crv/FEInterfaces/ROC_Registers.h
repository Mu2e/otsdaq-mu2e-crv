#ifndef ROC_REGISTERS_H
#define ROC_REGISTERS_H

// #include <functional>  // std::bind, std::function (if needed)

namespace ROC
{
enum Register : uint16_t
{
	// FPGA1 registers
	CR              = 0x00,  // bit 5:
	GTP_CRC         = 0x02,
	LinkWdCnt0      = 0x04,
	LinkWdCnt1      = 0x05,
	LinkWdCnt2      = 0x06,
	EvBuffStat      = 0x07,
	ActivePortsHigh = 0x08,
	ActivePortsLow  = 0x09,
	ID              = 0x0a,
	// TRigReqWdUsed      = 0x0e, // same as DreqBuffStat
	PLLMuxHigh        = 0x17,
	PLLMuxHLow        = 0x18,
	PLLStat           = 0x19,  // bit 0 is pwr dwn (1 is powered down), bit 4 is lock
	GTPRxRead         = 0x20,  // GTP0 input trace
	LinkFifoRead      = 0x24,  // 0x24-0x26, link FIFO data: reading REMOVES a word
	CRS               = 0x27,  // link FIFOs: bits 6:4 full, 2:0 empty
	TestCounter       = 0x35,
	DreqBuffStat      = 0x3C,
	HrtBtBuffStat     = 0x3D,
	MarkerCnt         = 0x41,  // not used
	HeartBeat         = 0x42,  // markers sent out by FPGA 1
	LastEventLength   = 0x43,
	InjectionTS       = 0x44,
	Clk80MHz          = 0x45,  // enable the 80MHz clock alignment
	LoopbackMode      = 0x47,
	LoopbackMarkerCnt = 0x4A,
	HeartBeatCn       = 0x4F,  // EWT from fibers, bad name!
	DcsBufferWdCnt    = 0x51,
	sendGR            = 0x58,
	InjectionCnt      = 0x59,
	InjectionLength   = 0x5A,
	DRTimeout         = 0x5D,
	UpTimeHigh        = 0x6C,
	UpTimeLow         = 0x6D,
	LastUbSent        = 0x70,
	MarkerDelay       = 0x78,
	LinkErrors        = 0x80,
	uBOffset          = 0x81,
	DRCntHigh         = 0x82,
	DRCnLow           = 0x83,
	GTPTxRead         = 0x85,
	DReqBuffTrace     = 0x86,  // trace of the data request words given to the uC
	EvBuildUbDebug0   = 0x88,  // 0x88-0x8E: uB of link 2/1/0 in the last built event,
	                           // mismatch flag EWT match, FPGA1 version 0xa7 and later
	EwtReqDropped = 0x8F,      // requests dropped (no event built)
	EwtEvDropped  = 0x90,      // events dropped (request gone)
	EwtPending    = 0x91,      // 15:8 built events waiting, 7:0 pending EWT words
	EwtState      = 0x92,  // 11:8 builder, 7 former idle, 6:4 former status, 3:0 handler
	EwtCtrl = 0x93,  // bit 0 drop late, 1 bypass matching, 2 input FIFO flush (a8), 3
	                 // bad-tag guard (a8); a8 default 0x000D
	DRTimeoutCnt       = 0x94,
	EwtLastDropEvent   = 0x9A,  // event tag (low 16 bits) at the last drop
	EwtLastDropRequest = 0x9B,  // requested EWT (low 16 bits) at the last drop
	DReqRxCnt          = 0x9C,  // data requests received from the DTC
	EvBuiltCnt         = 0x9D,  // events built
	                    // FPGA1 version 0xa8 and later (counters stop at 0xFFFF, cleared
	                    // at run start)
	LinkFifoRstCnt = 0x9E,  // resets of the FPGA1 input FIFOs, any cause
	BadTagCnt      = 0x9F,  // events newer than the only pending request
	StaleDataCnt   = 0xA0,  // requests that found leftover data in the input FIFOs
	TruncEvCnt     = 0xA1,  // events cut at the size limit
	FlushSnapStat  = 0xA2,  // snapshot at the last bad tag / stale data: 0 bad tag, 1
	                        // stale data, 2 flush enabled, 7:4 builder state, 10:8 input
	                        // FIFO empty flags, 15:12 request words
	FlushSnapHead0 = 0xA3,  // 0xA3-0xA5: first word of input FIFO 0, 1, 2 in the snapshot
	FlushSnapWords0 = 0xA6,  // 0xA6-0xA8: words in input FIFO 0, 1, 2 in the snapshot
	GitHashHigh     = 0x96,
	GitHashLow      = 0x97,
	Version         = 0x99,

	// FPGA2 (data-FPGAs) registers
	Data_Broadcast      = 0x300,
	Data_CRC            = 0x00,
	Data_LinkCtrl       = 0x01,
	Data_DDR_WriteHigh  = 0x02,
	Data_DDR_WriteLow   = 0x03,
	Data_DDR_ReadHigh   = 0x04,
	Data_DDR_ReadLow    = 0x05,
	Data_DDRStat        = 0x08,  // MIG FIFO flags, bit 1 calibration done
	Data_DDRCount       = 0x09,
	Data_DDRFsmStat     = 0x0C,  // bits 15:12 DDR write state, 2:0 DDR read state
	Data_RxErr          = 0x14,
	Data_RxCRS          = 0x15,
	Data_InputMask      = 0x17,
	Data_PhyRxWdUsed    = 0x18,  // 0x18-0x1F, words in the input buffer of each port
	Data_PhyRxRead      = 0x20,  // 0x20-0x27, input buffer data: reading REMOVES a word
	Data_RxActive       = 0x2F,  // ports the data FPGA waits for
	Data_TestCounter    = 0x44,
	Data_DDRRdPtrHigh   = 0x46,
	Data_DDRRdPtrLow    = 0x47,
	Data_CRCErr         = 0x60,  // sticky CRC error per port
	Data_UpTimeHigh     = 0x6C,
	Data_UpTimeLow      = 0x6D,
	Data_PhyActivityCnt = 0x70,  // 0x70-0x77, words received per port
	Data_TxOverflowCnt  = 0x80,  // events truncated to the maximum size
	                             // Port match ("one event part per request, in order"),
	                             // FPGA2 0x0012 and later. The enable bit and the timeout
	                             // are cleared by the uC reset and must be written again.
	Data_PortMatchCtrl = 0x81,   // W [0] on, [15] clear 0x83-0x8B. R [0] on, [3:2] state,
	                             // [6:4] open requests, [15:8] ports with a header
	Data_PortTimeout = 0x82,     // wait for a FEB answer, units of 10 us, 0 = no timeout
	Data_PMLastTime =
	    0x83,  // request to event, last, units of 10 us (also counted with the match off)
	Data_PMMaxTime     = 0x84,  // request to event, maximum
	Data_PMEvCnt       = 0x85,  // events built (counters stop at 0xFFFF)
	Data_PMReqCnt      = 0x86,  // requests seen
	Data_PMDrainCnt    = 0x87,  // port buffers emptied
	Data_PMMissCnt     = 0x88,  // events with missing ports
	Data_PMTimeoutCnt  = 0x89,  // events built by the timeout
	Data_PMListDropCnt = 0x8A,  // requests dropped from a full list
	Data_PMPorts = 0x8B,       // [7:0] missing ports of the last such event, [15:8] ports
	                           // emptied since the last clear
	Data_PMBadEwtHigh = 0x8C,  // EWT of the last header that was not in the list
	Data_PMBadEwtLow  = 0x8D,
	Data_PMBadPort = 0x8E,  // [10:8] port of that header, [7:0] ports being emptied now
	Data_PMOldestHigh = 0x8F,  // oldest open request
	Data_PMOldestLow  = 0x90,
	Data_PMTxWord0 =
	    0x91,  // first word of the last packet loaded for the FEBs (0x0182 = one request)
	Data_Version = 0x99,

	// FEB registers
	FEB           = 0x1000,
	FEB_Broadcast = 0x3000,  // superseeds FEB

	// uC functions
	LP      = 0x8000,
	Reset   = 0x8001,
	PWRRST  = 0x800A,
	TRIG    = 0x800B,
	POOLENA = 0x8107,

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
	POOLPARA_Bias          = 0x000b,  // up to 0x0012
	POOLPARA_BiasADC =
	    0x0013,  // 0x00014, +n*6: (0x00019, 0x0001a),
	             // POOLPARA_BiasBus       = 0x0015, // 0x00016, +n*6: (0x0001b, 0x0001c),
	POOLPARA_CMB_Temp  = 0x0015,  // 0x00018, +n*6: (0x0001b, 0x0001e),
	POOLPARA_TrigCntrl = 0x002b,
	POOLPARA_Pipeline  = 0x002c,
	POOLPARA_Sample    = 0x002d,
	POOLPARA_RESERVED  = 0x002e,  // to 0x3f
};                                // end ROC_Register enum

uint16_t Data[] = {0x400, 0x800, 0xC00};

}  // namespace ROC

#endif  // ROC_REGISTERS_H
