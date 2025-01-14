//-----------------------------------------------------------------------------
// interactive interface for ROOT-based GUI
// mixes high- and low-level commands
// assume everything is happening on one node
// there could be one or two DTCs and only one CFO
//-----------------------------------------------------------------------------
#ifndef __crvdaq_dtc_interface_cc__
#define __crvdaq_dtc_interface_cc__

#define __CLING__ 1

#include "iostream"
#include "vector"

#include "otsdaq-mu2e-crv/FEInterfaces/ROC_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/FEB_Registers.h"

#include "DtcInterface.hh"
#include "TString.h"    // includes ROOT's Form

#include "TRACE/tracemf.h"
#define  TRACE_NAME "DtcInterface"
#define TLVL_ROCConfig TLVL_DEBUG + 5
#define TLVL_FEBConfig TLVL_DEBUG + 6

using namespace DTCLib;
using namespace std;

namespace crvdaq {

DtcInterface::DtcInterface(int PcieAddr, uint LinkMask, bool SkipInit, bool initRocs, bool initFebs) 
    : mu2edaq::DtcInterface(PcieAddr, LinkMask, SkipInit),
    fInitRocs(initRocs),
    fInitFebs(initFebs)
    {
        fIsCrv = 1;
        if(fInitRocs) InitRocs();
    }

  DtcInterface* DtcInterface::Instance(int PcieAddr, uint LinkMask, bool SkipInit, bool initRocs, bool initFebs) {
    int pcie_addr = PcieAddr;
    if (pcie_addr < 0) {
//-----------------------------------------------------------------------------
// PCIE address is not specified, check environment
//-----------------------------------------------------------------------------
      if (getenv("DTCLIB_DTC") != nullptr) pcie_addr = atoi(getenv("DTCLIB_DTC"));
      else {
        TLOG(TLVL_ERROR) << Form("PcieAddr < 0 and $DTCLIB_DTC is not defined. BAIL out\n");
        return nullptr;
      }
    }
                                    
    TLOG(TLVL_DEBUG) << "pcie_addr:" << pcie_addr
                     << " LinkMask:0x" << std::hex << LinkMask
                     << std::dec
                     << " SkipInit:" << SkipInit << std::endl;
    
    if (fgInstance[pcie_addr] == nullptr) fgInstance[pcie_addr] = new DtcInterface(pcie_addr,LinkMask,SkipInit);
    
    if (fgInstance[pcie_addr]->PcieAddr() != pcie_addr) {
      TLOG(TLVL_ERROR) << Form("DtcInterface::Instance has been already initialized with PcieAddress = %i. BAIL out\n", 
                               fgInstance[pcie_addr]->PcieAddr());
      return nullptr;
    }
    else return dynamic_cast<crvdaq::DtcInterface*>(fgInstance[pcie_addr]);
  }

    void DtcInterface::InitRocs() {

        if(fInitRocs){
            for(int ilink=0; ilink<6; ilink++) {
                // can we do this in parallel?
                if(LinkEnabled(ilink)) {
                    auto link = DTCLib::DTC_Link_ID(ilink);

                    // set the ROC address
                    WriteRocRegister(link, ROC::ID, (uint16_t)ilink);

                    // Enable the onboard PLL (1 is power down)
                    WriteRocRegister(link, ROC::PLLStat,     0x0);
                    // and configure PLL mux to read digital lock
                    WriteRocRegister(link, ROC::PLLMuxHigh, 0x12);
                    WriteRocRegister(link, ROC::PLLMuxHLow, 0x12);

                    // enable package forwarding based on markers
                    //this->writeRegister(ROC::CR, 0x20);
                    SetRocMarkerSync(link, true);

                    WriteRocRegister(link, ROC::Clk80MHz, 0x0); // enable the 80MHz clock alignment

                    // Set CSR of data-FPGAs
                    // bit 3: FM Rx Enable
                    // bit 5: DDR Write Sequencer Enable
                    // bit 7: DDR read sequencer Enable
                    WriteRocRegister(link, ROC::Data_Broadcast|ROC::Data_CRC, 0xA8); //

                    // Reset input buffers
                    ResetRocDDRBuffers(link);
        
                    // Set TRIG 1
                    WriteRocRegister(link, ROC::TRIG, 0x1);

                    // Not set the run mode here, this is done at the run start

                    // enable active FEB/Port communciation 
                    EnableRocActivePorts(link);

                    // uB offset for Wideband/VST tests
                    WriteRocRegister(link, ROC::uBOffset, 0xa);

                    // 
                    if(fInitFebs) InitFebs(link);

                } // end LinkEnabled
            }
        }
        //         

    }

