//-----------------------------------------------------------------------------
// interactive interface for ROOT-based GUI
// mixes high- and low-level commands
// assume everything is happening on one node
// there could be one or two DTCs and only one CFO
//-----------------------------------------------------------------------------
#ifndef __crvdaq_dtc_interface_hh__
#define __crvdaq_dtc_interface_hh__

#define __CLING__ 1

#include <string>
#include <vector>
#include "iostream"
#include "dtcInterfaceLib/DTC.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_Link_ID.h"


#include "otsdaq-mu2e-tracker/Ui/DtcInterfaceBase.hh"

namespace crvdaq {

  class DtcInterface : public mu2edaq::DtcInterface { 
    private:
        int _tmo_ms = 100;
        bool fInitRocs;
        bool fInitFebs;

        DtcInterface(int PcieAddr, uint LinkMask, bool SkipInit, bool initRocs = false, bool initFebs = false);
        void InitFebs(DTCLib::DTC_Link_ID& link); // part of the constructor
    public:
        static DtcInterface* Instance(int PcieAddr, uint LinkMask = 0x11, bool SkipInit = false, bool initRocs = true, bool initFebs = true);
        void InitRocs(); // part of the constructor
        //void SetInit(bool initRocs, bool initFebs) {
        //    fInitRocs = initRocs;
        //    fInitFebs = initFebs;
        //}

        // all the virtual functions
        void InitRocReadoutMode() override;

        //std::vector<std::string> GetRocRegistersNames     (           bool history = false) override;
        //std::vector<uint32_t>    GetRocRegisters          (int ilink, bool history = false) override;
        //std::vector<float>       GetConvertedRocRegisters (int ilink, bool history = false) override;


        // CRV specific helper functions
        void WriteRocRegister(DTCLib::DTC_Link_ID& Link, uint16_t add, uint16_t data) {
            fDtc->WriteROCRegister   (Link, add , data, false, _tmo_ms);
        };

        uint16_t ReadRocRegister(DTCLib::DTC_Link_ID& Link, uint16_t add) {
            return fDtc->ReadROCRegister   (Link, add, _tmo_ms);
        };

        // CRV ROC specific functions
        void SetRocMarkerSync     (DTCLib::DTC_Link_ID& link, bool enable);
        void ResetRocRxBuffers    (DTCLib::DTC_Link_ID& link);
        void ResetRocDDRBuffers   (DTCLib::DTC_Link_ID& link);
        void EnableRocActivePorts (DTCLib::DTC_Link_ID& link, bool enable = true); 
        void DisableRocActivePorts(DTCLib::DTC_Link_ID& link); 

        // CRV ROC Port specific functions
        uint32_t GetRocActivePorts(DTCLib::DTC_Link_ID& link);
        void SetRocActivePort     (DTCLib::DTC_Link_ID& link, uint16_t port, bool check = true); // starts at 1, max 24  

        // CRV FEB specific functions
        void ResetFebDDRBuffers   (DTCLib::DTC_Link_ID& link);

  };

};

#endif
