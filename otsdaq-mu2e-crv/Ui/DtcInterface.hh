//-----------------------------------------------------------------------------
// interactive interface for ROOT-based GUI
// mixes high- and low-level commands
// assume everything is happening on one node
// there could be one or two DTCs and only one CFO
//-----------------------------------------------------------------------------
#ifndef __crvaq_dtc_interface_hh__
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
      DtcInterface(int PcieAddr, uint LinkMask, bool SkipInit);
    public:

  };

};

#endif