    void DtcInterface::InitFebs(DTCLib::DTC_Link_ID& link) {
        // first handle broadcasts
        TLOG(TLVL_FEBConfig) << "FebConfigure start..." << std::endl;
        // try a read to see if the FEB actually responds
        ReadRocRegister(link, ROC::Version);

        // first broadcast common settings
	    // Set external trigger to RJ45
        // To make the PLL lock properly, we often need to shut the PLL off first
	    WriteRocRegister(link, FEB::AllFEB|FEB::TRIG, 0x1);
        usleep(0); // seems to work, 2025-01-03
        WriteRocRegister(link, FEB::AllFEB|FEB::TRIG, 0x0);

	    // Enable self-triggering on spill gate
	    //this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::IntTrgEn, 0x2); // doesn't seem to work yet? 2025-01-03
        WriteRocRegister(link, FEB::AllFPGA|FEB::IntTrgEn, 0x2);
	    // Set number of ADC samples to 8, will be 12 moving forward
	    WriteRocRegister(link, FEB::AllFEB|FEB::AllFPGA|FEB::Samples, 0x8);
	
        // Reset DDR write/read pointers
        ResetFebDDRBuffers(link); // not really needed

        // Set default settings
        uint16_t TEMPFIX = 0xefff;
        WriteRocRegister(link, (FEB::AllFEB|FEB::AllFPGA|FEB::OnSpillGate)&TEMPFIX, 0x0ff);
        WriteRocRegister(link, (FEB::AllFEB|FEB::AllFPGA|FEB::OffSpillGate)&TEMPFIX, 0x0ff);
        WriteRocRegister(link, (FEB::AllFEB|FEB::AllFPGA|FEB::Pipeline)&TEMPFIX, 0x5);  

        // FEB specific settings
        uint32_t active_ports = GetRocActivePorts(link);
        for(int iport = 0; iport < 24; iport++) {
            if(active_ports & (1 << iport)) { // port active
                SetRocActivePort(link, iport+1);
                WriteRocRegister(link, FEB::AllFPGA|FEB::Port, (uint16_t)(iport+1));
            }
        }
    }

    // RunBegin
    void DtcInterface::InitRocReadoutMode()  {
        // TODO
    }

    //std::vector<std::string> DTCInterface::GetRocRegistersNames     (           bool history);
    //std::vector<uint32_t>    DTCInterface::GetRocRegisters          (int ilink, bool history);
    //std::vector<float>       DTCInterface::GetConvertedRocRegisters (int ilink, bool history);

    //-----------------------------------------------------------
    // ROC specific functions, all with the first argument of linl
    // We might want to make a class out of these at some point
    //-----------------------------------------------------------
    void DtcInterface::SetRocMarkerSync(DTCLib::DTC_Link_ID& link, bool enable) {
        uint32_t cr = ReadRocRegister(link, ROC::CR);
        cr = enable ? (cr | (1u << 5)) : (cr & ~(1u << 5));
        WriteRocRegister(link, ROC::CR, cr);
    }

    void DtcInterface::ResetRocRxBuffers(DTCLib::DTC_Link_ID& link) {
        WriteRocRegister(link, ROC::GTP_CRC, 0x1);
        WriteRocRegister(link, ROC::CRS, 0x300);
        WriteRocRegister(link, ROC::GTP_CRC, 0x1);
    }

    void DtcInterface::ResetRocDDRBuffers(DTCLib::DTC_Link_ID& link) {
    	for (int i = 0; i < 3; ++i) {
	        WriteRocRegister(link, ROC::Data[i]|ROC::Data_DDR_WriteHigh,0x0);
	        WriteRocRegister(link, ROC::Data[i]|ROC::Data_DDR_WriteHigh,0x0);
	        WriteRocRegister(link, ROC::Data[i]|ROC::Data_DDR_ReadHigh, 0x0);
	        WriteRocRegister(link, ROC::Data[i]|ROC::Data_DDR_ReadLow, 0x0);
        }
	}

    void DtcInterface::EnableRocActivePorts(DTCLib::DTC_Link_ID& link, bool enable ) {
        for(int fpga=0; fpga < 4; fpga++) {
            WriteRocRegister(link, ROC::Data[fpga]|ROC::Data_LinkCtrl, (uint16_t)enable);
        }
    }
    void DtcInterface::DisableRocActivePorts(DTCLib::DTC_Link_ID& link) {
        return EnableRocActivePorts(link, false);
    }

    void DtcInterface::ResetFebDDRBuffers(DTCLib::DTC_Link_ID& link) {
    	WriteRocRegister(link, FEB::AllFEB|FEB::AllFPGA|FEB::RdPtrHi, 0x0); 
	    WriteRocRegister(link, FEB::AllFEB|FEB::AllFPGA|FEB::RdPtrLo, 0x0); 
        WriteRocRegister(link, FEB::AllFEB|FEB::AllFPGA|FEB::WrPtrHi, 0x0); 
        WriteRocRegister(link, FEB::AllFEB|FEB::AllFPGA|FEB::WrPtrLo, 0x0); 
    }

    //--------------------------------
    // CRV ROC Port specific functions
    //--------------------------------
    uint32_t DtcInterface::GetRocActivePorts(DTCLib::DTC_Link_ID& link) {
        uint32_t activeHigh = ReadRocRegister(link, ROC::ActivePortsHigh);
	    uint32_t activeLow  = ReadRocRegister(link, ROC::ActivePortsLow);
        return (activeHigh << 16) | (activeLow);
    }

    void DtcInterface::SetRocActivePort(DTCLib::DTC_Link_ID& link, uint16_t port, bool check) {
        if(check) {
            uint32_t active = GetRocActivePorts(link);
            if( !(active & (0x00000001<<(port-1))) ) { // throuw exception if selected port is not activr
                std::stringstream ss;
                ss << "Error selecting port " << +port << ", port is not active: 0x" << std::hex << active;
                throw std::runtime_error(ss.str());
                //__FE_SS__  << "Error selecting port " << +port << ", port is not active: 0x" << std::hex << active;
                //__SS_THROW__;
            }
        }
        WriteRocRegister(link, ROC::LP, port);

        auto startTime = std::chrono::high_resolution_clock::now();
        while(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startTime).count() < 1000) {
            try {
                auto activePort = ReadRocRegister(link,ROC::LP);
                if(activePort == port) {
                    TLOG(TLVL_DEBUG) << "Port " << activePort << " is active (requested " << port << "). Took " 
                    << std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::high_resolution_clock::now() - startTime).count() << " ms." << std::endl;
                    return;
                }
            } catch(...) {
                usleep(5000); // 5ms before retry
            }
        }
    }
};

#endif
