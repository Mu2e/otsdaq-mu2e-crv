#include <bitset>
#include <chrono>
#include "otsdaq-mu2e-crv/FEInterfaces/FEBII_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/FEB_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/ROCCosmicRayVetoInterface.h"
#include "otsdaq-mu2e-crv/FEInterfaces/ROC_Registers.h"
#include "otsdaq/Macros/InterfacePluginMacros.h"

using namespace ots;

#define TLVL_ROCConfig TLVL_DEBUG + 5
#define TLVL_FEBConfig TLVL_DEBUG + 6
#define TLVL_Start TLVL_DEBUG + 7

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCCosmicRayVetoInterface"

namespace
{
std::string makeRocConfigureDcsWarning(const std::string& details)
{
	return "RocConfigure could not read after RocConfigure. That likely indicates "
	       "an issue with the DCS. Please contact an expert to reset the CRV ROC "
	       "(wr 0x2 1 might do it, if not try RESET). " +
	       details;
}
}  // namespace

//=========================================================================================
ROCCosmicRayVetoInterface::ROCCosmicRayVetoInterface(
    const std::string&       rocUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       theConfigurationPath)
    : ROCCoreVInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath), gr(false)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__COUT_INFO__ << "ROCCosmicRayVetoInterface instantiated with link: " << (int)linkID_
	              << " and EventWindowDelayOffset = " << delay_ << __E__;

	/*registerFEMacroFunction("Do the CRV Dance",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::DoTheCRV_Dance),
	                        std::vector<std::string>{"Which Step"},
	                        std::vector<std::string>{"Random Result"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Do the CRV Fancy Dance",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::DoTheCRV_Dance2),
	                        std::vector<std::string>{"Which Step"},
	                        std::vector<std::string>{"Random Result"},
	                        1);  // requiredUserPermissions
	*/

	registerFEMacroFunction("Get Firmware Version",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetFirmwareVersion),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"version", "git hash"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Get Test Counter",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetTestCounter),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"counter"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Set Test Counter",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::SetTestCounter),
	                        std::vector<std::string>{"Set Counter (Default: 0)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Set Port Match",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::SetPortMatch),
	    std::vector<std::string>{"port match on (Default: false)",
	                             "FEB timeout in units of 10 us, 0 = none (Default: 0)",
	                             "clear counters (Default: false)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions

	registerFEMacroFunction("Reset uC",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::HardReset),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Set Input Mask",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::SetInputMask),
	                        std::vector<std::string>{"mask (Default: 0xffffff)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	/*registerFEMacroFunction("Configure CRV (FEBI)",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::Configure),
	                        std::vector<std::string>{"hard resets (Default: false)",
	                                                 "bias (Default: 0xaac)",
	                                                 "threshold (Default: 0xc)",
	                                                 "spill length (Default: 0xff)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	*/

	registerFEMacroFunction(
	    "Configure ROC",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::RocConfigure),
	    std::vector<std::string>{
	        "send GR packages (Default: false)",
	        "# of counter packages (Default: 0)",
	        "uB offset (if not GR) (Default: 0xa)",
	        "ROC timeout, units of 6.25ns, off: 0xffff (Default: off)",
	    },
	    std::vector<std::string>{},
	    1);  // requiredUserPermissions

	/*registerFEMacroFunction("FEB Configure",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebConfigure),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions
	*/

	registerFEMacroFunction("Soft Reset",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::SoftReset),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Get Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetStatus),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"version",
	                                                 "git hash",
	                                                 "CR",
	                                                 "Send GR",
	                                                 "Loopback Mode",
	                                                 "PLL lock",
	                                                 "Active Ports",
	                                                 "Uptime",
	                                                 "Link Errors Loss",
	                                                 "Link Errors CRC",
	                                                 "Test Cnt",
	                                                 "Marker Decoded Cnt",
	                                                 "Marker Delayed Cnt",
	                                                 "Heartbeat Rx Cnt",
	                                                 "Heartbeat Tx Cnt",
	                                                 "DR Cnt",
	                                                 "Injection Cnt",
	                                                 "Loopback Markers (fiber) Cnt",
	                                                 "Last Event Length (12.5ns)",
	                                                 "Injection Length (12.5ns)",
	                                                 "Injection Timestamp",
	                                                 "Last EWT",
	                                                 "HB Buffer Empty",
	                                                 "HB Buffer Words",
	                                                 "DR Buffer Empty",
	                                                 "DR Buffer Words",
	                                                 "Link 0 Word Cnt",
	                                                 "Link 1 Word Cnt",
	                                                 "Link 2 Word Cnt",
	                                                 "Event Buffer Empty",
	                                                 "Event Buffer Full"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Get Status Pretty",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetStatusPretty),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("Get POOL",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetPool),
	                        std::vector<std::string>{"Port (Default: -1)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	/*registerFEMacroFunction("FEB Get Status Pretty",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetFebStatusPretty),
	                        std::vector<std::string>{"Port (Default: -1)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("FEB Take Pedestral",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebTakePedestral),
	                        std::vector<std::string>{"Port (Default: -1)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions
	*/

	registerFEMacroFunction("Read Fiber Rx",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FiberRx),
	                        std::vector<std::string>{"number of packages (Default: 10)"},
	                        std::vector<std::string>{"buffer"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Read Fiber Tx",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FiberTx),
	                        std::vector<std::string>{"number of packages (Default: 10)"},
	                        std::vector<std::string>{"buffer"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Set Loopback Mode",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::SetLoopbackMode),
	                        std::vector<std::string>{"loopback mode (Default: 0)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	/*registerFEMacroFunction("FEB Set Bias",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebSetBias),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "fpga [0,1,2,3]",
	                                                 "number [0,1]",
	                                                 "bias"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("FEB Set Bias Trim",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebSetBiasTrim),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "fpga [0,1,2,3]",
	                                                 "channel [0-15]",
	                                                 "bias trim"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("FEB Set Threshold",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebSetThreshold),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "fpga [0,1,2,3]",
	                                                 "channel [0-15]",
	                                                 "threshold"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("FEBs Set Pipeline Delay",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebSetPipeline),
	                        std::vector<std::string>{"pipeline delay (Default 5)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions
	*/

	registerFEMacroFunction("FEBs CMBENA",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebCMBENA),
	                        std::vector<std::string>{"value (Default 1)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("PWRRST",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::PWRRST),
	                        std::vector<std::string>{"port (Default 25 - all)"},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

	/*registerFEMacroFunction(
	    "Histogram",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::GetHistograms),
	    std::vector<std::string>{"port (Default: -1, current active)",
	                             "fpga [0,1,2,3]",
	                             "channel [0-15]",
	                             "interval (Default 2s) [ms]",
	                             "filename (Default: histogram.csv)",
	                             "number of bins (Default all: 0x400)"},
	    std::vector<std::string>{"buffer"},
	    1);  // requiredUserPermissions
	*/

	registerFEMacroFunction("Register Dump",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::RegDump),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	// registerFEMacroFunction("FEB II Configure",
	//                         static_cast<FEVInterface::frontEndMacroFunction_t>(
	//                             &ROCCosmicRayVetoInterface::FebIIConfigure),
	//                             std::vector<std::string>{
	//                                 "port (Default: -1, current active)",
	//                                 "bias (Default: 0xaac)",
	//                                 "threshold (Default: 0xc)",
	//                             },
	//                             std::vector<std::string>{"response"},
	//                             1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Threshold",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetThreshold),
	                        std::vector<std::string>{
	                            "port (Default: -1, all active ports)",
	                            "fpga [0,1,2,3], -1 all (Default)",
	                            "channel [0-15], -1 all (Default)",
	                            "threshold",
	                            "use broadcast for all ports (Default: true)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Bias",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetBias),
	                        std::vector<std::string>{
	                            "port (Default: -1, all active ports)",
	                            "fpga [0,1,2,3], -1 all (Default)",
	                            "number [0,1], -1 all (Default)",
	                            "bias",
	                            "sleep [s] (Default: 5)",
	                            "use broadcast for all ports (Default: true)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Bias Trim",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetBiasTrim),
	                        std::vector<std::string>{
	                            "port (Default: -1, all active ports)",
	                            "fpga [0,1,2,3], -1 all (Default)",
	                            "channel [0-15], -1 all (Default)",
	                            "bias trim",
	                            "use broadcast for all ports (Default: true)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "FEB II Set Gate OnSpill",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIISetGateOnSpill),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "gate start, 6.25ns (Default: 16)",
	                             "gate end, 6.25ns (Default: 255)",
	                             "use broadcast for all ports (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "FEB II Set Gate OffSpill",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIISetGateOffSpill),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "gate start, 6.25ns (Default: 16)",
	                             "gate end, 6.25ns (Default: 15000)",
	                             "use broadcast for all ports (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "FEB II Set Channel",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIISetChannel),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "fpga [0,1,2,3], -1 all (Default)",
	                             "channel [0-15], -1 all (Default)",
	                             "fake (Default: false)",
	                             "off (Default: false)",
	                             "use broadcast for all ports (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Get Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIIGetStatus),
	                        std::vector<std::string>{
	                            "port (Default: -1, current active)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Error Status",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIIErrorStatus),
	                        std::vector<std::string>{
	                            "port (Default: 0 = all active, >0 = single)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "ROC Buffer Status",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::BufferStatus),
	    std::vector<std::string>{
	        "peek head words, DESTRUCTIVE: only on a stuck ROC (Default: false)",
	    },
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions
	registerFEMacroFunction("ROC Buffer Dump",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::BufferDump),
	                        std::vector<std::string>{
	                            "max words per buffer (Default: 512)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("ROC Request Trace",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::RequestTrace),
	                        std::vector<std::string>{
	                            "requests to print in full (Default: 16)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Reset Error Counters",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIIResetErrorCounters),
	                        std::vector<std::string>{
	                            "port (Default: -1, all active ports via broadcast)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("Test FEB Connection",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::TestFebConnection),
	                        std::vector<std::string>{
	                            "register (Default: 0x1023 = CntLO)",
	                            "port (Default: 0 = all active, >0 = single)",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("Test ROC Links",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::TestRocLinks),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{"result", "response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set AFE Offset",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetAFEOffset),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "offset (in range [-512, 511])"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Baselines",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIIGetBaselines),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "re-measure (Default: false)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "FEB II Currents",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIIGetCurrents),
	    std::vector<std::string>{"port (Default: -1, current active)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Trigger Baselines",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIITrigBaselines),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "channel [0-63] (Default: -1, all)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction(
	    "FEB II Configure",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIIConfigure),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "use broadcast for all ports (Default: true)",
	                             "run ROC clock align sequence (Default: true)",
	                             "bias (Default: 0xaaf)",
	                             "skip bias (Default: false)",
	                             "threshold (Default: 0x50)",
	                             "update baseline (Default: true)",
	                             "onSpillGateEnd (Default: 255, 6.25ns)",
	                             "offSpillGateEnd (Default: 15000, 6.25ns)",
	                             "pll reset (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "FEB II PLL Reset and Align",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::PLLReset),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "sleep [ms] (Default: 1000)",
	                             "run ROC clock align sequence (Default: true)",
	                             "use broadcast when port<0 (Default: true)",
	                             "check status (Default: false)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions

	registerFEMacroFunction("FEB II Uptime and EWT",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIIUptimeAndEWT),
	                        std::vector<std::string>{"port (Default: -1, all active)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction("FEB II Get Align Score",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::GetAlignScore),
	                        std::vector<std::string>{"port (Default: -1, all active)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "FEB II Configure from Tables",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIIConfigureFromTables),
	    std::vector<std::string>{
	        "port (Default: -1, all active)",
	        "skip bias (Default: false)",
	        "bias only (Default: false)",
	        "bias overwrite broadcast (Default: -1, disabled; e.g. 0xa7c)",
	        "skip readbacks (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Burst Write Test",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::BurstWriteTest),
	    std::vector<std::string>{"port (Default: -1, all active ports)",
	                             "address (Default: 0x35)",
	                             "value (Default: 0)",
	                             "count (Default: 10)",
	                             "broadcast (Default: true)"},
	    std::vector<std::string>{"response"},
	    1);  // requiredUserPermissions

	registerFEMacroFunction("FEB II LED setting",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetLED),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "offset, 0 (Default)",
	                                                 "LED bias",
	                                                 "on (Default: true)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
}

//==========================================================================================
ROCCosmicRayVetoInterface::~ROCCosmicRayVetoInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << " Destructor" << __E__;
}

////==================================================================================================
// void ROCCosmicRayVetoInterface::writeROCRegister(uint16_t address, uint16_t
// data_to_write)
//{
//	__FE_COUT__ << "Calling write ROC register: link number " << std::dec << (int)linkID_
//	            << ", address = " << address << ", write data = " << data_to_write
//	            << __E__;
//
//	return;
// }
//
////==================================================================================================
// uint16_t ROCCosmicRayVetoInterface::readROCRegister(uint16_t address)
//{
//	__FE_COUT__ << "Calling read ROC register: link number " << std::dec << linkID_
//	            << ", address = " << address << __E__;
//
//	return -1;
// }
//
//============================================================================================
void ROCCosmicRayVetoInterface::writeEmulatorRegister(uint16_t address,
                                                      uint16_t data_to_write)
{
	__FE_COUT__ << "Calling write ROC Emulator register: link number " << std::dec
	            << (int)linkID_ << ", address = " << address
	            << ", write data = " << data_to_write << __E__;

	return;
}

//==================================================================================================
uint16_t ROCCosmicRayVetoInterface::readEmulatorRegister(uint16_t address)
{
	__FE_COUT__ << "Calling read ROC Emulator register: link number " << std::dec
	            << (int)linkID_ << ", address = " << address << __E__;

	return -1;
}

////==================================================================================================
// int ROCCosmicRayVetoInterface::readTimestamp() { return this->readRegister(12); }

////==================================================================================================
// void ROCCosmicRayVetoInterface::writeDelay(uint16_t delay)
//{
//	this->writeRegister(21, delay);
//	return;
// }

////==================================================================================================
// int ROCCosmicRayVetoInterface::readDelay() { return this->readRegister(7); }

////==================================================================================================
// int ROCCosmicRayVetoInterface::readDTCLinkLossCounter() { return this->readRegister(8);
// }

////==================================================================================================
// void ROCCosmicRayVetoInterface::resetDTCLinkLossCounter()
//{
//	this->writeRegister(24, 0x1);
//	return;
// }

//==================================================================================================
void ROCCosmicRayVetoInterface::configure(void)
try
{
	__COUT_INFO__ << "configure CRV ROC";
	// bool gr = false;
	getDTC()->SoftReset();
	try
	{
		auto rocConfigs = getSelfNode()
		                      .getNode("ROCTypeLinkTable")
		                      .getNode("LinkToSubsystemCRVGroupedParametersTable")
		                      .getChildren();
		for(const auto& rocConfig : rocConfigs)
		{
			if(rocConfig.second.getNode("Name").getValueAsString() == "ROCGR")
			{
				gr = rocConfig.second.getNode("Value").getValue<bool>();
				if(gr)
					__FE_COUT__ << "Enable CRV ROC GR mode" << __E__;
				break;
			}
		}
	}
	catch(...)
	{
		TLOG(TLVL_WARNING)
		    << "Missing 'ROCTypeLinkTable/LinkToSubsystemCRVGroupedParametersTable', GR "
		       "mode not set, default to "
		    << gr << __E__;
	}
	RocConfigure(gr, 0, 0x0, 0xffff);

	// ================================ FEB part ================================

	bool doConfigureFEBs = false;
	try
	{
		doConfigureFEBs = Configurable::getSelfNode()
		                      .getNode("EnableFEBConfigureStep")
		                      .getValue<bool>();
	}
	catch(...)
	{
		__FE_COUT__ << "'EnableFEBConfigureStep' not found. Default to "
		            << doConfigureFEBs << __E__;
	}  // ignore missing field
	if(doConfigureFEBs)
		FebConfigure();

	usleep(200000);
	getDTC()->SoftReset();
}
catch(const std::runtime_error& e)
{
	__FE_COUT__ << "Error caught: " << e.what() << __E__;
	throw;
}
catch(...)
{
	__FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
	__FE_COUT__ << ss.str();
	__FE_SS_THROW__;
}

//==============================================================================
void ROCCosmicRayVetoInterface::halt(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::pause(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::resume(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::start(std::string)
{
	{
		const uint16_t markerCnt1 = this->readRegister(ROC::MarkerCnt);
		if(markerCnt1 != 0)
		{
			usleep(500000);
			const uint16_t markerCnt2 = this->readRegister(ROC::MarkerCnt);
			if(markerCnt2 != markerCnt1)
			{
				__FE_SS__ << "Cannot start: ROC::MarkerCnt (0x41) is actively "
				          << "counting (0x" << std::hex << markerCnt1 << " -> 0x"
				          << markerCnt2 << std::dec << " over 500ms). "
				          << "Markers are being sent to this ROC before the run "
				          << "has started — check that the CFO is not sending "
				          << "events and that no other DTC instance is driving "
				          << "this hardware.";
				__FE_SS_THROW__;
			}
		}
	}

	RocConfigure(gr, 0, 0x0, 0xffff);
	sleep(2);

	__FE_COUT__ << "Testing FEB links before run start..." << __E__;
	std::string linkReport;
	const bool  linksOk = testRocLinks(&linkReport, true);
	__FE_COUT__ << linkReport << __E__;
	if(!linksOk)
	{
		__FE_SS__
		    << "FEB link test failed at start: one or more active ports did not "
		       "respond to CntLO readback. This likely means one PHY is locked up. "
		       "Please ask an expert to check (Test ROC Links Macro; lca sn). If the "
		       "link is down, the FEB needs to be power cycled.\n"
		    << linkReport << __E__;
		__FE_SS_THROW__;
	}
	else
		__FE_COUT__ << "FEB link test passed." << __E__;

	// Clear the FEB error counters so the counts read during the run belong to it.
	{
		std::string resetReport;
		const bool  resetOk = resetFebErrorCounters(-1, &resetReport);
		if(resetOk)
			__FE_COUT__ << resetReport << __E__;
		else
			__FE_COUT_WARN__ << "FEB error counter reset failed (run continues): "
			                 << resetReport << __E__;
	}

	// Clear the latching FM encoded serial stream parity error (status 0x001 bit 7)
	// so a flag seen during the run belongs to it.
	{
		std::string clearReport;
		const bool  clearOk = clearFebParityError(-1, &clearReport);
		if(clearOk)
			__FE_COUT__ << clearReport << __E__;
		else
			__FE_COUT_WARN__ << "FEB parity error clear failed (run continues): "
			                 << clearReport << __E__;
	}

	sleep(1);
	RocConfigure(gr, 0, 0x0, 0xffff);
	usleep(1000);

	// Clear the sticky per-port CRC error flags of the data FPGAs (not touched by
	// any of the resets above) so a flag seen after the run belongs to it.
	for(int i = 0; i < 3; ++i)
		this->writeRegister(ROC::Data[i] | ROC::Data_CRCErr, 0xff);

	// The FPGA1 heartbeat buffer must be empty now: no heartbeat arrives before the
	// CFO starts. Words in it at this point put FPGA1 out of step for the whole run
	// (it reads heartbeats in blocks of 9 by counting), and the FEBs then get wrong
	// event window numbers and send no hits (runs 125853, 125854, 125858). Log what
	// is there, and reset the receive buffers again if it is not empty.
	for(int attempt = 1; attempt <= 5; ++attempt)
	{
		const uint16_t hbBuf =
		    this->readRegister(ROC::HrtBtBuffStat);  // bit 15 empty, 9:0 words
		if((hbBuf & 0x3ff) == 0)
		{
			__FE_COUT_INFO__ << "Heartbeat buffer empty at the end of start (0x"
			                 << std::hex << hbBuf << std::dec << ", check " << attempt
			                 << ")." << __E__;
			break;
		}
		__FE_COUT_WARN__ << "Heartbeat buffer holds " << (hbBuf & 0x3ff)
		                 << " words at the end of start (0x" << std::hex << hbBuf
		                 << std::dec << ", check " << attempt
		                 << " of 5): resetting the receive buffers again." << __E__;
		this->writeRegister(ROC::GTP_CRC, 0x1);
		usleep(100000);
	}

	{
		const uint16_t markerCnt1 = this->readRegister(ROC::MarkerCnt);
		if(markerCnt1 != 0)
		{
			usleep(500000);
			const uint16_t markerCnt2 = this->readRegister(ROC::MarkerCnt);
			if(markerCnt2 != markerCnt1)
			{
				__FE_SS__ << "Cannot start: ROC::MarkerCnt (0x41) is actively "
				          << "counting (0x" << std::hex << markerCnt1 << " -> 0x"
				          << markerCnt2 << std::dec << " over 500ms) after "
				          << "RocConfigure in start(). Markers are being sent to "
				          << "this ROC before the run has started — check that the "
				          << "CFO is not sending events and that no other DTC "
				          << "instance is driving this hardware.";
				__FE_SS_THROW__;
			}
			else
				__FE_COUT_WARN__ << "ROC::MarkerCnt is non-zero but stable after "
				                 << "start RocConfigure (0x" << std::hex << markerCnt1
				                 << std::dec << ")." << __E__;
		}
	}
}

//==============================================================================
void ROCCosmicRayVetoInterface::stop(void) {}

//==============================================================================
bool ROCCosmicRayVetoInterface::running(void) { return false; }

//========================================================================
void ROCCosmicRayVetoInterface::DoTheCRV_Dance(__ARGS__)
{
	//	uint32_t address = __GET_ARG_IN__("Which Step", uint32_t);
	__FE_COUT__ << "Hello" << __E__;
	__SET_ARG_OUT__("Random Result", 0xA4);

}  // end DoTheCRV_Dance()

void ROCCosmicRayVetoInterface::DoTheCRV_Dance2(__ARGS__)
{
	//	uint32_t address = __GET_ARG_IN__("Which Step", uint32_t);
	__FE_COUT__ << "Hello" << __E__;
	__SET_ARG_OUT__("Random Result", "Fancy Dance 2");

}  // end DoTheCRV_Dance()

void ROCCosmicRayVetoInterface::GetFirmwareVersion(__ARGS__)
{
	__SET_ARG_OUT__("version", this->readRegister(ROC::Version));
	__SET_ARG_OUT__("git hash",
	                (this->readRegister(ROC::GitHashHigh) << 16) +
	                    this->readRegister(ROC::GitHashLow));
}  // end GetFirmwareVersion()

void ROCCosmicRayVetoInterface::GetTestCounter(__ARGS__)
{
	__SET_ARG_OUT__("counter", this->readRegister(ROC::TestCounter));
}  // end GetTestCounter()

void ROCCosmicRayVetoInterface::SetTestCounter(__ARGS__)
{
	uint16_t value = __GET_ARG_IN__("Set Counter (Default: 0)", uint16_t, 0);
	this->writeRegister(ROC::TestCounter, value);
}  // end SetTestCounter()

// Port match of the data FPGAs (FPGA2 0x0012 and later): for every request one
// event part, in order. The settings are lost at a uC reset.
void ROCCosmicRayVetoInterface::SetPortMatch(__ARGS__)
{
	bool     on      = __GET_ARG_IN__("port match on (Default: false)", bool, false);
	uint16_t timeout = __GET_ARG_IN__(
	    "FEB timeout in units of 10 us, 0 = none (Default: 0)", uint16_t, 0);
	bool clear = __GET_ARG_IN__("clear counters (Default: false)", bool, false);
	std::stringstream ostr;
	ostr << std::endl;
	for(int n = 0; n < 3; ++n)
	{
		uint16_t base    = ROC::Data[n];
		uint16_t version = this->readRegister(base | ROC::Data_Version);
		if(version < 0x12)
		{
			ostr << "data FPGA " << n << ": firmware 0x" << std::hex << version
			     << std::dec << " has no port match, nothing written" << std::endl;
			continue;
		}
		this->writeRegister(base | ROC::Data_PortTimeout, timeout);
		this->writeRegister(base | ROC::Data_PortMatchCtrl,
		                    (on ? 0x1 : 0x0) | (clear ? 0x8000 : 0x0));
		ostr << "data FPGA " << n << ": port match "
		     << ((this->readRegister(base | ROC::Data_PortMatchCtrl) & 0x1) ? "on"
		                                                                    : "off")
		     << ", timeout " << this->readRegister(base | ROC::Data_PortTimeout) * 10
		     << " us" << std::endl;
	}
	__SET_ARG_OUT__("response", ostr.str());
}  // end SetPortMatch()

void ROCCosmicRayVetoInterface::Reset() { this->writeRegister(ROC::Reset, 0x1); }
void ROCCosmicRayVetoInterface::HardReset(__ARGS__) { Reset(); }

void ROCCosmicRayVetoInterface::ResetRxBuffers()
{
	this->writeRegister(ROC::GTP_CRC, 0x1);
	this->writeRegister(ROC::CRS, 0x300);
	this->writeRegister(ROC::GTP_CRC, 0x1);
	sleep(1);
}
void ROCCosmicRayVetoInterface::SoftReset(__ARGS__) { ResetRxBuffers(); }

void ROCCosmicRayVetoInterface::FebConfigure(bool useOtsConfig)
{
	TLOG(TLVL_FEBConfig) << "FebConfigure start..." << __E__;

	if(!useOtsConfig)
	{
		TLOG(TLVL_WARNING) << "FebConfigure called with useOtsConfig=false. "
		                      "FEB II requires config tables — skipping."
		                   << __E__;
		return;
	}

	try
	{
		std::string result = febIIConfigureFromTables();
		TLOG(TLVL_FEBConfig) << "FebConfigure (FEB II from tables):" << result << __E__;
	}
	catch(std::exception& e)
	{
		TLOG(TLVL_ERROR) << "FebConfigure failed: " << e.what() << __E__;
		throw;
	}
}

void ROCCosmicRayVetoInterface::RocConfigure(bool     gr,
                                             uint16_t grn,
                                             uint16_t uBoffset,
                                             uint16_t timeout)
{
	TLOG(TLVL_ROCConfig) << "RocConfigure Start " << __E__;

	// set the ROC address, and read it back before touching anything else so we
	// know DCS write+read works at all (one retry, then fail Configure)
	this->writeRegister(ROC::ID, (uint16_t)linkID_);
	{
		uint16_t idReadback = 0;
		try
		{
			idReadback = readRegisterWithRetry(ROC::ID, 1 /*maxRetries*/, 200 /*ms*/);
		}
		catch(const std::exception& e)
		{
			const std::string msg = makeRocConfigureDcsWarning(
			    std::string("ROC::ID readback failed right after write. Exception: ") +
			    e.what() + ". Failing Configure.");
			__FE_COUT_WARN__ << msg << __E__;
			TLOG(TLVL_WARNING) << msg << __E__;
			throw std::runtime_error(msg);
		}
		catch(...)
		{
			const std::string msg = makeRocConfigureDcsWarning(
			    "ROC::ID readback failed right after write. Unknown exception. Failing "
			    "Configure.");
			__FE_COUT_WARN__ << msg << __E__;
			TLOG(TLVL_WARNING) << msg << __E__;
			throw std::runtime_error(msg);
		}
		if(idReadback != (uint16_t)linkID_)
		{
			std::stringstream ss;
			ss << "ROC::ID readback mismatch: wrote 0x" << std::hex << (uint16_t)linkID_
			   << ", read 0x" << idReadback << std::dec << ". Failing Configure.";
			const std::string msg = makeRocConfigureDcsWarning(ss.str());
			__FE_COUT_WARN__ << msg << __E__;
			TLOG(TLVL_WARNING) << msg << __E__;
			throw std::runtime_error(msg);
		}
		TLOG(TLVL_ROCConfig) << "ROC::ID readback OK (0x" << std::hex << idReadback
		                     << std::dec << ")" << __E__;
	}

	// Enable the onboard PLL (1 is power down)
	this->writeRegister(ROC::PLLStat, 0x0);
	// and configure PLL mux to read digital lock
	this->writeRegister(ROC::PLLMuxHigh, 0x12);
	this->writeRegister(ROC::PLLMuxHLow, 0x12);

	// enable package forwarding based on markers
	// this->writeRegister(ROC::CR, 0x20);
	usleep(1200000);
	// return;

	// After the PLL settle time, confirm DCS read AND write still work using the
	// TestCounter register (throws on failure).
	verifyDcsReadWrite();
	try
	{
		SetMarkerSync(true);
	}
	catch(const std::exception& e)
	{
		const std::string msg = makeRocConfigureDcsWarning(
		    std::string("Marker sync setup failed (ROC::CR access). Exception: ") +
		    e.what() + ". Failing Configure.");
		__FE_COUT_WARN__ << msg << __E__;
		TLOG(TLVL_WARNING) << msg << __E__;
		throw std::runtime_error(msg);
	}
	catch(...)
	{
		const std::string msg = makeRocConfigureDcsWarning(
		    "Marker sync setup failed (ROC::CR access). Unknown exception. Failing "
		    "Configure.");
		__FE_COUT_WARN__ << msg << __E__;
		TLOG(TLVL_WARNING) << msg << __E__;
		throw std::runtime_error(msg);
	}

	this->writeRegister(ROC::Clk80MHz, 0x1);  // enable the 80MHz clock alignment

	// Reset procedure
	// Reset FM Rx: bit 0
	// Reset DDR write: bit 5 from 0 to 1
	// Reset DDR read (Init): bit 8
	// :::::::::::::::;
	// 0x009, 0x0A8, 0x1A8

	// Set CSR of data-FPGAs
	// bit 0: Reset FM Rx
	// bit 3: FM Rx Enable
	// bit 5: DDR Write Sequencer Enable
	// bit 7: DDR read sequencer Enable
	// bit 8: DDR Init (reset read pointer)

	// this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0x009);
	// ResetRxBuffers();
	this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0x009);
	// usleep(100);
	this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0x0A8);
	// usleep(100);
	this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0x1A8);
	// usleep(100);
	this->writeRegister(ROC::GTP_CRC, 0x1);

	// this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0x08);  //

	// Reset input buffers
	// ResetRxBuffers();
	// usleep(100);
	//
	// Reset DDR on Data FPGAs
	// for(int i = 0; i < 3; ++i)
	//{
	//	this->writeRegister(ROC::Data[i] | ROC::Data_DDR_WriteHigh, 0x0);
	//	this->writeRegister(ROC::Data[i] | ROC::Data_DDR_WriteLow, 0x0);
	//	this->writeRegister(ROC::Data[i] | ROC::Data_DDR_ReadHigh, 0x0);
	//	this->writeRegister(ROC::Data[i] | ROC::Data_DDR_ReadLow, 0x0);
	//}
	// this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0xA8);  //

	// Set TRIG 1
	this->writeRegister(ROC::TRIG, 0x1);
	usleep(500000);

	// Enable GR package return
	TLOG(TLVL_ROCConfig) << "Global Run Mode is " << (gr ? "enabled" : "disabled") << "."
	                     << __E__;
	if(gr)
	{
		this->writeRegister(ROC::sendGR, 0x1 + (grn << 8));
		// this->writeRegister(ROC::sendGR, 0x2);///

		// Disable send of active FEBs
		// this->writeRegister(ROC::Data[0] | ROC::Data_LinkCtrl, 0x0);
		this->writeRegister(ROC::uBOffset, 0x0);
	}
	else
	{
		this->writeRegister(ROC::sendGR, 0x0);

		// Enable send of active FEBs
		// this->writeRegister(ROC::Data[0] | ROC::Data_LinkCtrl, 0x0);
		this->writeRegister(ROC::uBOffset, uBoffset);
	}

	// 0xffff means disable timeout
	this->writeRegister(ROC::DRTimeout, timeout);

	{
		const uint16_t dcsBufferWdCnt = this->readRegister(ROC::DcsBufferWdCnt);
		if(dcsBufferWdCnt != 0)
		{
			__FE_COUT_WARN__ << "RocConfigure completed with non-zero DcsBufferWdCnt (0x"
			                 << std::hex << dcsBufferWdCnt << std::dec
			                 << "). This may indicate pending/stale DCS words." << __E__;
		}
	}
}

void ROCCosmicRayVetoInterface::Configure(__ARGS__)
{
	bool     reset      = __GET_ARG_IN__("hard resets (Default: false)", bool, false);
	uint16_t bias       = __GET_ARG_IN__("bias (Default: 0xaac)", uint16_t, 0xaac);
	uint16_t th         = __GET_ARG_IN__("threshold (Default: 0xc)", uint16_t, 0xc);
	uint16_t spillLengh = __GET_ARG_IN__("spill length (Default: 0xff)", uint16_t, 0xff);
	std::stringstream ostr;
	// BusBiases
	ostr << std::endl;
	if(reset)
	{
		ostr << "Power cycle all FEB ports" << std::endl;
		this->writeRegister(ROC::PWRRST, 25);
		ostr << "Reset ROC uC, sleep 10s" << std::endl;
		Reset();
		sleep(10);
	}
	ostr << "ROC Configure with a uB offset of 0xa" << std::endl;
	RocConfigure(false, 0, 0xa);

	ostr << "FEB Configure" << std::endl;
	FebConfigure(false);
	ostr << "Set Biases to 0x" << std::hex << bias << std::endl;
	ostr << "Set Threshold to 0x" << std::hex << th << std::endl;
	for(unsigned int fpga = 0; fpga < 4; fpga++)
	{
		for(unsigned int number = 0; number < 2; number++)
		{
			this->writeRegister(FEB::FPGA[fpga] | (FEB::Bias + (number & 0x1)), bias);
		}
		for(unsigned int channel = 0; channel < 16; channel++)
		{
			this->writeRegister(FEB::FPGA[fpga] | (FEB::Threshold + (channel & 0xf)), th);
		}
	}
	ostr << "Let the bias ramp up for 6s" << std::endl;
	sleep(6);
	ostr << "Take Pedestals" << std::endl;
	this->writeRegister(FEB::AllFPGA | FEB::CSRBroadCast, 0x100);
	ostr << "Set the gate length to 0x" << std::hex << spillLengh << " (" << std::dec
	     << (spillLengh * 0.0125) << "us)" << std::endl;
	this->writeRegister(FEB::AllFPGA | FEB::OffSpillGate, spillLengh);
	ostr << "Ready" << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::RocConfigure(__ARGS__)
{
	bool     gr  = __GET_ARG_IN__("send GR packages (Default: false)", bool, false);
	uint16_t grn = __GET_ARG_IN__("# of counter packages (Default: 0)", uint16_t, 0);
	uint16_t uBoffset =
	    __GET_ARG_IN__("uB offset (if not GR) (Default: 0xa)", uint16_t, 0xa);
	uint16_t rocTimeout = __GET_ARG_IN__(
	    "ROC timeout, units of 6.25ns, off: 0xffff (Default: off)", uint16_t, 0xffff);
	RocConfigure(gr, grn, uBoffset, rocTimeout);
}

void ROCCosmicRayVetoInterface::FebConfigure(__ARGS__)
{
	FebConfigure(false);  // useOtsConfig = false
}

void ROCCosmicRayVetoInterface::GetStatus(__ARGS__)
{
	__SET_ARG_OUT__("version", this->readRegister(ROC::Version));
	__SET_ARG_OUT__("git hash",
	                ((this->readRegister(ROC::GitHashHigh) << 16) +
	                 this->readRegister(ROC::GitHashLow)) &
	                    0xffffffff);
	__SET_ARG_OUT__("CR", this->readRegister(ROC::CR));
	__SET_ARG_OUT__("Send GR", this->readRegister(ROC::sendGR) & 0x1);
	__SET_ARG_OUT__("Loopback Mode", this->readRegister(ROC::LoopbackMode));
	__SET_ARG_OUT__("PLL lock", ((this->readRegister(ROC::PLLStat)) >> 4) & 0x1);
	__SET_ARG_OUT__("Active Ports",
	                (this->readRegister(ROC::ActivePortsHigh) << 16) +
	                    this->readRegister(ROC::ActivePortsLow));
	__SET_ARG_OUT__(
	    "Uptime",
	    (this->readRegister(ROC::UpTimeHigh) << 16) + this->readRegister(ROC::UpTimeLow));
	__SET_ARG_OUT__("Link Errors Loss", this->readRegister(ROC::LinkErrors) & 0xff);
	__SET_ARG_OUT__("Link Errors CRC", this->readRegister(ROC::LinkErrors) >> 12);

	// Counters
	__SET_ARG_OUT__("Test Cnt", this->readRegister(ROC::TestCounter));
	__SET_ARG_OUT__("Marker Decoded Cnt", this->readRegister(ROC::MarkerCnt) & 0xff);
	__SET_ARG_OUT__("Marker Delayed Cnt", (this->readRegister(ROC::MarkerCnt) >> 8));
	__SET_ARG_OUT__("Heartbeat Rx Cnt", this->readRegister(ROC::HeartBeat) & 0xff);
	__SET_ARG_OUT__("Heartbeat Tx Cnt", this->readRegister(ROC::HeartBeat) >> 8);
	__SET_ARG_OUT__(
	    "DR Cnt",
	    (this->readRegister(ROC::DRCntHigh) << 16) + this->readRegister(ROC::DRCnLow));
	__SET_ARG_OUT__("Injection Cnt", this->readRegister(ROC::InjectionCnt));
	__SET_ARG_OUT__("Loopback Markers (fiber) Cnt",
	                this->readRegister(ROC::LoopbackMarkerCnt));

	// Event Lengths
	__SET_ARG_OUT__("Last Event Length (12.5ns)",
	                this->readRegister(ROC::LastEventLength));
	__SET_ARG_OUT__("Injection Length (12.5ns)",
	                this->readRegister(ROC::InjectionLength));
	__SET_ARG_OUT__("Injection Timestamp", this->readRegister(ROC::InjectionTS));

	// Readout liveness and buffers (same registers as Get Status Pretty)
	__SET_ARG_OUT__("Last EWT", this->readRegister(ROC::LastUbSent));
	uint16_t hb = this->readRegister(ROC::HrtBtBuffStat);
	__SET_ARG_OUT__("HB Buffer Empty", hb >> 15);
	__SET_ARG_OUT__("HB Buffer Words", hb & 0x0fff);
	uint16_t dr = this->readRegister(ROC::DreqBuffStat);
	__SET_ARG_OUT__("DR Buffer Empty", dr >> 15);
	__SET_ARG_OUT__("DR Buffer Words", dr & 0x0fff);
	__SET_ARG_OUT__("Link 0 Word Cnt", this->readRegister(ROC::LinkWdCnt0));
	__SET_ARG_OUT__("Link 1 Word Cnt", this->readRegister(ROC::LinkWdCnt1));
	__SET_ARG_OUT__("Link 2 Word Cnt", this->readRegister(ROC::LinkWdCnt2));
	uint16_t ev = this->readRegister(ROC::EvBuffStat);
	__SET_ARG_OUT__("Event Buffer Empty", ev & 0x1);
	__SET_ARG_OUT__("Event Buffer Full", (ev >> 1) & 0x1);
}

void ROCCosmicRayVetoInterface::GetPool(__ARGS__)
{
	uint16_t          port = 1;
	uint16_t          val;
	std::stringstream ostr;
	// BusBiases
	ostr << std::endl;
	ostr << "Serial:         "
	     << "0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister((ROC::POOLPARA | ROC::POOLPARA_Serial) + (port << 7))
	     << std::endl;
	ostr << "Spill Counter:  " << std::setw(5) << std::dec
	     << this->readRegister((ROC::POOLPARA | ROC::POOLPARA_SpillCycleCnt) +
	                           (port << 7))
	     << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_FebTemp) + (port << 7));
	ostr << "FEB Temperature " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.01) << "C (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "FEB Voltages" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_1_2V) + (port << 7));
	ostr << "    1.2V        " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.001) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_1_8V) + (port << 7));
	ostr << "    1.8V        " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.001) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_5V) + (port << 7));
	ostr << "    5V          " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.002) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_10V) + (port << 7));
	ostr << "    10V         " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.004) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_2_5V) + (port << 7));
	ostr << "    2.5V        " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.001) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_n5V) + (port << 7));
	ostr << "    -5V         " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.002) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	ostr << "    15V         " << std::dec << (val * 0.006) << "V (0x"
	     << std::setfill('0') << std::setw(4) << std::hex << val << ")" << std::endl;
	val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_3_3V) + (port << 7));
	ostr << "    3.3V        " << std::fixed << std::setprecision(2) << std::dec
	     << (val * 0.001) << "V (0x" << std::setfill('0') << std::setw(4) << std::hex
	     << val << ")" << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Bias" << std::endl;
	for(int k = 0; k < 8; k++)
	{
		val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_Bias) + (port << 7) + k);
		ostr << "    " << k << "         " << std::fixed << std::setprecision(2)
		     << std::dec << (val * 0.02) << "V (0x" << std::setfill('0') << std::setw(4)
		     << std::hex << val << ")";
		ostr << " ADC "
		     << "0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister((ROC::POOLPARA | ROC::POOLPARA_BiasADC) + k % 2 +
		                           (6 * int(k / 2)) + (port << 7))
		     << std::endl;
	}
	ostr << "----------------" << std::endl;
	ostr << "CMB Temperatures" << std::endl;
	for(int fpga = 0; fpga < 4; fpga++)
	{
		for(int cmb = 0; cmb < 4; cmb++)
		{
			val = this->readRegister((ROC::POOLPARA | ROC::POOLPARA_CMB_Temp) + cmb +
			                         (6 * fpga) + (port << 7));
			ostr << "   " << std::setfill(' ') << std::setw(2) << (fpga * 4 + cmb)
			     << "         " << std::fixed << std::setprecision(2) << std::dec
			     << (val * .0625) << "C (0x" << std::setfill('0') << std::setw(4)
			     << std::hex << val << ")" << std::endl;
		}
	}
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetLED(__ARGS__)
{
	int      port    = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t offset  = __GET_ARG_IN__("offset, 0 (Default)", uint16_t, 0);
	uint16_t LEDbias = __GET_ARG_IN__("LED bias", uint16_t, 0);
	uint16_t onoff   = __GET_ARG_IN__("on (Default: true)", bool, true);

	// broadcast loop
	if(port > 0)
		SetActivePort(port);
	// pulse selection and on/off 01 is led on
	this->writeRegister(FEBII::FlashGateEn, onoff);
	// pulse offset time / duration time set
	// 0x318 	LED Pulser turn on time value, 16 bits, default = 48
	// A write to this address sets the LED pulser turn on time in 6.25ns steps
	// relative to the start of the microbunch. The pulse width is fixed at 12.5ns.
	this->writeRegister(FEBII::LEDOn, offset);

	// LED voltage loop
	// because one FEB's channels are all in one side, don't need to select FEBs, but just
	// ports and also by the same reason, don't need to select LEDs.

	std::stringstream ostr;
	ostr << "Set all FPGAs, ";
	for(unsigned int fpga_ = 0; fpga_ < 4; fpga_++)
	{
		for(unsigned int i = 0; i < 4; i++)
		{
			ostr << "all LEDs ";
			this->writeRegister(FEBII::FPGA[fpga_] | (FEBII::LEDBias + i), LEDbias);
			ostr << "FPGA[" << fpga_ << "] LED bias 0x " << std::hex << LEDbias
			     << " channel" << std::dec << i << ", ";
		}
	}
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::GetStatusPretty(__ARGS__)
{
	std::stringstream ostr;
	uint16_t          val;
	ostr << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Firmware version" << std::endl;
	ostr << "    version:       "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::Version) << std::endl;
	ostr << "    git hash:      "
	     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
	     << (((this->readRegister(ROC::GitHashHigh) << 16) +
	          this->readRegister(ROC::GitHashLow)) &
	         0xffffffff)
	     << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Status" << std::endl;
	ostr << "    ID:            "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::ID) << std::endl;
	val = this->readRegister(ROC::CR);
	ostr << "    Control:       "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::CR) << std::endl;
	// ostr << "        0: IntTmgEn" << "       " << (val & 0x1) << std::endl;
	// ostr << "        1:TmgCntEn*  " << "       " << ((val >> 1) & 0x1) << std::endl;
	ostr << "        2:FormHold "
	     << "       " << ((val >> 2) & 0x1) << std::endl;
	ostr << "        4:ExtTmg   "
	     << "       " << ((val >> 4) & 0x1) << std::endl;
	ostr << "        5:MrkrSyncE"
	     << "       " << ((val >> 5) & 0x1) << std::endl;
	ostr << "        6:TrigTxSel"
	     << "       " << ((val >> 6) & 0x1) << std::endl;

	ostr << "    PLL locked:    "
	     << "  " << (((this->readRegister(ROC::PLLStat)) >> 4) & 0x1) << std::endl;
	auto bits = std::bitset<24>(((this->readRegister(ROC::ActivePortsHigh) << 16) +
	                             this->readRegister(ROC::ActivePortsLow)) &
	                            0xffffffff)
	                .to_string();
	std::reverse(bits.begin(), bits.end());
	ostr << "    active FEBs    "
	     << "  " << bits << std::endl;
	ostr << "    active port:   "
	     << "  " << std::dec << this->readRegister(ROC::LP) << std::endl;
	ostr << "    Loopback:      "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::LoopbackMode) << std::endl;
	ostr << "    80MHz align:   "
	     << "  " << (this->readRegister(ROC::Clk80MHz) & 0x1) << std::endl;
	ostr << "    80MHz al. cnt  "
	     << "  " << std::dec << (this->readRegister(ROC::Clk80MHz) >> 8) << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Settings" << std::endl;
	ostr << "    marker delay:  "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::MarkerDelay) << std::endl;
	ostr << "    DR timeout:    "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::DRTimeout) << std::endl;
	ostr << "    EWT Offset:    "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(ROC::uBOffset) << std::endl;
	val = this->readRegister(ROC::sendGR);
	ostr << "    GR mode:       "
	     << "  " << (val & 0x1) << std::endl;
	ostr << "    GR n packets:  "
	     << "  0xXXXX" << std::endl;
	// ostr << "    GR n packets:  " << std::dec << (val >> 8) << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Counters" << std::endl;
	ostr << "    uptime:        " << std::setfill(' ') << std::setw(8) << std::dec
	     << ((this->readRegister(ROC::UpTimeHigh) << 16) +
	         this->readRegister(ROC::UpTimeLow))
	     << " seconds" << std::endl;
	ostr << "    test cnt:      " << std::setfill(' ') << std::setw(8) << std::dec
	     << this->readRegister(ROC::TestCounter) << std::endl;
	// val = this->readRegister(ROC::MarkerCnt);
	// ostr << "    marker dec.    " << std::setfill(' ') << std::setw(8)
	//                              << (val & 0xff) << std::endl;
	// ostr << "    marker dely    " << std::setfill(' ') << std::setw(8)
	//                               << ((val >> 8)) << std::endl;
	ostr << "    EWT (fiber)    " << std::setfill(' ') << std::setw(8) << std::dec
	     << (this->readRegister(ROC::HeartBeatCn)) << std::endl;
	ostr << "    markers (sent) " << std::setfill(' ') << std::setw(8) << std::dec
	     << (this->readRegister(ROC::HeartBeat)) << std::endl;
	// ostr << "    DR cnt         " << std::setfill(' ') << std::setw(8) << std::dec
	//                               << ((this->readRegister(ROC::DRCntHigh) << 16) +
	//	                               this->readRegister(ROC::DRCnLow))
	//                               << std::endl;
	ostr << "    GR mode:       " << std::setfill(' ') << std::setw(8) << (val & 0x1)
	     << std::endl;
	ostr << "    fake pack. cnt:" << std::setfill(' ') << std::setw(8) << std::dec
	     << (this->readRegister(ROC::sendGR) >> 8) << std::endl;
	ostr << "    loopback       " << std::setfill(' ') << std::setw(8)
	     << (this->readRegister(ROC::LoopbackMarkerCnt)) << std::endl;
	ostr << "    last EWT       "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << (this->readRegister(ROC::LastUbSent)) << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Error Counters" << std::endl;
	val = this->readRegister(ROC::sendGR);
	ostr << "    lock loss      " << std::setfill(' ') << std::setw(8) << std::dec
	     << (val & 0xff) << std::endl;
	ostr << "    crc            " << std::setfill(' ') << std::setw(8) << std::dec
	     << (val >> 12) << std::endl;

	// val = this->readRegister(ROC::HeartBeat);
	// ostr << "    heartbeat rx   " << std::setfill(' ') << std::setw(8)
	//                               << ((val& 0xff)) << std::endl;
	// ostr << "    marker dely    " << std::setfill(' ') << std::setw(8)
	//                               << ((val >> 8)) << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Buffer Status" << std::endl;
	val = this->readRegister(ROC::HrtBtBuffStat);
	ostr << "    HB buf. empty  "
	     << "       " << (val >> 15) << std::endl;
	ostr << "    HB buf. words  " << std::setfill(' ') << std::setw(8) << std::dec
	     << (val & 0x0fff) << std::endl;
	val = this->readRegister(ROC::DreqBuffStat);
	ostr << "    DR buf. empty  "
	     << "       " << (val >> 15) << std::endl;
	ostr << "    DR buf. words  " << std::setfill(' ') << std::setw(8) << std::dec
	     << (val & 0x0fff) << std::endl;
	ostr << "    Wd cnt Link 0  " << std::setfill(' ') << std::setw(8) << std::dec
	     << this->readRegister(ROC::LinkWdCnt0) << std::endl;
	ostr << "    Wd cnt Link 1  " << std::setfill(' ') << std::setw(8) << std::dec
	     << this->readRegister(ROC::LinkWdCnt1) << std::endl;
	ostr << "    Wd cnt Link 2  " << std::setfill(' ') << std::setw(8) << std::dec
	     << this->readRegister(ROC::LinkWdCnt2) << std::endl;
	val = this->readRegister(ROC::EvBuffStat);
	ostr << "    Ev. buff empty "
	     << "       " << (val & 0x1) << std::endl;
	ostr << "    Ev. buff full  "
	     << "       " << ((val & 0x2) >> 1) << std::endl;
	ostr << "----------------" << std::endl;

	ostr << std::endl;
	for(int n = 0; n < 3; n++)
	{
		ostr << std::endl;
		ostr << "============ FPGA " << (n + 1) << "============" << std::endl;
		ostr << "----------------" << std::endl;
		ostr << "Status" << std::endl;
		val = this->readRegister(ROC::Data[n] | ROC::Data_CRC);
		ostr << "    Control:       "
		     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(ROC::CR) << std::endl;
		ostr << "        3:FM Rx En "
		     << "       " << ((val >> 3) & 0x1) << std::endl;
		ostr << "        5:DDR wrtie"
		     << "       " << ((val >> 5) & 0x1) << std::endl;
		// ostr << "        6:Phy Src  " << "       " << ((val >> 6) & 0x1) << std::endl;
		ostr << "        7:DDR read "
		     << "       " << ((val >> 7) & 0x1) << std::endl;
		ostr << "    uptime:        " << std::setfill(' ') << std::setw(8) << std::dec
		     << ((this->readRegister(ROC::Data[n] | ROC::Data_UpTimeHigh) << 16) +
		         this->readRegister(ROC::Data[n] | ROC::Data_UpTimeLow))
		     << " seconds" << std::endl;
		ostr << "    test cnt:      " << std::setfill(' ') << std::setw(8) << std::dec
		     << this->readRegister(ROC::Data[n] | ROC::Data_TestCounter) << std::endl;
		ostr << "----------------" << std::endl;
		ostr << "DDR" << std::endl;
		ostr << "    write add:     "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(ROC::Data[n] | ROC::Data_DDR_WriteHigh) << 16) +
		         this->readRegister(ROC::Data[n] | ROC::Data_DDR_WriteLow))
		     << std::endl;
		ostr << "    read add:      "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(ROC::Data[n] | ROC::Data_DDR_ReadHigh) << 16) +
		         this->readRegister(ROC::Data[n] | ROC::Data_DDR_ReadLow))
		     << std::endl;
		ostr << "----------------" << std::endl;
	}
	ostr << "===============================" << std::endl;

	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::GetFebStatusPretty(__ARGS__)
{
	std::stringstream ostr;
	uint16_t          val;
	ostr << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Status" << std::endl;
	val = this->readRegister(FEB::CR);
	ostr << "    Control:       "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex << val << std::endl;
	ostr << "    IntTrgEn:      "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::IntTrgEn) << std::endl;
	ostr << "    Port:          "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::Port) << std::endl;
	ostr << "----------------" << std::endl;
	ostr << "Settings" << std::endl;
	ostr << "    Pipeline Delay:"
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::Pipeline) << std::endl;
	ostr << "    OnSpillGate:   "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::OnSpillGate) << std::endl;
	ostr << "    OffSpillGate:  "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::OffSpillGate) << std::endl;
	ostr << "    Samples:       "
	     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
	     << this->readRegister(FEB::Samples) << std::endl;
	ostr << "----------------" << std::endl;

	ostr << std::endl;
	for(int n = 0; n < 4; n++)
	{
		ostr << std::endl;
		ostr << "============ FPGA " << (n) << "============" << std::endl;
		ostr << "Version"
		     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(FEB::FPGA[n] | FEB::DebugVersion) << std::endl;
		ostr << "Counters" << std::endl;
		ostr << "    onSpill:      " << std::setfill(' ') << std::setw(8) << std::dec
		     << this->readRegister(FEB::FPGA[n] | FEB::onSpillCnt) << std::endl;
		ostr << "    offSpill:     " << std::setfill(' ') << std::setw(8) << std::dec
		     << this->readRegister(FEB::FPGA[n] | FEB::offSpillCnt) << std::endl;
		ostr << "DDR" << std::endl;
		ostr << "    write add:     "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(FEB::FPGA[n] | FEB::WrPtrHi) << 16) +
		         this->readRegister(FEB::FPGA[n] | FEB::WrPtrLo))
		     << std::endl;
		ostr << "    read add:      "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(FEB::FPGA[n] | FEB::RdPtrHi) << 16) +
		         this->readRegister(FEB::FPGA[n] | FEB::RdPtrLo))
		     << std::endl;
		ostr << "uB" << std::endl;
		ostr << "    uB:            "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(FEB::FPGA[n] | FEB::uBHi) << 16) +
		         this->readRegister(FEB::FPGA[n] | FEB::uBLo))
		     << std::endl;
		ostr << "    uBBuff:        "
		     << "  0x" << std::setfill('0') << std::setw(8) << std::hex
		     << ((this->readRegister(FEB::FPGA[n] | FEB::uBBuffHi) << 16) +
		         this->readRegister(FEB::FPGA[n] | FEB::uBBuffLo))
		     << std::endl;
		ostr << "Channel Settings" << std::endl;
		ostr << "    Bias0:       "
		     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister((FEB::FPGA[n] | FEB::Bias) + 0) << std::endl;
		ostr << "    Bias1:       "
		     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister((FEB::FPGA[n] | FEB::Bias) + 1) << std::endl;
		for(int j = 0; j < 8; j++)
		{
			ostr << "    Bias Trim " << j << ":  "
			     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEB::FPGA[n] | FEB::BiasTrim) + j) << std::endl;
		}
		for(int j = 0; j < 8; j++)
		{
			ostr << "    Threshold " << j << ":  "
			     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEB::FPGA[n] | FEB::Threshold) + j) << std::endl;
		}
		for(int j = 0; j < 8; j++)
		{
			ostr << "    Pedestal " << j << ":  "
			     << "  0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEB::FPGA[n] | FEB::Pedestal) + j) << std::endl;
		}
	}
	ostr << "===============================" << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FiberRx(__ARGS__)
{
	int n = __GET_ARG_IN__("number of packages (Default: 10)", int, 10);

	std::stringstream o;
	o << std::endl;
	for(int i = 0; i < n; ++i)
	{  // n packages
		for(int k = 0; k < 10; ++k)
		{  // 10 words per package
			o << std::hex << std::setfill('0') << std::setw(4)
			  << this->readRegister(ROC::GTPRxRead) << " ";
		}
		o << std::endl;
	}
	__SET_ARG_OUT__("buffer", o.str());
}

void ROCCosmicRayVetoInterface::FiberTx(__ARGS__)
{
	int n = __GET_ARG_IN__("number of packages (Default: 10)", int, 10);

	std::stringstream o;
	o << std::endl;
	for(int i = 0; i < n; ++i)
	{  // n packages
		for(int k = 0; k < 10; ++k)
		{  // 10 words per package
			o << std::hex << std::setfill('0') << std::setw(4)
			  << this->readRegister(ROC::GTPTxRead) << " ";
		}
		o << std::endl;
	}
	__SET_ARG_OUT__("buffer", o.str());
}

void ROCCosmicRayVetoInterface::FebSetBias(__ARGS__)
{
	int      port   = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga   = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t number = __GET_ARG_IN__("number [0,1]", uint16_t);
	uint16_t bias   = __GET_ARG_IN__("bias", uint16_t, 0);

	if(port > 0)
	{
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga] | (FEB::Bias + (number & 0x1)), bias);
}

void ROCCosmicRayVetoInterface::FebSetBiasTrim(__ARGS__)
{
	int      port     = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga     = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t channel  = __GET_ARG_IN__("channel [0-15]", uint16_t);
	uint16_t biasTrim = __GET_ARG_IN__("bias trim", uint16_t, 0);

	if(port > 0)
	{
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga] | (FEB::BiasTrim + (channel & 0xf)), biasTrim);
}

void ROCCosmicRayVetoInterface::FebSetThreshold(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga      = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t channel   = __GET_ARG_IN__("channel [0-15]", uint16_t);
	uint16_t threshold = __GET_ARG_IN__("threshold", uint16_t, 0);

	if(port > 0)
	{
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga] | (FEB::Threshold + (channel & 0xf)), threshold);
}

void ROCCosmicRayVetoInterface::FebSetPipeline(__ARGS__)
{
	uint16_t hitPipelineDelay = __GET_ARG_IN__("pipeline delay (Default 5)", uint16_t, 5);
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::Pipeline, hitPipelineDelay);
}

void ROCCosmicRayVetoInterface::SetLoopbackMode(__ARGS__)
{
	int16_t mode = __GET_ARG_IN__("loopback mode (Default: 0)", int16_t, 0);
	this->writeRegister(ROC::LoopbackMode, mode);
}

void ROCCosmicRayVetoInterface::FebCMBENA(__ARGS__)
{
	int16_t value = __GET_ARG_IN__("value (Default 1)", int16_t, 1);
	this->writeRegister(FEB::AllFEB | FEB::CMBENA, value);
}

void ROCCosmicRayVetoInterface::PWRRST(__ARGS__)
{
	int16_t port = __GET_ARG_IN__("port (Default 25 - all)", int16_t, 25);
	this->writeRegister(ROC::PWRRST, port);
}

void ROCCosmicRayVetoInterface::RegDump(__ARGS__)
{
	std::stringstream ostr;
	ostr << "ROC Register Dump:" << std::endl;
	for(uint16_t add = 0; add <= 0xFF; add++)
	{
		ostr << "0x" << std::setfill('0') << std::setw(4) << std::hex << add << ": ";
		ostr << "0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(add) << std::endl;
	}

	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::GetHistograms(__ARGS__)
{
	int         port     = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	int16_t     interval = __GET_ARG_IN__("interval (Default 2s) [ms]", int16_t, 2000);
	uint16_t    fpga     = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t    channel  = __GET_ARG_IN__("channel [0-15]", uint16_t);
	std::string filename =
	    __GET_ARG_IN__("filename (Default: histogram.csv)", std::string, "histogram.csv");
	uint16_t nbins =
	    __GET_ARG_IN__("number of bins (Default all: 0x400)", uint16_t, 0x400);

	uint16_t histRunParam = channel & 0x7;
	if(channel & 0x8)
		histRunParam += 0x40;
	else
		histRunParam += 0x20;

	if(port > 0)
		SetActivePort(port);
	this->writeRegister(FEB::FPGA[fpga] | FEB::HistInterval, interval);
	this->writeRegister(FEB::FPGA[fpga] | FEB::HistRun, histRunParam);
	this->writeRegister(
	    FEB::FPGA[fpga] | (FEB::HistPointer + (channel & 0x8 ? 0x1 : 0x0)), 0);

	std::stringstream o;
	// o << "DEBUG: interval: " << interval << ", port: " << port << ", fpga: " << fpga <<
	// ", channel: " << channel << ", filename: " << filename << std::endl;
	//__SET_ARG_OUT__("buffer", o.str());

	//        __SET_ARG_OUT__("buffer", "Waiting for histogram to be filled ...");  //is
	//        there a way to add additional text to the output window while the macro is
	//        running?
	// this->writeRegister(ROC::POOLENA, 0x0); // disaple the pooling
	TLOG(TLVL_DEBUG) << "Sleep for " << interval / 1000 << "s to fill the histogram"
	                 << __E__;
	sleep(interval / 1000);
	while(1)
	{
		uint16_t histStatus = this->readRegister(FEB::FPGA[fpga] | FEB::HistRun);
		TLOG(TLVL_DEBUG) << "Test if the histogram is done: status = 0x" << std::hex
		                 << histStatus << __E__;
		if(histStatus & 0x20 || histStatus & 0x40)
			sleep(1);
		else
			break;
	}
	//        __SET_ARG_OUT__("buffer", "Done!");

	/*
	//ReadBlock results in errors here and in the existing MacroMaker function
	        std::vector<uint16_t> histMemory;
	        this->readBlock(histMemory, FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ?
	0x1 : 0x0)), 0x800, false);

	        std::ofstream histFile;
	        histFile.open(filename);
	        for(size_t i=0; i<histMemory.size(); i+=2)
	        {
	          uint32_t binContent = (((uint32_t)histMemory.at(i))<<16) +
	histMemory.at(i+1); histFile << binContent << std::endl;
	        }
	        histFile.close();
	*/
	// ReadRegister results in errors here unless increasing the timeout in
	// ROCCoreVInterface::readROCRegister to 1000ms
	std::ofstream histFile;
	histFile.open(filename);
	TLOG(TLVL_DEBUG) << "Open '" << filename << "' for histogram. Start read out."
	                 << __E__;
	for(size_t i = 0; i < nbins * 2; i += 2)
	{
		TLOG(TLVL_DEBUG) << "Read histogram bin " << i << __E__;
		// auto read0 = chrono::high_resolution_clock::now();
		// auto read1 = chrono::high_resolution_clock::now();
		// auto read2 = chrono::high_resolution_clock::now();
		// auto read3 = chrono::high_resolution_clock::now();
		uint32_t binContent = 0;
		try
		{
			auto read0 = chrono::high_resolution_clock::now();
			binContent =
			    this->readRegister(FEB::FPGA[fpga] |
			                       (FEB::HistMemory + (channel & 0x8 ? 0x1 : 0x0)))
			    << 16;
			auto read1 = chrono::high_resolution_clock::now();
			// read2 = chrono::high_resolution_clock::now();
			binContent += this->readRegister(
			    FEB::FPGA[fpga] | (FEB::HistMemory + (channel & 0x8 ? 0x1 : 0x0)));
			auto read3 = chrono::high_resolution_clock::now();
			TLOG(TLVL_DEBUG)
			    << "Read times "
			    << chrono::duration_cast<chrono::milliseconds>(read1 - read0).count()
			    << "ms, "
			    << chrono::duration_cast<chrono::milliseconds>(read3 - read1).count()
			    << "ms" << __E__;
			histFile << binContent << std::endl;
			o << binContent << ",";
		}
		catch(...)
		{
			// assume one missed DCS from the FEB
			TLOG(TLVL_ERROR) << "Did we miss one DCS read from the FEB?" << __E__;
			// auto read4 = chrono::high_resolution_clock::now();
			// TLOG(TLVL_DEBUG) << "Read times " <<
			// chrono::duration_cast<chrono::milliseconds>(read1 - read0).count() << "ms,
			// "
			//                                   <<
			//                                   chrono::duration_cast<chrono::milliseconds>(read3
			//                                   - read2).count() << "ms, "
			//                                   <<
			//                                   chrono::duration_cast<chrono::milliseconds>(read4
			//                                   - read0).count() << "ms (since start of
			//                                   iteration)" <<  __E__;
			// sleep(1.); // 20ms is a typical
			// TLOG(TLVL_DEBUG) << "Waited 1s, try to read again" << __E__;
			// this->readRegister(FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ? 0x1 :
			// 0x0))); // to clean out histFile << -1 << std::endl;
		}
	}
	histFile.close();
	__SET_ARG_OUT__("buffer", o.str());
}
// FEB related functions

void ROCCosmicRayVetoInterface::FebTakePedestral(__ARGS__)
{
	int16_t port = __GET_ARG_IN__("Port (Default: -1)", int16_t, -1);
	if(port < 0)
	{
		this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::CSRBroadCast, 0x100);
	}
	else
	{
		// TODO, select PORT
		this->writeRegister(FEB::AllFPGA | FEB::CSRBroadCast, 0x100);
	}
}

uint32_t ROCCosmicRayVetoInterface::GetActivePorts()
{
	uint32_t activeHigh = this->readRegister(ROC::ActivePortsHigh);
	uint32_t activeLow  = this->readRegister(ROC::ActivePortsLow);
	return (activeHigh << 16) | (activeLow);
}

void ROCCosmicRayVetoInterface::SetActivePort(uint16_t port, bool check)
{
	if(check)
	{
		uint32_t active = GetActivePorts();
		if(!(active & (0x00000001 << (port - 1))))
		{  // throuw exception if selected port is not activr
			// std::stringstream ss;
			__FE_SS__ << "Error selecting port " << +port << ", port is not active: 0x"
			          << std::hex << active;
			__SS_THROW__;
			// throw std::runtime_error(ss.str());
		}
	}
	this->writeRegister(ROC::LP, port);

	auto startTime = std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<std::chrono::milliseconds>(
	          std::chrono::high_resolution_clock::now() - startTime)
	          .count() < 3000)
	{
		try
		{
			auto activePort = this->readRegister(ROC::LP);
			if(activePort == port)
			{
				TLOG(TLVL_DEBUG)
				    << "Port " << activePort << " is active (requested " << port
				    << "). Took "
				    << std::chrono::duration_cast<std::chrono::milliseconds>(
				           std::chrono::high_resolution_clock::now() - startTime)
				           .count()
				    << " ms." << __E__;
				return;
			}
		}
		catch(...)
		{
			usleep(5000);  // 5ms before retry
		}
	}
}

void ROCCosmicRayVetoInterface::SetMarkerSync(bool enable)
{
	uint32_t cr = this->readRegister(ROC::CR);
	cr          = enable ? (cr | (1u << 5)) : (cr & ~(1u << 5));
	// Always clear bit 4 (ExtTmg). In the CRV FPGA1 firmware this bit switches the
	// HeartBeatFM output to the FEBs from the heartbeat/marker transmitter to the GPI
	// pin (Controller_FPGA1.vhd: "HeartBeatFM <= HrtBtTxOuts.FM when ExtTmg = '0'
	// else GPI"). The ROC uC Ver630 line writes 0x10 to this register at boot
	// ("set LVDS clock to 20MHz", valid only for the non-CRV FPGA1), which leaves
	// the FEBs without a valid encoded clock/marker stream. Do not trust the
	// uC-set value here.
	cr &= ~(1u << 4);
	this->writeRegister(ROC::CR, cr);
}

/*
void ROCCosmicRayVetoInterface::FebIIConfigure(__ARGS__)
{
    int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
    uint16_t bias = __GET_ARG_IN__("bias (Default: 0xaac)", uint16_t, 0xaac);
    uint16_t threshold = __GET_ARG_IN__("threshold (Default: 0xc)", uint16_t, 0xc);
    if(port > 0) SetActivePort(port);
    std::stringstream ostr;
    for(unsigned int fpga = 0; fpga < 4; ++fpga) {
        this->writeRegister(FEBII::FPGA[fpga] | FEBII::ThresholdGlobal, threshold);
        for(unsigned int number = 0; number < 4; ++number) {
            this->writeRegister(FEBII::FPGA[fpga] | (FEBII::BiasBase + (number & 0x1)),
bias);
        }
        ostr << "FPGA " << fpga << ": Set ThresholdGlobal=0x" << std::hex << threshold <<
", CR=0x" << bias << std::endl;
    }
    __SET_ARG_OUT__("response", ostr.str());
}*/

void ROCCosmicRayVetoInterface::FebIISetThreshold(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t fpga      = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t channel   = __GET_ARG_IN__("channel [0-15], -1 all (Default)", uint16_t, -1);
	uint16_t threshold = __GET_ARG_IN__("threshold", uint16_t, 0);
	bool     use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;

	auto setThresholdRegisters = [&](uint16_t PORT_) {
		uint16_t fpga_start = (fpga == uint16_t(-1)) ? 0 : fpga;
		uint16_t fpga_end   = (fpga == uint16_t(-1)) ? 4 : fpga + 1;
		for(uint16_t f = fpga_start; f < fpga_end; ++f)
		{
			uint16_t ch_start = (channel == uint16_t(-1)) ? 0 : channel;
			uint16_t ch_end   = (channel == uint16_t(-1)) ? 16 : channel + 1;
			for(uint16_t ch = ch_start; ch < ch_end; ++ch)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[f] | (FEBII::ThresholdBase + (ch & 0xF)),
				    threshold);
			}
		}
	};

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			setThresholdRegisters(ROC::FEB);
		}
		ostr << "Set threshold=0x" << std::hex << threshold << " on each active port"
		     << std::endl;
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
			PORT_ = PORT_ | ROC::FEB_Broadcast;
		setThresholdRegisters(PORT_);
		if(use_broadcast)
			ostr << "Set threshold=0x" << std::hex << threshold << " using broadcast"
			     << std::endl;
		else
			ostr << "Set threshold=0x" << std::hex << threshold << std::endl;
	}

	waitForFebResponsive();
	ostr << "waitForFebResponsive: ok" << std::endl;

	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetBias(__ARGS__)
{
	int      port    = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t fpga    = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t number  = __GET_ARG_IN__("number [0,1], -1 all (Default)", uint16_t, -1);
	uint16_t bias    = __GET_ARG_IN__("bias", uint16_t, 0);
	int      sleep_s = __GET_ARG_IN__("sleep [s] (Default: 0)", int, 0);
	bool     use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;

	auto setBiasRegisters = [&](uint16_t PORT_) {
		uint16_t fpga_start = (fpga == uint16_t(-1)) ? 0 : fpga;
		uint16_t fpga_end   = (fpga == uint16_t(-1)) ? 4 : fpga + 1;
		for(uint16_t f = fpga_start; f < fpga_end; ++f)
		{
			uint16_t num_start = (number == uint16_t(-1)) ? 0 : number;
			uint16_t num_end   = (number == uint16_t(-1)) ? 2 : number + 1;
			for(uint16_t idx = num_start; idx < num_end; ++idx)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[f] | (FEBII::BiasBase + (idx & 0x1)), bias);
				if(sleep_s > 0)
					sleep(sleep_s);
				ostr << "  FPGA" << f << "[" << idx << "] = 0x" << std::hex << bias
				     << std::dec << std::endl;
			}
		}
	};

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			ostr << "Port " << p << ":" << std::endl;
			setBiasRegisters(ROC::FEB);
		}
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
		{
			PORT_ = PORT_ | ROC::FEB_Broadcast;
			ostr << "Broadcast:" << std::endl;
		}
		setBiasRegisters(PORT_);
	}

	ostr << "Set bias=0x" << std::hex << bias << std::dec << " (sleep=" << sleep_s
	     << "s per step)" << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetBiasTrim(__ARGS__)
{
	int      port     = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t fpga     = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t channel  = __GET_ARG_IN__("channel [0-15], -1 all (Default)", uint16_t, -1);
	uint16_t biasTrim = __GET_ARG_IN__("bias trim", uint16_t, 0);
	bool     use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;

	auto setTrimRegisters = [&](uint16_t PORT_) {
		uint16_t fpga_start = (fpga == uint16_t(-1)) ? 0 : fpga;
		uint16_t fpga_end   = (fpga == uint16_t(-1)) ? 4 : fpga + 1;
		for(uint16_t f = fpga_start; f < fpga_end; ++f)
		{
			uint16_t ch_start = (channel == uint16_t(-1)) ? 0 : channel;
			uint16_t ch_end   = (channel == uint16_t(-1)) ? 16 : channel + 1;
			for(uint16_t ch = ch_start; ch < ch_end; ++ch)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[f] | (FEBII::TrimBase + (ch & 0xF)), biasTrim);
			}
		}
	};

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			setTrimRegisters(ROC::FEB);
		}
		ostr << "Set bias trim=0x" << std::hex << biasTrim << " on each active port"
		     << std::endl;
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
			PORT_ = PORT_ | ROC::FEB_Broadcast;
		setTrimRegisters(PORT_);
		if(use_broadcast)
			ostr << "Set bias trim=0x" << std::hex << biasTrim << " using broadcast"
			     << std::endl;
		else
			ostr << "Set bias trim=0x" << std::hex << biasTrim << std::endl;
	}

	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetGateOnSpill(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t gateStart = __GET_ARG_IN__("gate start, 6.25ns (Default: 16)", uint16_t, 16);
	uint16_t gateEnd   = __GET_ARG_IN__("gate end, 6.25ns (Default: 255)", uint16_t, 255);
	bool     use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOnOnSpill),
			                    gateStart);
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOffOnSpill),
			                    gateEnd);
		}
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
			PORT_ = PORT_ | ROC::FEB_Broadcast;

		this->writeRegister(PORT_ | FEBII::GateOnOnSpill, gateStart);
		this->writeRegister(PORT_ | FEBII::GateOffOnSpill, gateEnd);
	}
	std::stringstream ostr;
	if(use_broadcast)
		ostr << "Set GateOnOnSpill=0x" << std::hex << gateStart << ", GateOffOnSpill=0x"
		     << gateEnd << " using broadcast" << std::endl;
	else if(all_ports)
		ostr << "Set GateOnOnSpill=0x" << std::hex << gateStart << ", GateOffOnSpill=0x"
		     << gateEnd << " on each active port" << std::endl;
	else
		ostr << "Set GateOnOnSpill=0x" << std::hex << gateStart << ", GateOffOnSpill=0x"
		     << gateEnd << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetGateOffSpill(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t gateStart = __GET_ARG_IN__("gate start, 6.25ns (Default: 16)", uint16_t, 16);
	uint16_t gateEnd =
	    __GET_ARG_IN__("gate end, 6.25ns (Default: 15000)", uint16_t, 15000);
	bool use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOnOffSpill),
			                    gateStart);
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOffOffSpill),
			                    gateEnd);
		}
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
			PORT_ = PORT_ | ROC::FEB_Broadcast;

		this->writeRegister(PORT_ | FEBII::GateOnOffSpill, gateStart);
		this->writeRegister(PORT_ | FEBII::GateOffOffSpill, gateEnd);
	}
	std::stringstream ostr;
	if(use_broadcast)
		ostr << "Set GateOnOffSpill=0x" << std::hex << gateStart << ", GateOffOffSpill=0x"
		     << gateEnd << " using broadcast" << std::endl;
	else if(all_ports)
		ostr << "Set GateOnOffSpill=0x" << std::hex << gateStart << ", GateOffOffSpill=0x"
		     << gateEnd << " on each active port" << std::endl;
	else
		ostr << "Set GateOnOffSpill=0x" << std::hex << gateStart << ", GateOffOffSpill=0x"
		     << gateEnd << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIIGetStatus(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	if(port > 0)
		SetActivePort(port);
	std::stringstream ostr;
	for(int n = 0; n < 4; n++)
	{
		ostr << std::endl;
		ostr << "============ FPGA " << n << "============" << std::endl;
		ostr << "Firmware Version: 0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(FEBII::FPGA[n] | FEBII::FirmwareHI) << " "
		     << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(FEBII::FPGA[n] | FEBII::FirmwareLO) << std::endl;
		ostr << "Counters" << std::endl;
		ostr << "    Uptime:         " << std::dec
		     << ((this->readRegister(FEBII::FPGA[n] | FEBII::UptimeHI) << 16) |
		         this->readRegister(FEBII::FPGA[n] | FEBII::UptimeLo))
		     << std::endl;
		ostr << "    Counter:        0x" << std::setfill('0') << std::setw(4) << std::hex
		     << ((this->readRegister(FEBII::FPGA[n] | FEBII::CntHI) << 16) |
		         this->readRegister(FEBII::FPGA[n] | FEBII::CntLO))
		     << std::endl;
		ostr << "    last EWT:      "
		     << " 0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(FEBII::FPGA[n] | FEBII::EWT) << std::endl;
		ostr << "    EW Counter:    "
		     << " 0x" << std::setfill('0') << std::setw(4) << std::hex
		     << this->readRegister(FEBII::FPGA[n] | FEBII::EWTCount) << std::endl;
		ostr << "Thresholds" << std::endl;
		for(int j = 0; j < 16; j++)
		{
			ostr << "    Threshold " << j << ":  "
			     << " 0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEBII::FPGA[n] | FEBII::ThresholdBase) + j)
			     << std::endl;
		}
		ostr << "Channel Map" << std::endl;
		for(int j = 0; j < 16; j++)
		{
			ostr << "    Channel Map " << j << ":  "
			     << " 0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEBII::FPGA[n] | FEBII::ChannelMapBase) + j)
			     << std::endl;
		}
		ostr << "Bias Trim" << std::endl;
		for(int j = 0; j < 16; j++)
		{
			ostr << "    Trim " << j << ":  "
			     << " 0x" << std::setfill('0') << std::setw(4) << std::hex
			     << this->readRegister((FEBII::FPGA[n] | FEBII::TrimBase) + j)
			     << std::endl;
		}
	}
	ostr << "===============================" << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

//==============================================================================
// FPGA die temperature from the FEB II DDR status register (FEBII::DDRStatus).
// FEB2 firmware before 2026-09-18 (cRV_FEB2 commit 982c940) puts the raw 12-bit
// XADC value in bits 11..0, degC = raw * 0.123 - 273. Newer firmware puts
// integer degC in bits 7..0 with bits 12..8 zero. In the old layout bits 11..8
// are non-zero for any temperature above 0 degC, so they select the decode.
static double ddrStatusTemperatureC(uint16_t ddrStatus)
{
	const uint16_t raw12 = ddrStatus & 0x0FFF;
	if(raw12 & 0x0F00)
		return raw12 * 0.123 - 273.0;
	return static_cast<double>(ddrStatus & 0x00FF);
}

void ROCCosmicRayVetoInterface::FebIIErrorStatus(__ARGS__)
{
	int requestedPort =
	    __GET_ARG_IN__("port (Default: 0 = all active, >0 = single)", int, 0);

	std::stringstream response;
	uint32_t          active = GetActivePorts();

	response << "FEB Error Status / DDR Status" << std::endl;
	response << "Active ports mask: 0x" << std::hex << std::setw(6) << std::setfill('0')
	         << active << std::dec << std::setfill(' ') << std::endl;

	auto readPort = [this, &response](uint16_t port) {
		try
		{
			SetActivePort(port, true);
		}
		catch(...)
		{
			response << "Port " << port << ": errCnt=FAIL" << std::endl;
			response << "  failed to select port" << std::endl;
			return;
		}

		uint16_t errCnt = 0;
		try
		{
			errCnt = this->readRegister(FEBII::ErrorCounts);
		}
		catch(...)
		{
			response << "Port " << port << ": errCnt=FAIL" << std::endl;
			response << "  error count register read failed" << std::endl;
			return;
		}

		response << "Port " << port << ": errCnt=0x" << std::hex << std::setfill('0')
		         << std::setw(4) << errCnt;

		uint16_t ddr[4];
		for(int n = 0; n < 4; ++n)
		{
			try
			{
				ddr[n] = this->readRegister(FEBII::FPGA[n] | FEBII::DDRStatus);
			}
			catch(...)
			{
				ddr[n] = 0xFFFF;
			}
			response << " fpga" << n << "=0x" << std::setfill('0') << std::setw(4)
			         << ddr[n];
		}
		response << std::dec << std::setfill(' ') << std::endl;

		for(int n = 0; n < 4; ++n)
		{
			uint16_t v          = ddr[n];
			int      initOK     = (v >> 15) & 1;
			int      almostFull = (v >> 14) & 1;
			int      full       = (v >> 13) & 1;
			response << "  FPGA " << n << ": initOK=" << initOK
			         << " almostFull=" << almostFull << " full=" << full
			         << " temp=" << std::fixed << std::setprecision(1)
			         << ddrStatusTemperatureC(v) << "C" << std::endl;
		}
	};

	if(requestedPort > 0)
	{
		readPort(static_cast<uint16_t>(requestedPort));
	}
	else
	{
		bool anyPort = false;
		for(uint16_t port = 1; port <= 24; ++port)
		{
			if(active & (0x00000001u << (port - 1)))
			{
				anyPort = true;
				readPort(port);
			}
		}
		if(!anyPort)
			response << "No active ports found." << std::endl;
	}

	__SET_ARG_OUT__("response", response.str());
}

//==============================================================================
// Readout-chain buffer status of the ROC, to find where data got stuck when the
// ROC stops answering data requests (DTC reports timeouts).
//
// Data path (CRVFirmware ROC/FPGA2 and ROC/FPGA1):
//   FEB port -> PhyRxBuff (4k, one per port, 8 per data FPGA) -> DDR -> LinkTx
//   -> LinkFIFO (8k, one per data FPGA, in FPGA1) -> EventBuilder -> EventBuff
//   -> PacketFormer -> DTC.
// A data FPGA copies an event to its DDR only when EVERY port in Rx_active
// holds a complete event, and the event builder builds only when EVERY active
// link holds a complete event. The data FPGAs send a new block of events only
// when all three link FIFOs are empty, so one silent FEB (or one corrupt word
// count) stalls the whole ROC until it is reset.
//
// This is one snapshot, meant for a ROC that stopped sending data (after a run
// crashed, before it is reset): while data is flowing the buffers are
// legitimately non-empty and the findings mean nothing.
// Lines starting with "PROBLEM:" / "WARN:" are parsed by the CRV page.
//
// All reads are non-destructive unless "peek head words" is set: that reads
// the first word (the event word count) of every non-empty FIFO, which REMOVES
// the word and breaks the event framing. Only use it on a ROC that is already
// stuck and will be reset afterwards.
void ROCCosmicRayVetoInterface::BufferStatus(__ARGS__)
{
	bool peek = __GET_ARG_IN__(
	    "peek head words, DESTRUCTIVE: only on a stuck ROC (Default: false)",
	    bool,
	    false);

	std::vector<std::string> problems, warnings;
	int                      readFailures = 0;

	auto rd = [this, &readFailures](uint16_t address) -> uint16_t {
		try
		{
			return this->readRegister(address);
		}
		catch(...)
		{
			++readFailures;
			return 0;
		}
	};
	auto hex = [](uint32_t v, int width = 4) {
		std::stringstream s;
		s << "0x" << std::hex << std::setfill('0') << std::setw(width) << v;
		return s.str();
	};
	// Tables are "label | cell | cell ..." rows; the CRV page turns every block
	// of such rows into an HTML table, so keep that shape.
	auto row = [](const std::string& label, const std::vector<std::string>& cells) {
		std::stringstream s;
		s << std::left << std::setw(16) << label << std::right;
		for(const auto& c : cells)
			s << " | " << std::setw(10) << c;
		s << std::endl;
		return s.str();
	};

	// ---- FPGA1 ------------------------------------------------------------
	uint16_t linkWords[3];
	for(int i = 0; i < 3; ++i)
		linkWords[i] = rd(ROC::LinkWdCnt0 + i) & 0x3fff;
	uint16_t linkStat = rd(ROC::CRS);         // bits 6:4 full, 2:0 empty
	uint16_t evStat   = rd(ROC::EvBuffStat);  // bit 1 full, bit 0 empty
	uint16_t cr       = rd(ROC::CR);
	uint32_t activeReg =
	    (static_cast<uint32_t>(rd(ROC::ActivePortsHigh)) << 16) | rd(ROC::ActivePortsLow);
	uint16_t sendGR    = rd(ROC::sendGR) & 0x1;
	uint16_t linkErr   = rd(ROC::LinkErrors);
	uint16_t drTimeout = rd(ROC::DRTimeout);
	uint16_t lastUb    = rd(ROC::LastUbSent);
	uint16_t ub[7];
	for(int i = 0; i < 7; ++i)
		ub[i] = rd(ROC::EvBuildUbDebug0 + i);
	// uBDebug = mismatch & uB(link 0) & uB(link 1) & uB(link 2)
	uint32_t ubLink[3]  = {(static_cast<uint32_t>(ub[5]) << 16) | ub[4],
	                       (static_cast<uint32_t>(ub[3]) << 16) | ub[2],
	                       (static_cast<uint32_t>(ub[1]) << 16) | ub[0]};
	bool     ubMismatch = ub[6] & 0x1;
	bool     formHold   = (cr >> 2) & 0x1;
	bool     drHold     = (cr >> 11) & 0x1;
	bool     evFull     = (evStat >> 1) & 1;
	bool     evNotEmpty = !(evStat & 1);

	std::stringstream out;
	out << "FPGA1: active " << hex(activeReg, 6) << ", CR " << hex(cr) << " (FormHold "
	    << formHold << ", DRHold " << drHold << "), sendGR " << sendGR << ", DR timeout "
	    << hex(drTimeout) << ", last EW " << hex(lastUb) << std::endl;
	out << "FPGA1: event buffer "
	    << (evFull       ? "FULL"
	        : evNotEmpty ? "not empty"
	                     : "empty")
	    << ", fiber CRC errors " << (linkErr >> 8) << ", fiber loss of sync "
	    << (linkErr & 0xf) << ", last event uB " << (ubMismatch ? "MISMATCH" : "matched")
	    << ", firmware " << hex(rd(ROC::Version)) << std::endl;

	// The run start writes 0xffff (off) to the ROC timeout. With any other value
	// FPGA1 gives up on a request after that time and answers the DTC without
	// data (DTC link status 0xa0, 16 byte blocks). Below about 100 us that is
	// before the FEB data arrives, so every event is lost (run 125872, 0x0fff).
	if(drTimeout != 0xffff)
	{
		std::stringstream ss;
		ss << "the ROC timeout (register 0x5D) reads " << hex(drTimeout)
		   << ", the run start writes 0xffff (off): FPGA1 answers a request without data "
		      "after "
		   << std::fixed << std::setprecision(1) << drTimeout * 0.0064
		   << " us. Something wrote this register; if the value is real, no events come "
		      "from this ROC until the next run start";
		warnings.push_back(ss.str());
	}

	// EWT match logic and its counters, FPGA1 firmware version 0xa7 and later:
	// an event is only sent with the EWT it was requested with.
	uint16_t fwVersion = rd(ROC::Version);
	if(fwVersion >= 0xa7)
	{
		uint16_t reqDropped = rd(ROC::EwtReqDropped);
		uint16_t evDropped  = rd(ROC::EwtEvDropped);
		uint16_t pending    = rd(ROC::EwtPending);  // 15:8 built events, 7:0 EWT words
		uint16_t state      = rd(ROC::EwtState);
		uint16_t ctrl       = rd(ROC::EwtCtrl);  // bit 0 drop late, bit 1 bypass matching
		uint16_t dropEvTag  = rd(ROC::EwtLastDropEvent);
		uint16_t dropReq    = rd(ROC::EwtLastDropRequest);
		uint16_t reqRx      = rd(ROC::DReqRxCnt);
		uint16_t evBuilt    = rd(ROC::EvBuiltCnt);
		// In the firmware 0x82 is the LOW and 0x83 the HIGH word of the header
		// counter; the enum names DRCntHigh (0x82) / DRCnLow (0x83) are swapped.
		uint32_t hdrSent =
		    (static_cast<uint32_t>(rd(ROC::DRCnLow)) << 16) | rd(ROC::DRCntHigh);
		out << "FPGA1: requests received " << reqRx << ", events built " << evBuilt
		    << " (16 bit counters), headers sent " << hdrSent << std::endl;
		out << "FPGA1: requests dropped " << reqDropped << ", events dropped "
		    << evDropped << ", last drop: event " << hex(dropEvTag) << " request "
		    << hex(dropReq) << ", matching " << ((ctrl & 0x2) ? "BYPASSED" : "on")
		    << ", drop late " << ((ctrl & 0x1) ? "on" : "off") << std::endl;
		out << "FPGA1: built events waiting " << (pending >> 8) << ", pending requests "
		    << ((pending & 0xff) / 3) << " (" << (pending & 0xff)
		    << " EWT words), states: builder " << ((state >> 8) & 0xf)
		    << ", packet former " << (((state >> 7) & 1) ? "idle" : "busy") << " ("
		    << ((state >> 4) & 0x7) << "), request handler " << (state & 0xf)
		    << std::endl;
		if(reqDropped)
			warnings.push_back(std::to_string(reqDropped) +
			                   " data requests got no event (dropped by the EWT match)");
		if(evDropped)
			warnings.push_back(std::to_string(evDropped) +
			                   " built events were dropped by the EWT match (late or no "
			                   "matching request), last one: event " +
			                   hex(dropEvTag) + " against request " + hex(dropReq));
		if(ctrl & 0x2)
			warnings.push_back("EWT matching is bypassed (register 0x93 bit 1)");
		if((pending & 0xff) >= 0xf0)
			warnings.push_back(
			    "the request EWT FIFO is full: requests are being lost and "
			    "the FIFO may be out of step, reset before the next run");
		if(fwVersion >= 0xa8 && !(ctrl & 0x1))
			warnings.push_back("drop late is off (register 0x93 bit 0)");
	}
	// Input FIFO flush and its counters, FPGA1 firmware version 0xa8 and later:
	// the input FIFOs are reset when they hold data no pending request can have
	// produced (bad tag, stale data) and after a truncated event.
	if(fwVersion >= 0xa8)
	{
		static const char* builderNames[16] = {"idle",
		                                       "waiting for parts",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "building",
		                                       "fake event",
		                                       "flushing"};
		uint16_t           ctrl             = rd(ROC::EwtCtrl);
		uint16_t           resets           = rd(ROC::LinkFifoRstCnt);
		uint16_t           badTag           = rd(ROC::BadTagCnt);
		uint16_t           stale            = rd(ROC::StaleDataCnt);
		uint16_t           trunc            = rd(ROC::TruncEvCnt);
		uint16_t           snap             = rd(ROC::FlushSnapStat);
		out << "FPGA1: input FIFO resets " << resets << ", bad tags " << badTag
		    << ", stale data " << stale << ", truncated events " << trunc
		    << " (a8 counters stop at 65535), flush " << ((ctrl & 0x4) ? "on" : "OFF")
		    << ", bad-tag guard " << ((ctrl & 0x8) ? "on" : "OFF") << std::endl;
		std::string emptyLinks;
		if(snap & 0x3)
		{
			out << "FPGA1: last flush trigger: " << ((snap & 0x1) ? "bad tag" : "")
			    << ((snap & 0x3) == 0x3 ? " and " : "")
			    << ((snap & 0x2) ? "stale data" : "") << ", flush was "
			    << ((snap & 0x4) ? "on" : "off") << ", builder "
			    << builderNames[(snap >> 4) & 0xf] << " (" << ((snap >> 4) & 0xf)
			    << "), request words " << (snap >> 12) << "; input FIFOs then:";
			for(int i = 0; i < 3; ++i)
			{
				bool empty = (snap >> (8 + i)) & 1;
				out << " " << i << ": " << rd(ROC::FlushSnapWords0 + i)
				    << " words, first word " << hex(rd(ROC::FlushSnapHead0 + i))
				    << (empty ? " (empty)" : "") << (i < 2 ? "," : "");
				if(empty)
					emptyLinks += (emptyLinks.empty() ? "" : ", ") + std::to_string(i);
			}
			out << std::endl;
		}
		if(badTag)
			warnings.push_back(
			    std::to_string(badTag) +
			    " events had a tag newer than the only pending request (bad "
			    "tag): the FPGA1 input FIFOs were out of step and were flushed");
		if(stale)
			warnings.push_back(
			    std::to_string(stale) +
			    " requests found leftover data in the FPGA1 input FIFOs (stale "
			    "data) and flushed them" +
			    (emptyLinks.empty() ? std::string()
			                        : "; at the last one input FIFO " + emptyLinks +
			                              " was empty (that data FPGA sent nothing)"));
		if(trunc)
			warnings.push_back(std::to_string(trunc) +
			                   " events were truncated at the size limit");
		if(resets != 0xffff && badTag != 0xffff && stale != 0xffff && trunc != 0xffff &&
		   static_cast<unsigned>(resets) > static_cast<unsigned>(trunc) + badTag + stale)
			warnings.push_back(
			    "more input FIFO resets (" + std::to_string(resets) +
			    ") than truncated events, bad tags and stale data together: "
			    "this should not happen, report it");
		if(!(ctrl & 0x4))
			warnings.push_back(
			    "the input FIFO flush is off (register 0x93 bit 2): bad tags "
			    "and stale data are only counted");
		if(!(ctrl & 0x8))
			warnings.push_back("the bad-tag guard is off (register 0x93 bit 3)");
	}
	{
		// Requests stored for the uC (9 words each); the uC takes each one out
		// to send the request to the FEBs, so normally 0 or 1 request waits here.
		uint16_t drBuf   = rd(ROC::DreqBuffStat);  // bit 15 empty, 9:0 words
		unsigned drWords = drBuf & 0x3ff;
		out << "FPGA1: requests waiting for the uC " << (drWords / 9) << " (" << drWords
		    << " words)" << std::endl;
		if(drWords >= 27)
			warnings.push_back(std::to_string(drWords / 9) +
			                   " data requests wait for the uC: the uC is not reading "
			                   "them (stopped or too slow), so the FEBs get no request");
	}
	{
		// Heartbeats from the fibre wait here (9 words each) until FPGA1 has sent
		// the event window number on to the FEBs. FPGA1 takes them out in blocks
		// of 9 by counting, so words left over (1 to 8) mean the blocks are out
		// of step: the FEBs then get wrong event window numbers and flag every
		// answer (run 125853). Only a run start reset clears it.
		uint16_t hbBuf   = rd(ROC::HrtBtBuffStat);  // bit 15 empty, 9:0 words
		unsigned hbWords = hbBuf & 0x3ff;
		out << "FPGA1: heartbeat buffer " << hbWords << " words, " << rd(ROC::HeartBeatCn)
		    << " heartbeats sent to the FEBs (16 bit counter), last event window sent "
		    << hex(lastUb) << std::endl;
		if(hbWords % 9 != 0)
			warnings.push_back(
			    std::to_string(hbWords) +
			    " words in the FPGA1 heartbeat buffer (not a multiple of 9): "
			    "heartbeats are read out of step, the FEBs get wrong event "
			    "window numbers. Stop and start the run again");
		// The number last sent to the FEBs must follow the last event built: the
		// heartbeat of a window comes before its request, and after a stop only a
		// few more heartbeats arrive. ubLink holds the EWT with its halves swapped,
		// so the upper half is the low word of the EWT.
		uint32_t lastEvent = ubLink[0] ? ubLink[0] : ubLink[1] ? ubLink[1] : ubLink[2];
		if(lastEvent != 0)
		{
			uint16_t evLow = lastEvent >> 16;
			uint16_t ahead = lastUb - evLow;  // 16 bit difference
			if(ahead > 0x4000)
				warnings.push_back(
				    "the event window number last sent to the FEBs (low word " +
				    hex(lastUb) + ") does not follow the last event built (low word " +
				    hex(evLow) +
				    "): FPGA1 sends wrong event window numbers, the FEBs flag "
				    "every event and send no hits. Stop and start the run again");
		}
	}
	out << std::endl;

	bool                     linkActive[3];
	bool                     anyLinkHolds = false, allActiveLinksHold = true;
	int                      activeLinks = 0;
	std::vector<std::string> cPorts, cWords, cState, cUb;
	for(int i = 0; i < 3; ++i)
	{
		linkActive[i] = ((activeReg >> (8 * i)) & 0xff) != 0;
		bool full     = (linkStat >> (4 + i)) & 1;
		cPorts.push_back(std::to_string(8 * i + 1) + "-" + std::to_string(8 * i + 8));
		cWords.push_back(std::to_string(linkWords[i]));
		cState.push_back(full             ? "FULL"
		                 : !linkActive[i] ? "unused"
		                 : linkWords[i]   ? "data"
		                                  : "empty");
		cUb.push_back(hex(ubLink[i], 8));
		if(full)
			problems.push_back("FPGA1 input FIFO " + std::to_string(i) +
			                   " (from data FPGA " + std::to_string(i) + ") is full");
		if(linkActive[i])
		{
			++activeLinks;
			if(linkWords[i] > 0)
				anyLinkHolds = true;
			else
				allActiveLinksHold = false;
		}
		else if(linkWords[i] > 0)
			warnings.push_back("FPGA1 input FIFO " + std::to_string(i) +
			                   " holds data but its data FPGA has no active port");
	}
	out << row("FPGA1 input FIFO", {"0", "1", "2"}) << row("ports", cPorts)
	    << row("words", cWords) << row("state", cState) << row("last event uB", cUb)
	    << std::endl;

	for(int i = 0; i < 3; ++i)
		if(linkActive[i] && linkWords[i] == 0 && anyLinkHolds)
			problems.push_back(
			    "FPGA1 input FIFO " + std::to_string(i) +
			    " is empty while the other two hold data: FPGA1 cannot build "
			    "the event, it waits for data FPGA " +
			    std::to_string(i) + " (ports " + std::to_string(8 * i + 1) + "-" +
			    std::to_string(8 * i + 8) + ")");
	if(activeLinks > 0 && allActiveLinksHold && sendGR == 0)
		problems.push_back(
		    "FPGA1 has data from every data FPGA in its input FIFOs but does not build "
		    "the event: hold data but nothing is read. Most likely one FIFO starts with "
		    "a wrong word count (run Buffer Dump and compare 'head word count' with the "
		    "words in the FIFO)");
	if(evFull)
		problems.push_back("FPGA1 event buffer is full");
	else if(evNotEmpty)
		warnings.push_back(
		    "FPGA1 event buffer holds a built event that was not sent (no data "
		    "request?)");
	if(formHold)
		warnings.push_back("FormHold is set: the event builder is held");
	if(drHold)
		warnings.push_back("DRHold is set: data requests are not handled");
	if(ubMismatch)
		warnings.push_back(
		    "last built event had different uB numbers on the links "
		    "(data FPGAs out of sync)");

	// ---- data FPGAs -------------------------------------------------------
	std::vector<std::string> dPorts, dActive, dMask, dCrc, dRxErr, dWr, dRd, dWrPtr,
	    dRdPtr, dBacklog, dDdr, dTx, dTxFull, dTrunc, dVersion, dMatch, dTimeout, dPmTime,
	    dPmCounts, dPmOpen, dPmOldest, dPmTx, dPmDrained, dPmMissing, dPmTimedOut,
	    dPmListDrops;
	std::stringstream portTables;
	for(int n = 0; n < 3; ++n)
	{
		uint16_t base     = ROC::Data[n];
		uint16_t rxActive = rd(base | ROC::Data_RxActive) & 0xff;
		uint16_t mask     = rd(base | ROC::Data_InputMask) & 0xff;
		uint16_t crcErr   = rd(base | ROC::Data_CRCErr) & 0xff;
		uint16_t rxErr    = rd(base | ROC::Data_RxErr) & 0xff;
		uint16_t ddrStat  = rd(base | ROC::Data_DDRStat);
		uint16_t truncCnt = rd(base | ROC::Data_TxOverflowCnt);
		uint16_t fsm      = rd(base | ROC::Data_DDRFsmStat);
		uint16_t linkTx =
		    rd(base | ROC::Data_LinkCtrl);  // 15:8 writes while full, 5 full, 4 empty
		uint32_t wrPtr =
		    (static_cast<uint32_t>(rd(base | ROC::Data_DDR_WriteHigh)) << 16) |
		    rd(base | ROC::Data_DDR_WriteLow);
		uint32_t rdPtr =
		    (static_cast<uint32_t>(rd(base | ROC::Data_DDRRdPtrHigh)) << 16) |
		    rd(base | ROC::Data_DDRRdPtrLow);
		// The write pointer stops at the end of the last event written (padded to
		// 32 bytes) and is zeroed when the DDR write is enabled at configure; the
		// read pointer stops at the end of the last event sent to FPGA1 and keeps
		// its old value until the first event of a run is read. So a difference
		// of 32 bytes or more is data that was written but never sent.
		bool        ddrWritten = wrPtr != 0;
		uint32_t    ddrBacklog = (wrPtr - rdPtr) & 0x3fffffff;  // bytes, 30 bit pointers
		std::string backlog    = !ddrWritten       ? "no data"
		                         : ddrBacklog < 32 ? "none"
		                         : ddrBacklog >= 0x20000000
		                             ? "rd > wr"
		                             : "WAIT " + std::to_string(ddrBacklog / 2);
		int         wrState    = fsm >> 12;  // 0 = idle
		int         rdState    = fsm & 0x7;  // 0 = idle
		bool        txFull     = (linkTx >> 5) & 1;
		bool        txEmpty    = (linkTx >> 4) & 1;
		std::string name       = "data FPGA " + std::to_string(n);

		dPorts.push_back(std::to_string(8 * n + 1) + "-" + std::to_string(8 * n + 8));
		dActive.push_back(hex(rxActive, 2));
		dMask.push_back(hex(mask, 2));
		dCrc.push_back(hex(crcErr, 2));
		dRxErr.push_back(hex(rxErr, 2));
		// With no active port the write state machine bounces between idle and
		// state 2 (it finds "all active ports ready" with nothing to copy).
		if(rxActive == 0)
			wrState = 0;
		dWr.push_back(rxActive == 0 ? "unused"
		              : wrState     ? "BUSY " + std::to_string(wrState)
		                            : "idle");
		dRd.push_back(rdState ? "BUSY " + std::to_string(rdState) : "idle");
		dWrPtr.push_back(hex(wrPtr, 8));
		dRdPtr.push_back(hex(rdPtr, 8));
		dBacklog.push_back(backlog);
		dDdr.push_back(((ddrStat >> 1) & 1) ? "cal done" : "NO CAL");
		dTx.push_back(txFull ? "FULL" : txEmpty ? "empty" : "data");
		dTxFull.push_back(std::to_string(linkTx >> 8));
		dTrunc.push_back(std::to_string(truncCnt));

		// Port match ("one event part per request, in order"), FPGA2 version 0x0012 and
		// later
		uint16_t dataVersion = rd(base | ROC::Data_Version);
		dVersion.push_back(hex(dataVersion));
		if(dataVersion >= 0x12)
		{
			uint16_t pmCsr     = rd(base | ROC::Data_PortMatchCtrl);
			uint16_t timeout   = rd(base | ROC::Data_PortTimeout);
			uint16_t lastTime  = rd(base | ROC::Data_PMLastTime);
			uint16_t maxTime   = rd(base | ROC::Data_PMMaxTime);
			uint16_t pmEvents  = rd(base | ROC::Data_PMEvCnt);
			uint16_t pmReqs    = rd(base | ROC::Data_PMReqCnt);
			uint16_t drained   = rd(base | ROC::Data_PMDrainCnt);
			uint16_t missing   = rd(base | ROC::Data_PMMissCnt);
			uint16_t timedOut  = rd(base | ROC::Data_PMTimeoutCnt);
			uint16_t listDrops = rd(base | ROC::Data_PMListDropCnt);
			uint16_t pmPorts   = rd(base | ROC::Data_PMPorts);
			uint32_t badEwt =
			    (static_cast<uint32_t>(rd(base | ROC::Data_PMBadEwtHigh)) << 16) |
			    rd(base | ROC::Data_PMBadEwtLow);
			uint16_t badPort = rd(base | ROC::Data_PMBadPort);
			uint32_t oldest =
			    (static_cast<uint32_t>(rd(base | ROC::Data_PMOldestHigh)) << 16) |
			    rd(base | ROC::Data_PMOldestLow);
			uint16_t txWord0 = rd(base | ROC::Data_PMTxWord0);
			bool     matchOn = pmCsr & 0x1;

			dMatch.push_back(matchOn ? "on" : "off");
			dTimeout.push_back(timeout ? std::to_string(timeout * 10) + " us" : "none");
			dPmTime.push_back(std::to_string(lastTime * 10) + " / " +
			                  std::to_string(maxTime * 10));
			dPmCounts.push_back(std::to_string(pmReqs) + " / " +
			                    std::to_string(pmEvents));
			dPmOpen.push_back(std::to_string((pmCsr >> 4) & 0x7));
			dPmOldest.push_back(hex(oldest, 8));
			dPmTx.push_back(hex(txWord0));
			dPmDrained.push_back(std::to_string(drained));
			dPmMissing.push_back(std::to_string(missing));
			dPmTimedOut.push_back(std::to_string(timedOut));
			dPmListDrops.push_back(std::to_string(listDrops));

			if(matchOn && timeout == 0 && rxActive != 0)
				warnings.push_back(
				    name +
				    ": port match is on without a timeout (register 0x82): "
				    "a FEB that stops answering blocks this data FPGA");
			if(missing)
				warnings.push_back(
				    name + ": " + std::to_string(missing) +
				    " events were sent with missing FEBs (" + std::to_string(timedOut) +
				    " of them by the timeout); ports missing in the last one: mask " +
				    hex(pmPorts & 0xff, 2));
			if(drained)
				warnings.push_back(
				    name + ": " + std::to_string(drained) +
				    " times a port buffer was emptied because its EWT was not an open "
				    "request (ports mask " +
				    hex(pmPorts >> 8, 2) + "; last: port " +
				    std::to_string(8 * n + ((badPort >> 8) & 0x7) + 1) + ", EWT " +
				    hex(badEwt, 8) + ")");
			if(listDrops)
				warnings.push_back(
				    name + ": " + std::to_string(listDrops) +
				    " requests were dropped from the full list of open requests "
				    "(more than four requests without an event)");
			if(badPort & 0xff)
				warnings.push_back(name +
				                   ": port buffers are being emptied now (ports mask " +
				                   hex(badPort & 0xff, 2) + ")");
		}
		else
		{
			for(auto* v : {&dMatch,
			               &dTimeout,
			               &dPmTime,
			               &dPmCounts,
			               &dPmOpen,
			               &dPmOldest,
			               &dPmTx,
			               &dPmDrained,
			               &dPmMissing,
			               &dPmTimedOut,
			               &dPmListDrops})
				v->push_back("-");
		}

		if(static_cast<uint32_t>(rxActive) != ((activeReg >> (8 * n)) & 0xff))
			warnings.push_back(name + ": Rx_active " + hex(rxActive, 2) +
			                   " differs from the FPGA1 active register " +
			                   hex((activeReg >> (8 * n)) & 0xff, 2));
		if(!((ddrStat >> 1) & 1))
			problems.push_back(name + ": DDR calibration not done");
		if(txFull)
			problems.push_back(name + ": output FIFO to FPGA1 is full");
		else if(!txEmpty)
			warnings.push_back(name + ": output FIFO to FPGA1 is not empty");
		if(linkTx >> 8)
			warnings.push_back(
			    name + ": " + std::to_string(linkTx >> 8) +
			    " writes to a full output FIFO to FPGA1 since the last ROC reset "
			    "(data lost)");
		if(truncCnt)
			warnings.push_back(name + ": " + std::to_string(truncCnt) +
			                   " events truncated to the maximum size since the last ROC "
			                   "reset");
		if(ddrWritten && ddrBacklog >= 32 && ddrBacklog < 0x20000000)
			warnings.push_back(name + ": " + std::to_string(ddrBacklog / 2) +
			                   " words in the DDR were not sent to FPGA1 (it sends only "
			                   "when all three FPGA1 input FIFOs are empty)");
		if(wrState != 0)
			warnings.push_back(name + ": DDR write state machine is not idle (state " +
			                   std::to_string(wrState) + ")");
		if(rdState != 0)
			warnings.push_back(name + ": DDR read state machine is not idle (state " +
			                   std::to_string(rdState) + ")");

		uint16_t words[8], received[8];
		bool     anyHolds = false, allActiveHold = rxActive != 0;
		for(int p = 0; p < 8; ++p)
		{
			words[p]    = rd(base | (ROC::Data_PhyRxWdUsed + p)) & 0x1fff;
			received[p] = rd(base | (ROC::Data_PhyActivityCnt + p));
			if(!((rxActive >> p) & 1))
				continue;
			if(words[p] > 0)
				anyHolds = true;
			else
				allActiveHold = false;
		}
		std::vector<std::string> pPort, pWords, pRecv, pFlags;
		for(int p = 0; p < 8; ++p)
		{
			int         port   = 8 * n + p + 1;
			bool        active = (rxActive >> p) & 1;
			bool        crc    = (crcErr >> p) & 1;
			std::string pn     = "port " + std::to_string(port);
			pPort.push_back(std::to_string(port));
			pWords.push_back(std::to_string(words[p]));
			pRecv.push_back(std::to_string(received[p]));
			pFlags.push_back(std::string(!active ? "off"
			                             : crc   ? "CRC"
			                                     : "-") +
			                 (((mask >> p) & 1) ? "" : " masked"));
			if(!active)
			{
				if(words[p] > 0)
					warnings.push_back(pn +
					                   " is inactive but its input buffer holds data");
				continue;
			}
			if(words[p] == 0 && anyHolds)
				problems.push_back(
				    pn + " input buffer is empty while other ports of " + name +
				    " hold data: no answer from this FEB to the last request, the data "
				    "FPGA waits for it");
			if(words[p] >= 0x1000)
				problems.push_back(pn + " input buffer is full");
			if(crc)
				warnings.push_back(pn +
				                   " CRC error flag set (sticky, cleared at run start)");
		}
		portTables << row("port", pPort) << row("buffer words", pWords)
		           << row("words received", pRecv) << row("flags", pFlags) << std::endl;
		if(allActiveHold && wrState == 0)
			problems.push_back(
			    name +
			    ": all active ports hold data but no event is ready: a head "
			    "word count is larger than the data in its buffer (corrupt "
			    "word count or truncated event, check with peek)");
	}
	out << row("data FPGA", {"0", "1", "2"}) << row("ports", dPorts)
	    << row("Rx_active", dActive) << row("input mask", dMask) << row("CRC error", dCrc)
	    << row("PHY RxErr", dRxErr) << row("DDR write FSM", dWr)
	    << row("DDR read FSM", dRd) << row("DDR write ptr", dWrPtr)
	    << row("DDR read ptr", dRdPtr) << row("DDR unsent wds", dBacklog)
	    << row("DDR", dDdr) << row("output FIFO to FPGA1", dTx)
	    << row("Tx full writes", dTxFull) << row("truncated evts", dTrunc)
	    << row("firmware", dVersion) << row("port match", dMatch)
	    << row("FEB timeout", dTimeout) << row("answer us last/max", dPmTime)
	    << row("requests / events", dPmCounts) << row("open requests", dPmOpen)
	    << row("oldest open EWT", dPmOldest) << row("last FEB packet", dPmTx)
	    << row("buffers emptied", dPmDrained) << row("evts missing FEBs", dPmMissing)
	    << row("evts by timeout", dPmTimedOut) << row("list overflows", dPmListDrops)
	    << std::endl;
	out << "Input buffers per port (words received is a 16 bit counter, it wraps)"
	    << std::endl
	    << portTables.str();

	// ---- optional destructive read of the head word counts ------------------
	if(peek)
	{
		out << "Head words (DESTRUCTIVE read, event framing is now broken: reset the ROC)"
		    << std::endl;
		for(int n = 0; n < 3; ++n)
			for(int p = 0; p < 8; ++p)
			{
				uint16_t words = rd(ROC::Data[n] | (ROC::Data_PhyRxWdUsed + p)) & 0x1fff;
				if(words == 0)
					continue;
				uint16_t head = rd(ROC::Data[n] | (ROC::Data_PhyRxRead + p));
				int      port = 8 * n + p + 1;
				out << "  port " << std::setw(2) << port << " head word count "
				    << hex(head) << ", words in buffer " << hex(words) << std::endl;
				if(head > 0x1001)
					problems.push_back("port " + std::to_string(port) +
					                   " head word count " + hex(head) +
					                   " can never fit the input buffer (corrupt)");
				else if(head > words)
					problems.push_back("port " + std::to_string(port) +
					                   " head word count " + hex(head) + " but only " +
					                   hex(words) +
					                   " words in the buffer (incomplete event)");
			}
		for(int i = 0; i < 3; ++i)
		{
			uint16_t words = rd(ROC::LinkWdCnt0 + i) & 0x3fff;
			if(words == 0)
				continue;
			uint16_t head = rd(ROC::LinkFifoRead + i);
			out << "  FPGA1 input FIFO " << i << " head word " << hex(head)
			    << " (word count " << hex(head & 0x1fff) << "), words in FIFO "
			    << hex(words) << std::endl;
			if((head & 0x1fff) > words)
				problems.push_back("FPGA1 input FIFO " + std::to_string(i) +
				                   " head word count " + hex(head & 0x1fff) +
				                   " but only " + hex(words) +
				                   " words in the FIFO (incomplete event)");
		}
	}

	if(readFailures)
		problems.push_back(std::to_string(readFailures) +
		                   " register reads failed (shown as 0)");

	std::stringstream response;
	if(problems.empty() && warnings.empty())
		response << "OK: nothing stuck" << std::endl;
	for(const auto& p : problems)
		response << "PROBLEM: " << p << std::endl;
	for(const auto& w : warnings)
		response << "WARN: " << w << std::endl;
	response << std::endl << out.str();
	__SET_ARG_OUT__("response", response.str());
}

//==============================================================================
// Dump the content of every non-empty link FIFO (FPGA1) and port input buffer
// (data FPGAs) of a stuck ROC, to see what is left in them: a complete event,
// or a fragment (e.g. the tail of a truncated event).
//
// DESTRUCTIVE: every word read is removed from its buffer, so the ROC has to be
// reset afterwards. Run "ROC Buffer Status" first, it does not change anything.
//
// An event starts with its word count (which includes the count itself),
// followed by status, uB high, uB low and the data. In a link FIFO the count is
// in bits 12:0 of the first word. The dump marks the word where the next event
// would start if the first word is a valid count.
void ROCCosmicRayVetoInterface::BufferDump(__ARGS__)
{
	int maxWords = __GET_ARG_IN__("max words per buffer (Default: 512)", int, 512);
	if(maxWords < 1)
		maxWords = 1;

	std::stringstream out;
	int               dumped = 0;

	// countMask: bits of the head word that hold the event word count
	auto dump = [this, &out, &dumped, maxWords](const std::string& name,
	                                            uint16_t           countAddress,
	                                            uint16_t           countRegMask,
	                                            uint16_t           dataAddress,
	                                            uint16_t           countMask) {
		int words = this->readRegister(countAddress) & countRegMask;
		if(words == 0)
			return;
		++dumped;
		int n = words < maxWords ? words : maxWords;
		out << name << ": " << words << " words";
		if(n < words)
			out << " (first " << n << " read, the rest is still in the buffer)";
		out << std::endl;
		int nextEvent = 0;  // index where the next event should start
		for(int i = 0; i < n; ++i)
		{
			uint16_t v = this->readRegister(dataAddress);
			if(i == 0)
			{
				int count = v & countMask;
				out << "  head word count " << std::dec << count
				    << (count == words ? " = buffer content: one complete event"
				        : count < words
				            ? " < buffer content: more than one event, or a fragment"
				            : " > buffer content: incomplete event or fragment")
				    << std::endl;
				nextEvent = count;
			}
			if(i % 8 == 0)
				out << "  " << std::dec << std::setfill(' ') << std::setw(5) << i << ":";
			bool start = i > 0 && i == nextEvent;
			if(start)
				nextEvent += (v & countMask) ? (v & countMask) : 0x10000;  // 0 would loop
			out << (start ? " >" : "  ") << std::hex << std::setfill('0') << std::setw(4)
			    << v;
			if(i % 8 == 7 || i == n - 1)
				out << std::endl;
		}
		out << std::dec << std::setfill(' ');
	};

	for(int i = 0; i < 3; ++i)
		dump("FPGA1 input FIFO " + std::to_string(i) + " (from data FPGA " +
		         std::to_string(i) + ")",
		     ROC::LinkWdCnt0 + i,
		     0x3fff,
		     ROC::LinkFifoRead + i,
		     0x1fff);
	for(int n = 0; n < 3; ++n)
		for(int p = 0; p < 8; ++p)
			dump("Port " + std::to_string(8 * n + p + 1) + " input buffer",
			     ROC::Data[n] | (ROC::Data_PhyRxWdUsed + p),
			     0x1fff,
			     ROC::Data[n] | (ROC::Data_PhyRxRead + p),
			     0xffff);

	std::stringstream response;
	if(dumped == 0)
		response << "All FPGA1 input FIFOs and port input buffers are empty."
		         << std::endl;
	else
		response << "The words below were REMOVED from the buffers: reset the ROC before "
		            "the next run. \">\" marks where the next event would start."
		         << std::endl
		         << out.str();
	__SET_ARG_OUT__("response", response.str());
}

//==============================================================================
// Read the data request trace buffer of FPGA1 (0x86): a copy of every word
// FPGA1 put into the request FIFO that the uC reads to send the uB requests to
// the FEBs. It keeps the last ~1000 words (~112 requests, 9 words each:
// byte count, packet type (low byte 0x20), EWT low, EWT mid, EWT high,
// 3 reserved, CRC) and is cleared by the fiber receiver reset at configure and
// run start, so read it after a crash and before the next start.
//
// Use: when a ROC skipped a data request (DTC timeout, then EWT off by one),
// this shows whether FPGA1 handed that request to the uC as 9 good words. A
// request with a word missing or added shifts all later ones.
// Reading empties the trace buffer but does not touch the data path.
void ROCCosmicRayVetoInterface::RequestTrace(__ARGS__)
{
	int nPrint = __GET_ARG_IN__("requests to print in full (Default: 16)", int, 16);

	const int             nWords = 1017;  // trace holds up to 0x3F0 words, plus margin
	std::vector<uint16_t> w(nWords);
	for(int i = 0; i < nWords; ++i)
		w[i] = this->readRegister(ROC::DReqBuffTrace);

	// Once the buffer is empty the same word repeats: cut the tail.
	int n = nWords;
	while(n > 1 && w[n - 1] == w[n - 2])
		--n;

	// Find the 9-word alignment: the one where most rows have packet type xx20.
	int bestOffset = 0, bestGood = -1;
	for(int offset = 0; offset < 9; ++offset)
	{
		int good = 0;
		for(int i = offset; i + 9 <= n; i += 9)
			if((w[i + 1] & 0xff) == 0x20)
				++good;
		if(good > bestGood)
		{
			bestGood   = good;
			bestOffset = offset;
		}
	}

	std::stringstream out, problems;
	int               rows      = (n - bestOffset) / 9;
	int               nProblems = 0;
	bool              havePrev  = false;
	uint64_t          prevEwt = 0, firstEwt = 0, lastEwt = 0;
	for(int r = 0; r < rows; ++r)
	{
		const uint16_t* p   = &w[bestOffset + 9 * r];
		uint64_t        ewt = p[2] | (static_cast<uint64_t>(p[3]) << 16) |
		               (static_cast<uint64_t>(p[4]) << 32);
		bool typeOk = (p[1] & 0xff) == 0x20;
		bool seqOk  = !havePrev || ewt == prevEwt + 1;
		if(!typeOk || !seqOk)
		{
			++nProblems;
			problems << "PROBLEM: request " << r << " of " << rows << ": "
			         << (!typeOk ? "packet type word is not xx20 (word missing or added)"
			                     : "EWT does not follow the previous one")
			         << ", EWT 0x" << std::hex << ewt << ", previous 0x" << prevEwt
			         << std::dec << std::endl;
		}
		if(!havePrev)
			firstEwt = ewt;
		lastEwt  = ewt;
		prevEwt  = ewt;
		havePrev = true;
		if(r >= rows - nPrint || !typeOk || !seqOk)
		{
			out << std::setfill(' ') << std::setw(4) << std::dec << r << ": EWT 0x"
			    << std::hex << std::setfill('0') << std::setw(8) << ewt << " |";
			for(int k = 0; k < 9; ++k)
				out << " " << std::setw(4) << p[k];
			out << (typeOk && seqOk ? "" : "  <--") << std::dec << std::endl;
		}
	}

	std::stringstream response;
	if(rows == 0)
		response << "No complete request in the trace buffer (" << n << " words read)."
		         << std::endl;
	else
	{
		if(nProblems == 0)
			response << "OK: " << rows << " requests, all 9 words and consecutive"
			         << std::endl;
		response << problems.str() << std::endl;
		response << rows << " requests in the trace, EWT 0x" << std::hex << firstEwt
		         << " to 0x" << lastEwt << std::dec << " (" << n << " words, "
		         << bestOffset << " skipped to align, " << (n - bestOffset - 9 * rows)
		         << " left over at the end)" << std::endl;
		response << "Requests as written to the uC FIFO (byte count, type, EWT low, mid, "
		            "high, 3 reserved, CRC):"
		         << std::endl
		         << out.str();
	}
	__SET_ARG_OUT__("response", response.str());
}

//==============================================================================
// Clear the FEB error counters (held in FEB FRAM) via the uC function MDIO with
// value FEBII::MDIOClearErrorCounters. requestedPort -1 (default, as in the
// other FEB II macros) is one broadcast write to FEBII::MDIOBroadcast (0xB109,
// "LCB MDIO 5" to every FEB on the ROC); requestedPort > 0 selects that port
// and writes FEBII::MDIO (0x9109, "LC MDIO 5"). Used by the "FEB II Reset Error
// Counters" macro and by start().
// Does not throw; returns true on success and puts the result text in report.
bool ROCCosmicRayVetoInterface::resetFebErrorCounters(int          requestedPort,
                                                      std::string* report)
{
	std::stringstream response;
	bool              ok = true;

	if(requestedPort > 0)
	{
		response << "Reset FEB Error Counters on port " << requestedPort << " (write "
		         << FEBII::MDIOClearErrorCounters << " to MDIO 0x" << std::hex
		         << FEBII::MDIO << std::dec << "): ";
		try
		{
			SetActivePort(static_cast<uint16_t>(requestedPort), true);
			this->writeRegister(FEBII::MDIO, FEBII::MDIOClearErrorCounters);
			response << "OK" << std::endl;
		}
		catch(...)
		{
			ok = false;
			response << "FAILED" << std::endl;
		}
	}
	else
	{
		response << "Reset FEB Error Counters on all FEBs (broadcast write "
		         << FEBII::MDIOClearErrorCounters << " to MDIO 0x" << std::hex
		         << FEBII::MDIOBroadcast << std::dec << "): ";
		try
		{
			this->writeRegister(FEBII::MDIOBroadcast, FEBII::MDIOClearErrorCounters);
			response << "OK" << std::endl;
		}
		catch(...)
		{
			ok = false;
			response << "FAILED" << std::endl;
		}
	}

	if(report)
		*report = response.str();
	return ok;
}

// Pulse FEB II control register (0x000) bit 7 on all four FPGAs to clear the
// latching FM encoded serial stream parity error (status 0x001 bit 7).
// requestedPort > 0 selects one port, otherwise the FEB broadcast address is used.
bool ROCCosmicRayVetoInterface::clearFebParityError(int          requestedPort,
                                                    std::string* report)
{
	std::stringstream response;
	bool              ok    = true;
	uint16_t          PORT_ = ROC::FEB;

	if(requestedPort > 0)
	{
		response << "Clear FEB parity error on port " << requestedPort << ": ";
		try
		{
			SetActivePort(static_cast<uint16_t>(requestedPort), true);
		}
		catch(...)
		{
			response << "FAILED (could not select port)" << std::endl;
			if(report)
				*report = response.str();
			return false;
		}
	}
	else
	{
		PORT_ = PORT_ | ROC::FEB_Broadcast;
		response << "Clear FEB parity error on all FEBs (broadcast): ";
	}

	try
	{
		for(int i = 0; i < 4; i++)
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x80);
		usleep(1000);
		for(int i = 0; i < 4; i++)
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x0);
		response << "OK" << std::endl;
	}
	catch(...)
	{
		ok = false;
		response << "FAILED" << std::endl;
	}

	if(report)
		*report = response.str();
	return ok;
}

void ROCCosmicRayVetoInterface::FebIIResetErrorCounters(__ARGS__)
{
	int requestedPort =
	    __GET_ARG_IN__("port (Default: -1, all active ports via broadcast)", int, -1);

	std::string report, parityReport;
	resetFebErrorCounters(requestedPort, &report);
	clearFebParityError(requestedPort, &parityReport);
	__SET_ARG_OUT__("response", report + parityReport);
}

void ROCCosmicRayVetoInterface::TestFebConnection(__ARGS__)
{
	uint16_t reg =
	    __GET_ARG_IN__("register (Default: 0x1023 = CntLO)", uint16_t, FEBII::CntLO);
	int requestedPort =
	    __GET_ARG_IN__("port (Default: 0 = all active, >0 = single)", int, 0);

	std::stringstream response;
	uint32_t          active = GetActivePorts();

	response << "Testing FEB readback for register 0x" << std::hex << std::setw(4)
	         << std::setfill('0') << reg << std::dec << std::setfill(' ') << std::endl;
	response << "Active ports mask: 0x" << std::hex << std::setw(6) << std::setfill('0')
	         << active << std::dec << std::setfill(' ') << std::endl;

	auto testPort = [this, reg, &response](uint16_t port) {
		response << "Port " << port << ":" << std::endl;
		try
		{
			SetActivePort(port, true);
		}
		catch(...)
		{
			response << "  failed to select port" << std::endl;
			return;
		}

		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			try
			{
				uint16_t value = this->readRegister(FEBII::FPGA[fpga] | reg);
				response << "  fpga " << fpga << ": 0x" << std::hex << std::setw(4)
				         << std::setfill('0') << value << std::dec << std::setfill(' ')
				         << std::endl;
			}
			catch(...)
			{
				response << "  fpga " << fpga << ": read failed" << std::endl;
			}
		}
	};

	if(requestedPort > 0)
	{
		testPort(static_cast<uint16_t>(requestedPort));
	}
	else
	{
		bool testedAnyPort = false;
		for(uint16_t port = 1; port <= 24; ++port)
		{
			if(active & (0x00000001u << (port - 1)))
			{
				testedAnyPort = true;
				testPort(port);
			}
		}

		if(!testedAnyPort)
			response << "No active ports found." << std::endl;
	}

	__SET_ARG_OUT__("response", response.str());
}
bool ROCCosmicRayVetoInterface::testRocLinks(std::string* response, bool logFailures)
{
	bool              allOk  = true;
	uint32_t          active = GetActivePorts();
	std::stringstream report;

	report << "FPGA0 CntLO readback per active port\n";
	report << "Active ports mask: 0x" << std::hex << std::setw(6) << std::setfill('0')
	       << active << std::dec << std::setfill(' ') << "\n";

	bool testedAnyPort = false;
	for(uint16_t port = 1; port <= 24; ++port)
	{
		if(!(active & (0x00000001u << (port - 1))))
			continue;

		testedAnyPort = true;
		// Up to maxAttempts reads, 1 s apart. The port is selected again on every
		// attempt: the first failure may have been the port selection itself.
		const int maxAttempts = 5;
		bool      portOk      = false;
		for(int attempt = 1; attempt <= maxAttempts && !portOk; ++attempt)
		{
			if(attempt > 1)
				sleep(1);
			try
			{
				SetActivePort(port, true);
				uint16_t val = this->readRegister(FEBII::FPGA[0] | FEBII::CntLO);
				report << "  port " << port << ": 0x" << std::hex << std::setw(4)
				       << std::setfill('0') << val << std::dec << std::setfill(' ');
				if(attempt > 1)
					report << " (answered on attempt " << attempt << ", after "
					       << (attempt - 1) << " s)";
				report << "\n";
				portOk = true;
			}
			catch(...)
			{
				if(logFailures)
				{
					__FE_COUT_WARN__ << "testRocLinks: port " << port
					                 << " did not respond on attempt " << attempt
					                 << " of " << maxAttempts << __E__;
				}
			}
		}
		if(!portOk)
		{
			report << "  port " << port << ": read failed on all " << maxAttempts
			       << " attempts\n";
			allOk = false;
		}
	}

	if(!testedAnyPort)
		report << "No active ports found.\n";

	report << (allOk ? "Result: PASS" : "Result: FAIL") << "\n";
	if(response)
		*response = report.str();
	return allOk;
}

void ROCCosmicRayVetoInterface::TestRocLinks(__ARGS__)
{
	std::string response;
	bool        allOk = testRocLinks(&response, false);
	__SET_ARG_OUT__("result", allOk ? "true" : "false");
	__SET_ARG_OUT__("response", response);
}  // end TestRocLinks()

void ROCCosmicRayVetoInterface::FebIISetChannel(__ARGS__)
{
	int      port    = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t fpga    = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t channel = __GET_ARG_IN__("channel [0-15], -1 all (Default)", uint16_t, -1);
	bool     fake    = __GET_ARG_IN__("fake (Default: false)", bool, false);
	bool     off     = __GET_ARG_IN__("off (Default: false)", bool, false);
	bool     use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	const bool     all_ports     = (port <= 0);
	const bool     use_broadcast = all_ports && use_broadcast_for_all;
	const uint32_t active        = GetActivePorts();

	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;

	auto setChannelRegisters = [&](uint16_t PORT_) {
		uint16_t fpga_start = (fpga == uint16_t(-1)) ? 0 : fpga;
		uint16_t fpga_end   = (fpga == uint16_t(-1)) ? 4 : fpga + 1;
		for(uint16_t f = fpga_start; f < fpga_end; ++f)
		{
			uint16_t ch_start = (channel == uint16_t(-1)) ? 0 : channel;
			uint16_t ch_end   = (channel == uint16_t(-1)) ? 16 : channel + 1;
			for(uint16_t ch = ch_start; ch < ch_end; ++ch)
			{
				uint16_t val = fake ? 0x10 : (off ? 0x11 : ch);
				this->writeRegister(
				    PORT_ | FEBII::FPGA[f] | (FEBII::ChannelMapBase + (ch & 0xF)), val);
			}
		}
	};

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			setChannelRegisters(ROC::FEB);
		}
		ostr << "Set channels on each active port";
	}
	else
	{
		uint16_t PORT_ = ROC::FEB;
		if(use_broadcast)
		{
			PORT_ = PORT_ | ROC::FEB_Broadcast;
			ostr << "Set channels using broadcast";
		}
		else
			ostr << "Set channels";
		setChannelRegisters(PORT_);
	}

	ostr << " to " << (fake ? "0x10 (fake mode)" : (off ? "0x11 (off)" : "ch-idx")) << "."
	     << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetAFEOffset(__ARGS__)
{
	int port          = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	int offset_signed = __GET_ARG_IN__("offset (in range [-512, 511])", int, 0);
	if(port > 0)
		SetActivePort(port);

	// limit to 10 bit
	if(offset_signed < -512)
		offset_signed = -512;
	if(offset_signed > 511)
		offset_signed = 511;
	std::stringstream ostr;

	uint16_t offset = static_cast<uint16_t>(offset_signed) & 0x3FF;
	for(uint16_t fpga = 0; fpga < 4; ++fpga)
	{
		for(uint16_t afe = 0; afe < 2; ++afe)
		{
			uint16_t afe_base = (afe == 0) ? FEBII::AFE0_base : FEBII::AFE1_base;
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch1, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch2, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch3, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch4, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch5, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch6, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch7, offset);
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_ch8, offset);

			uint16_t val = ReadAFE(fpga, afe, FEBII::Offset_en);
			val          = val | 0x0100;  // enable
			this->writeRegister(FEBII::FPGA[fpga] | afe_base | FEBII::Offset_en, val);
		}
	}
	ostr << "Set AFE offsets to " << offset_signed << " ( 0x" << std::hex << offset << ")"
	     << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

uint16_t ROCCosmicRayVetoInterface::ReadAFE(uint16_t fpga, uint16_t afe_no, uint16_t reg)
{
	if(afe_no > 1)
		return -1;
	auto afe_base = (afe_no == 0) ? FEBII::AFE0_base : FEBII::AFE1_base;
	// enable read mode
	this->writeRegister(FEBII::FPGA[fpga] | afe_base, 2);
	// dummy write to the register that we want to read
	this->writeRegister(FEBII::FPGA[fpga] | afe_base | reg, 0);
	// read
	uint16_t val = this->readRegister(FEBII::FPGA[fpga] | afe_base | reg);
	// back to write mode
	this->writeRegister(FEBII::FPGA[fpga] | afe_base, 0);
	return val;
}

void ROCCosmicRayVetoInterface::FebIIGetBaselines(__ARGS__)
{
	int  port   = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	bool update = __GET_ARG_IN__("re-measure (Default: false)", bool, false);
	if(port > 0)
		SetActivePort(port);

	if(update)
	{
		for(unsigned int fpga = 0; fpga < 4; fpga++)
		{
			for(uint16_t ch = 0; ch < 16; ++ch)
			{
				this->writeRegister(
				    FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xF)), 0x1);
			}
		}
		usleep(100000);  // wait 100ms for measurement to complete
	}

	std::stringstream ostr;
	bool              not_first = false;
	for(uint16_t fpga = 0; fpga < 4; ++fpga)
	{
		for(uint16_t ch = 0; ch < 16; ++ch)
		{
			if(not_first)
			{
				ostr << ", ";
			}
			not_first = true;
			ostr << std::dec
			     << this->readRegister(FEBII::FPGA[fpga] | (FEBII::BaselineBase + ch));
		}
	}
	ostr << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIIGetCurrents(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;
	ostr << "Currents for port " << port << std::endl;
	for(unsigned int fpga = 0; fpga < 4; ++fpga)
	{
		this->writeRegister(FEBII::MUX, fpga);  // set mux
		usleep(1000000);

		for(uint16_t ch = 0; ch < 16; ++ch)
		{
			this->writeRegister(FEBII::CR | FEBII::FPGA[fpga] | 0x20,
			                    0x10 | ch);  // enable mux
			this->writeRegister(FEBII::GAIN, 8);
			this->writeRegister(FEBII::A0,
			                    3);  // makes three A0 reads and calculates the average
			usleep(1000000);
			uint16_t avgInt =
			    this->readRegister(FEBII::A0);  // returns 1,000,000 * average
			float avg = avgInt / 1.0e6;
			if(avg > 4.096)
				avg = 8.192 - avg;
			float current = avg / 8.0 * 250.0;  // in uA
			ostr << "Channel " << fpga * 16 + ch << ":  " << std::fixed
			     << std::setprecision(3) << current << " uA" << std::endl;
		}

		this->writeRegister(FEBII::CR | FEBII::FPGA[fpga] | 0x20,
		                    0x0);  // disable mux (required before data taking)
	}

	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIITrigBaselines(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	int ch   = __GET_ARG_IN__("channel [0-63] (Default: -1, all)", int, -1);
	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;
	ostr << "Trigger baseline measurement ";

	if(ch < 0)
	{
		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			for(uint16_t ch_ = 0; ch_ < 16; ++ch_)
			{
				this->writeRegister(
				    FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch_ & 0xf)), 0x1);
			}
		}
		ostr << "for all channels." << std::endl;
	}
	else
	{
		uint16_t fpga = ch % 16;
		this->writeRegister(FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xf)), 0x1);
		ostr << "for channel " << ch << "(fpga " << fpga << ", ch " << (ch & 0xf) << ")."
		     << std::endl;
	}
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::ResetPLL(int  sleep_ms,
                                         bool allPorts,
                                         bool runRocClockAlign,
                                         bool checkStatus)
{
	uint16_t PORT_ = ROC::FEB;
	if(allPorts)
		PORT_ = ROC::FEB_Broadcast;

	// Quiet ROC marker-encoded traffic while we manipulate clocking.
	// bool restore_marker_sync = false;
	// if(runRocClockAlign)
	//{
	//	restore_marker_sync = ((this->readRegister(ROC::CR) & (1u << 5)) != 0);
	//	SetMarkerSync(false);
	//}

	// 1) Frequency-lock VCXO to ROC clock.
	this->writeRegister(PORT_ | FEBII::EWTFakeMode, 0x1);  // fake mode, Ph_det off
	usleep(sleep_ms * 1000);
	this->writeRegister(PORT_ | FEBII::EWTFakeMode,
	                    0x0);  // back to external, Ph_det enabled again
	usleep(sleep_ms * 1000);

	if(runRocClockAlign)
	{
		// 2) Pulse FPGA0 bit9 in CR to initiate ROC clock MMCM alignment.
		for(int i = 0; i < 4; i++)
		{
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x200);
		}
		usleep(1000);
		for(int i = 0; i < 4; i++)
		{
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x0);
		}
		usleep(1000);

		// 3) Confirm MMCM/PLL lock+alignment from FPGA0 status bits 9,5,4.
		if(checkStatus && !allPorts)
		{
			for(int i = 0; i < 4; i++)
			{
				const uint16_t status =
				    this->readRegister(ROC::FEB | FEBII::FPGA[i] | FEBII::Status);
				const uint16_t needed = (1u << 9) | (1u << 5) | (1u << 4);
				if((status & needed) != needed)
				{
					__COUT_WARN__ << "ResetPLL ROC clock align check for FPGA " << i
					              << " failed: status=0x" << std::hex << status
					              << " (need bits 9,5,4 set)" << __E__;
				}
			}
		}

		// 5) force AFE data alignment/calibration on FPGA0
		for(int i = 0; i < 4; i++)
		{
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x4);
		}
		usleep(1000);
		for(int i = 0; i < 4; i++)
		{
			this->writeRegister(PORT_ | FEBII::FPGA[i] | FEBII::CR, 0x0);
		}
		usleep(100000);

		// 6) Confirm AFE alignment from status bits 3,2 (set) and 1,0 (low).
		if(checkStatus && !allPorts)
		{
			for(int i = 0; i < 4; i++)
			{
				const uint16_t afeStatus =
				    this->readRegister(ROC::FEB | FEBII::FPGA[i] | FEBII::Status);
				const uint16_t mask     = (1u << 3) | (1u << 2) | (1u << 1) | (1u << 0);
				const uint16_t expected = (1u << 3) | (1u << 2);
				if((afeStatus & mask) != expected)
				{
					__COUT_WARN__ << "ResetPLL AFE align check failed on FPGA " << i
					              << ": status=0x" << std::hex << afeStatus
					              << " (need bits 3,2 set and bits 1,0 low)" << __E__;
				}
			}
		}

		// if(restore_marker_sync)
		//	SetMarkerSync(true);
	}

	if(!allPorts)
		waitForFebResponsive();
}

int16_t ROCCosmicRayVetoInterface::Realign(int sleep_uc)
{
	int16_t out = 0;
	for(unsigned int fpga = 0; fpga < 4; fpga++)
	{
		this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR, 0x4);  // force AFE realignment
		usleep(sleep_uc);
		this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR, 0x0);  // reset AFE realignment
		usleep(sleep_uc);
		uint16_t status = this->readRegister(FEBII::FPGA[fpga] | FEBII::Status);
		// ostr << "FPGA " << fpga << ": status=0x" << std::hex << status << std::endl;
		if(status & 0x200)
		{
			this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR,
			                    0x204);  // force AFE realignment, invert 80MHz phase
			usleep(sleep_uc);
			this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR,
			                    0x200);  // reset AFE realignment
			usleep(sleep_uc);
			out |= (0x1 << fpga);
			// status = this->readRegister(FEBII::FPGA[fpga] | FEBII::Status);
			// ostr << "FPGA " << fpga << " - inverted phase: status=0x" << std::hex <<
			// status << std::endl;
		}  // else {
		   // ostr << "FPGA " << fpga << " - default phase: status=0x" << std::hex <<
		   // status << std::endl;
		//}
	}
	return out;
}

void ROCCosmicRayVetoInterface::SetInputMask(__ARGS__)
{
	int mask = __GET_ARG_IN__("mask (Default: 0xffffff)", int, 0xffffff);
	for(int i = 0; i < 3; i++)
	{
		this->writeRegister(ROC::Data[i] | ROC::Data_InputMask, (mask >> (8 * i)) & 0xff);
	}
	std::stringstream ostr;
	ostr << "Set input mask to 0x" << std::hex << mask << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIIConfigure(__ARGS__)
{
	int  port = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	bool use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast for all ports (Default: true)", bool, true);
	bool run_roc_clock_align =
	    __GET_ARG_IN__("run ROC clock align sequence (Default: true)", bool, true);
	uint16_t bias        = __GET_ARG_IN__("bias (Default: 0xaaf)", uint16_t, 0xaaf);
	bool     skip_bias   = __GET_ARG_IN__("skip bias (Default: false)", bool, false);
	uint16_t threshold   = __GET_ARG_IN__("threshold (Default: 0x50)", uint16_t, 0x50);
	bool update_baseline = __GET_ARG_IN__("update baseline (Default: true)", bool, true);
	uint16_t onSpillGateEnd =
	    __GET_ARG_IN__("onSpillGateEnd (Default: 255, 6.25ns)", uint16_t, 255);
	uint16_t offSpillGateEnd =
	    __GET_ARG_IN__("offSpillGateEnd (Default: 15000, 6.25ns)", uint16_t, 15000);
	bool pll_reset = __GET_ARG_IN__("pll reset (Default: true)", bool, true);

	std::stringstream ostr;
	ostr << std::endl;
	const bool all_ports     = (port <= 0);
	const bool use_broadcast = all_ports && use_broadcast_for_all;
	ostr << "ROC clock align sequence: " << (run_roc_clock_align ? "enabled" : "disabled")
	     << std::endl;
	if(port > 0)
	{
		ostr << "Selecting port " << port << std::endl;
		SetActivePort(port);
	}

	uint16_t PORT_ = ROC::FEB;
	if(use_broadcast)
	{
		PORT_ = PORT_ | ROC::FEB_Broadcast;
		ostr << "Broadcast to all ports" << std::endl;
	}
	else if(all_ports)
	{
		ostr << "Configuring all active ports one-by-one (broadcast disabled)"
		     << std::endl;
	}

	usleep(100000);
	const uint32_t active = GetActivePorts();

	// Reset PLL
	if(pll_reset)
	{
		ostr << "Resetting PLL (wait 1s)" << std::endl;
		if(!(all_ports && !use_broadcast))
		{
			ResetPLL(1000, all_ports, run_roc_clock_align);
		}
	}

	// bias
	if(!skip_bias)
	{
		ostr << "Ramping all bias to " << std::hex << bias << " with spacing of 5s";
		if(all_ports && !use_broadcast)
			ostr << " (per active port)";
		ostr << std::endl;

		if(all_ports && !use_broadcast)
		{
			for(uint16_t p = 1; p <= 24; ++p)
			{
				if(!(active & (0x00000001 << (p - 1))))
					continue;
				SetActivePort(p);
				this->writeRegister(FEBII::PortAll, p);
				for(uint16_t fpga = 0; fpga < 4; ++fpga)
				{
					for(uint16_t idx = 0; idx < 2; ++idx)
					{
						this->writeRegister(ROC::FEB | FEBII::FPGA[fpga] |
						                        (FEBII::BiasBase + (idx & 0x1)),
						                    bias);
						// sleep(5);
					}
				}
			}
		}
		else
		{
			for(uint16_t fpga = 0; fpga < 4; ++fpga)
			{
				for(uint16_t idx = 0; idx < 2; ++idx)
				{
					this->writeRegister(
					    PORT_ | FEBII::FPGA[fpga] | (FEBII::BiasBase + (idx & 0x1)),
					    bias);
					// sleep(5);
				}
			}
		}
	}

	// things that can not be done as broadcast
	if(!(all_ports && !use_broadcast))
	{
		for(uint16_t p = 1; p <= 24; p++)
		{
			if((port > 0) && (p != port))
				continue;

			if(active & (0x00000001 << (p - 1)))
			{
				SetActivePort(p, false);
				// set port
				this->writeRegister(FEBII::PortAll, p);
			}

			// forcer AFE to realign

			/*
			uint16_t status = Realign(1000);
			ostr << "Forcing AFE realignment: " << ((status & 0x1) ? "inverted" :
			"default")
			    << ", " << ((status & 0x2) ? "inverted" : "default") << ", "
			    << ((status & 0x4) ? "inverted" : "default") << ", "
			    << ((status & 0x8) ? "inverted" : "default") << std::endl;
			*/

			if(!all_ports)
				break;  // only doing a single selected port
		}
	}

	// and back to broadcastable
	// Enable channels, and thresholds, trigger baseline
	ostr << "Enabling channels, and thresholds";
	if(update_baseline)
		ostr << ", and triggering baselines";
	if(all_ports && !use_broadcast)
		ostr << " (per active port)";
	ostr << std::endl;

	if(all_ports && !use_broadcast)
	{
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			this->writeRegister(FEBII::PortAll, p);
			if(pll_reset)
				ResetPLL(1000, false, run_roc_clock_align);
			for(unsigned int fpga = 0; fpga < 4; ++fpga)
			{
				for(uint16_t ch = 0; ch < 16; ++ch)
				{
					this->writeRegister(ROC::FEB | FEBII::FPGA[fpga] |
					                        (FEBII::ChannelMapBase + (ch & 0xF)),
					                    ch);
					this->writeRegister(ROC::FEB | FEBII::FPGA[fpga] |
					                        (FEBII::ThresholdBase + (ch & 0xF)),
					                    threshold);
					if(update_baseline)
					{
						this->writeRegister(ROC::FEB | FEBII::FPGA[fpga] |
						                        (FEBII::BaselineBase + (ch & 0xF)),
						                    0x1);
					}
				}
			}
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOffOnSpill),
			                    onSpillGateEnd);
			this->writeRegister(static_cast<uint16_t>(ROC::FEB) |
			                        static_cast<uint16_t>(FEBII::GateOffOffSpill),
			                    offSpillGateEnd);
		}
		ostr << "Setting on-spill gate end to " << onSpillGateEnd
		     << "(6.25ns) and off-spill gate end to " << offSpillGateEnd
		     << "(6.25ns) on each active port" << std::endl;
	}
	else
	{
		for(unsigned int fpga = 0; fpga < 4; fpga++)
		{
			for(uint16_t ch = 0; ch < 16; ++ch)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (ch & 0xF)), ch);
				this->writeRegister(
				    PORT_ | FEBII::FPGA[fpga] | (FEBII::ThresholdBase + (ch & 0xF)),
				    threshold);
				if(update_baseline)
				{
					this->writeRegister(
					    PORT_ | FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xF)),
					    0x1);
				}
			}
		}

		ostr << "Setting on-spill gate end to " << onSpillGateEnd
		     << "(6.25ns) and off-spill gate end to " << offSpillGateEnd << "(6.25ns)"
		     << std::endl;
		this->writeRegister(PORT_ | FEBII::GateOffOnSpill, onSpillGateEnd);
		this->writeRegister(PORT_ | FEBII::GateOffOffSpill, offSpillGateEnd);
	}

	__SET_ARG_OUT__("response", ostr.str());
}

//==========================================================================================
uint16_t ROCCosmicRayVetoInterface::readRegisterWithRetry(uint16_t address,
                                                          int      maxRetries,
                                                          int      retryInterval_ms)
{
	for(int attempt = 0; attempt <= maxRetries; ++attempt)
	{
		try
		{
			return this->readRegister(address);
		}
		catch(...)
		{
			if(attempt == maxRetries)
				throw;
			usleep(retryInterval_ms * 1000);
		}
	}
	return 0;  // unreachable
}

//==========================================================================================
// verifyDcsReadWrite
//	DCS self-test on ROC::TestCounter (0x35): read it, write (value + 5), read back.
//	The firmware increments this register on every read, so the read-back is accepted
//	as either the written value or written+1 (not specified whether the increment
//	happens before or after the value is returned). Without the write the counter
//	would sit at value+1, so either accepted read-back is unambiguous evidence the
//	write landed.
//	Throws std::runtime_error (RocConfigure DCS warning text) on any failure.
void ROCCosmicRayVetoInterface::verifyDcsReadWrite()
{
	auto fail = [this](const std::string& details) {
		const std::string msg =
		    makeRocConfigureDcsWarning(details + " Failing Configure.");
		__FE_COUT_WARN__ << msg << __E__;
		TLOG(TLVL_WARNING) << msg << __E__;
		throw std::runtime_error(msg);
	};

	uint16_t before = 0, written = 0, readback = 0;
	try
	{
		before  = this->readRegister(ROC::TestCounter);
		written = (uint16_t)(before + 5);
		this->writeRegister(ROC::TestCounter, written);
		readback = this->readRegister(ROC::TestCounter);
	}
	catch(const std::exception& e)
	{
		fail(std::string("TestCounter DCS self-test threw. Exception: ") + e.what() +
		     ".");
	}
	catch(...)
	{
		fail("TestCounter DCS self-test threw. Unknown exception.");
	}

	if(readback != written && readback != (uint16_t)(written + 1))
	{
		std::stringstream ss;
		ss << "TestCounter write did not take: read 0x" << std::hex << before
		   << ", wrote 0x" << written << ", read back 0x" << readback << std::dec << ".";
		fail(ss.str());
	}

	TLOG(TLVL_ROCConfig) << "DCS self-test OK: TestCounter read 0x" << std::hex << before
	                     << ", wrote 0x" << written << ", read back 0x" << readback
	                     << std::dec << __E__;
}

//==========================================================================================
bool ROCCosmicRayVetoInterface::waitForFebResponsive(int maxRetries, int retryInterval_ms)
{
	try
	{
		readRegisterWithRetry(
		    FEBII::FPGA[0] | FEBII::Status, maxRetries, retryInterval_ms);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

//==========================================================================================
std::string ROCCosmicRayVetoInterface::febIIConfigureFromTables(
    int portFilter, bool skipBias, bool biasOnly, int biasOverwrite, bool skipReadbacks)
{
	std::stringstream ostr;
	ostr << std::endl;

	auto t0_configure = std::chrono::steady_clock::now();
	auto elapsed_ms   = [&t0_configure]() -> long {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - t0_configure)
            .count();
	};

	auto febs =
	    getSelfNode().getNode("ROCTypeLinkTable").getNode("FEBsLink").getChildren();

	ostr << "Found " << febs.size() << " FEB(s) in FEBsLink table" << std::endl;

	// --- Broadcast bias overwrite (all ports at once, before per-FEB config) ---
	if(biasOverwrite >= 0)
	{
		uint16_t bias = static_cast<uint16_t>(biasOverwrite);
		ostr << "[" << elapsed_ms() << " ms] Bias overwrite via broadcast: 0x" << std::hex
		     << bias << std::dec << std::endl;
		uint16_t PORT_ = ROC::FEB | ROC::FEB_Broadcast;
		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			for(uint16_t idx = 0; idx < 2; ++idx)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[fpga] | (FEBII::BiasBase + (idx & 0x1)), bias);
				// sleep(5);
			}
		}
		ostr << "[" << elapsed_ms() << " ms] Bias overwrite complete." << std::endl;
	}

	unsigned int activeFebCount = 0;
	for(const auto& feb : febs)
	{
		bool active = feb.second.getNode("Status").getValue<bool>();
		if(!active)
			continue;

		uint16_t p = feb.second.getNode("Port").getValue<uint16_t>();
		if(portFilter > 0 && p != portFilter)
			continue;
		++activeFebCount;
		ostr << "[" << elapsed_ms() << " ms] Configuring FEB II '" << feb.first
		     << "' on port " << p << std::endl;
		SetActivePort(p);
		this->writeRegister(FEBII::PortAll, p);

		// --- Bias (BitMap 1x8: fpga*2 + idx) ---
		// Pass skipBias=true to skip entirely (e.g. bias already ramped).
		{
			auto bmp = feb.second.getNode("Bias").getValueAsBitMap<uint16_t>();
			if(skipBias || biasOverwrite >= 0)
			{
				ostr << "  Bias: skipped ("
				     << (biasOverwrite >= 0 ? "broadcast overwrite active"
				                            : "skipBias=true")
				     << ")" << std::endl;
			}
			else if(bmp.numberOfRows() > 0 && bmp.numberOfColumns(0) == 8)
			{
				ostr << "  [" << elapsed_ms() << " ms] Bias: ramping..." << std::endl;
				for(uint16_t fpga = 0; fpga < 4; ++fpga)
				{
					for(uint16_t idx = 0; idx < 2; ++idx)
					{
						uint16_t val = bmp.get(0, fpga * 2 + idx);
						this->writeRegister(FEBII::FPGA[fpga] | (FEBII::BiasBase + idx),
						                    val);
						// sleep(5);
						ostr << "    FPGA" << fpga << "[" << idx << "]  set=0x"
						     << std::hex << val;
						if(!skipReadbacks)
						{
							uint16_t rb = readRegisterWithRetry(FEBII::FPGA[fpga] |
							                                    (FEBII::BiasBase + idx));
							ostr << "  readback=0x" << rb;
						}
						ostr << std::dec << std::endl;
					}
				}
			}
			else
			{
				ostr << "  Bias: not configured in table, skipping" << std::endl;
			}
		}

		if(!biasOnly)
		{
			// --- Trim (BitMap 1x64: fpga*16 + ch) ---
			{
				auto bmp = feb.second.getNode("Trim").getValueAsBitMap<uint16_t>();
				if(bmp.numberOfRows() > 0 && bmp.numberOfColumns(0) == 64)
				{
					ostr << "  [" << elapsed_ms() << " ms] Trim:" << std::endl;
					for(uint16_t fpga = 0; fpga < 4; ++fpga)
					{
						ostr << "    FPGA" << fpga << ": ";
						for(uint16_t ch = 0; ch < 16; ++ch)
						{
							uint16_t val = bmp.get(0, fpga * 16 + ch);
							this->writeRegister(
							    FEBII::FPGA[fpga] | (FEBII::TrimBase + ch), val);
							ostr << "ch" << std::dec << ch << "=0x" << std::hex << val;
							if(!skipReadbacks)
							{
								uint16_t rb = readRegisterWithRetry(
								    FEBII::FPGA[fpga] | (FEBII::TrimBase + ch));
								ostr << "/0x" << rb;
							}
							ostr << " ";
						}
						ostr << std::dec << std::endl;
					}
				}
				else
				{
					ostr << "  Trim: not configured in table, skipping" << std::endl;
				}
			}

			// --- Threshold (BitMap 1x64: fpga*16 + ch) ---
			{
				auto bmp = feb.second.getNode("Threshold").getValueAsBitMap<uint16_t>();
				if(bmp.numberOfRows() > 0 && bmp.numberOfColumns(0) == 64)
				{
					ostr << "  [" << elapsed_ms() << " ms] Threshold:" << std::endl;
					for(uint16_t fpga = 0; fpga < 4; ++fpga)
					{
						ostr << "    FPGA" << fpga << ": ";
						for(uint16_t ch = 0; ch < 16; ++ch)
						{
							uint16_t val = bmp.get(0, fpga * 16 + ch);
							this->writeRegister(
							    FEBII::FPGA[fpga] | (FEBII::ThresholdBase + ch), val);
							ostr << "ch" << std::dec << ch << "=0x" << std::hex << val;
							if(!skipReadbacks)
							{
								uint16_t rb = readRegisterWithRetry(
								    FEBII::FPGA[fpga] | (FEBII::ThresholdBase + ch));
								ostr << "/0x" << rb;
							}
							ostr << " ";
						}
						ostr << std::dec << std::endl;
					}
				}
				else
				{
					ostr << "  Threshold: not configured in table, skipping" << std::endl;
				}
			}

			// --- Channel map & baseline trigger ---
			// When configuring all ports (portFilter <= 0), these are broadcast once
			// after the per-FEB loop.  For a single port, do them here.
			if(portFilter > 0)
			{
				ostr << "  [" << elapsed_ms() << " ms] ChannelMap:" << std::endl;
				for(uint16_t fpga = 0; fpga < 4; ++fpga)
				{
					for(uint16_t ch = 0; ch < 16; ++ch)
					{
						this->writeRegister(
						    FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (ch & 0xF)), ch);
					}
				}
				ostr << "    Set identity map for all 4 FPGAs x 16 channels" << std::endl;

				ostr << "  [" << elapsed_ms() << " ms] Baseline update:" << std::endl;
				for(uint16_t fpga = 0; fpga < 4; ++fpga)
				{
					for(uint16_t ch = 0; ch < 16; ++ch)
					{
						this->writeRegister(
						    FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xF)), 0x1);
					}
				}
				ostr << "    Triggered baseline update for all 4 FPGAs x 16 channels"
				     << std::endl;
			}

			// --- Gate settings from SettingsLink ---
			if(!feb.second.getNode("SettingsLink").isDisconnected())
			{
				auto     settings = feb.second.getNode("SettingsLink");
				uint16_t onStart  = settings.getNode("OnSpillStart").getValue<uint16_t>();
				uint16_t onEnd    = settings.getNode("OnSpillEnd").getValue<uint16_t>();
				uint16_t offStart =
				    settings.getNode("OffSpillStart").getValue<uint16_t>();
				uint16_t offEnd = settings.getNode("OffSpillEnd").getValue<uint16_t>();

				this->writeRegister(FEBII::GateOnOnSpill, onStart);
				this->writeRegister(FEBII::GateOffOnSpill, onEnd);
				this->writeRegister(FEBII::GateOnOffSpill, offStart);
				this->writeRegister(FEBII::GateOffOffSpill, offEnd);

				ostr << "  [" << elapsed_ms() << " ms] OnSpill  gate: start=" << onStart
				     << "  end=" << onEnd << "  [6.25ns]" << std::endl;
				ostr << "  OffSpill gate: start=" << offStart << "  end=" << offEnd
				     << "  [6.25ns]" << std::endl;
			}
			else
			{
				ostr << "  SettingsLink: disconnected, no gate settings applied"
				     << std::endl;
			}
		}  // end if(!biasOnly)

		if(!biasOnly && portFilter > 0)
		{
			ostr << "  [" << elapsed_ms() << " ms] PLL reset (wait 1s)..." << std::endl;
			ResetPLL(1000, false, true);
		}

		if(portFilter > 0)
		{
			ostr << "  [" << elapsed_ms() << " ms] Waiting for port " << p
			     << " to become responsive..." << std::endl;
			auto t0_wait    = std::chrono::steady_clock::now();
			bool responsive = waitForFebResponsive();
			auto wait_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - t0_wait)
			                   .count();
			if(responsive)
				ostr << "  [" << elapsed_ms() << " ms] Port " << p << " responsive after "
				     << wait_ms << " ms." << std::endl;
			else
				ostr << "  [" << elapsed_ms() << " ms] WARNING: port " << p
				     << " not responsive after " << wait_ms << " ms!" << std::endl;
		}
	}

	if(activeFebCount == 0)
		ostr << "WARNING: no active FEBs found in FEBsLink table!" << std::endl;

	// --- Broadcast channel map, baseline, PLL reset for all-port config ---
	if(portFilter <= 0 && !biasOnly && activeFebCount > 0)
	{
		uint16_t PORT_ = ROC::FEB | ROC::FEB_Broadcast;

		ostr << "[" << elapsed_ms()
		     << " ms] Broadcast ChannelMap (identity) to all ports..." << std::endl;
		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			for(uint16_t ch = 0; ch < 16; ++ch)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (ch & 0xF)), ch);
			}
		}

		ostr << "[" << elapsed_ms() << " ms] Broadcast baseline trigger to all ports..."
		     << std::endl;
		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			for(uint16_t ch = 0; ch < 16; ++ch)
			{
				this->writeRegister(
				    PORT_ | FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xF)), 0x1);
			}
		}

		ostr << "[" << elapsed_ms() << " ms] Broadcast PLL reset (wait 1s)..."
		     << std::endl;
		ResetPLL(1000, true, true);

		for(const auto& feb : febs)
		{
			if(!feb.second.getNode("Status").getValue<bool>())
				continue;
			uint16_t p = feb.second.getNode("Port").getValue<uint16_t>();
			SetActivePort(p);

			ostr << "  [" << elapsed_ms() << " ms] Waiting for port " << p
			     << " to become responsive..." << std::endl;
			auto t0_wait    = std::chrono::steady_clock::now();
			bool responsive = waitForFebResponsive();
			auto wait_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - t0_wait)
			                   .count();
			if(responsive)
				ostr << "  [" << elapsed_ms() << " ms] Port " << p << " responsive after "
				     << wait_ms << " ms." << std::endl;
			else
				ostr << "  [" << elapsed_ms() << " ms] WARNING: port " << p
				     << " not responsive after " << wait_ms << " ms!" << std::endl;
		}
	}

	ostr << "[" << elapsed_ms() << " ms] febIIConfigureFromTables complete." << std::endl;
	return ostr.str();
}

//==========================================================================================
void ROCCosmicRayVetoInterface::FebIIConfigureFromTables(__ARGS__)
{
	int  port          = __GET_ARG_IN__("port (Default: -1, all active)", int, -1);
	bool skipBias      = __GET_ARG_IN__("skip bias (Default: false)", bool, false);
	bool biasOnly      = __GET_ARG_IN__("bias only (Default: false)", bool, false);
	int  biasOverwrite = __GET_ARG_IN__(
        "bias overwrite broadcast (Default: -1, disabled; e.g. 0xa7c)", int, -1);
	bool skipReadbacks = __GET_ARG_IN__("skip readbacks (Default: true)", bool, true);
	std::string result;
	try
	{
		result = febIIConfigureFromTables(
		    port, skipBias, biasOnly, biasOverwrite, skipReadbacks);
	}
	catch(std::exception& e)
	{
		result = std::string("\nERROR: ") + e.what() + "\n";
	}
	__SET_ARG_OUT__("response", result);
}

//==========================================================================================
void ROCCosmicRayVetoInterface::PLLReset(__ARGS__)
{
	int  port     = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	int  sleep_ms = __GET_ARG_IN__("sleep [ms] (Default: 1000)", int, 1000);
	bool runRocClockAlign =
	    __GET_ARG_IN__("run ROC clock align sequence (Default: true)", bool, true);
	bool use_broadcast_for_all =
	    __GET_ARG_IN__("use broadcast when port<0 (Default: true)", bool, true);
	bool checkStatus = __GET_ARG_IN__("check status (Default: false)", bool, false);

	std::stringstream ostr;

	const bool allPorts = (port <= 0);
	ostr << "sleep=" << sleep_ms
	     << "ms, rocClockAlign=" << (runRocClockAlign ? "true" : "false")
	     << ", checkStatus=" << (checkStatus ? "true" : "false") << std::endl;

	if(allPorts && !use_broadcast_for_all)
	{
		const uint32_t active = GetActivePorts();
		for(uint16_t p = 1; p <= 24; ++p)
		{
			if(!(active & (0x00000001 << (p - 1))))
				continue;
			SetActivePort(p);
			ostr << "Resetting PLL on port " << std::dec << p << std::endl;
			ResetPLL(sleep_ms, false, runRocClockAlign, checkStatus);
		}
		ostr << "PLL Reset completed on each active port." << std::endl;
	}
	else
	{
		if(port > 0)
		{
			ostr << "Selecting port " << port << std::endl;
			SetActivePort(port);
		}
		else
		{
			ostr << "Resetting PLL on all ports (broadcast)" << std::endl;
		}

		ResetPLL(sleep_ms, allPorts, runRocClockAlign, checkStatus);
		ostr << "PLL Reset completed." << std::endl;
	}
	__SET_ARG_OUT__("response", ostr.str());
}

//==========================================================================================
void ROCCosmicRayVetoInterface::GetAlignScore(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, all active)", int, -1);

	std::stringstream ostr;
	const uint32_t    active = GetActivePorts();

	for(uint16_t p = 1; p <= 24; ++p)
	{
		if(port > 0 && p != port)
			continue;
		if(port < 0 && !(active & (0x00000001 << (p - 1))))
			continue;

		SetActivePort(p);
		uint16_t score = this->readRegister(FEBII::FPGA[0] | FEBII::AlignScore);
		ostr << "Port " << std::dec << p << ": AlignScore = 0x" << std::hex << score
		     << " (" << std::dec << score << ")" << std::endl;
	}
	__SET_ARG_OUT__("response", ostr.str());
}

// FEB II per-FPGA registers 0x6A-0x6F: clock align score, uptime, microbunch
// number (0x6B:0x6E) and microbunches since reset (0x6F), plus the main status
// register 0x001 (bit 9 clock align done, 7 FM parity error, 5 MMCM lock,
// 4 PLL lock, 3/2 AFE1/0 align done, 1/0 AFE1/0 align warning). One line per
// port and FPGA, parsed by the CRV web GUI.
void ROCCosmicRayVetoInterface::FebIIUptimeAndEWT(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, all active)", int, -1);

	std::stringstream ostr;
	const uint32_t    active = GetActivePorts();

	for(uint16_t p = 1; p <= 24; ++p)
	{
		if(port > 0 && p != port)
			continue;
		if(port < 0 && !(active & (0x00000001 << (p - 1))))
			continue;

		try
		{
			SetActivePort(p, true);
		}
		catch(...)
		{
			ostr << "Port " << std::dec << p << ": FAIL (could not select port)"
			     << std::endl;
			continue;
		}

		for(int n = 0; n < 4; ++n)
		{
			try
			{
				uint16_t align = this->readRegister(FEBII::FPGA[n] | FEBII::AlignScore);
				uint32_t uptime =
				    (this->readRegister(FEBII::FPGA[n] | FEBII::UptimeHI) << 16) |
				    this->readRegister(FEBII::FPGA[n] | FEBII::UptimeLo);
				uint32_t ub = (this->readRegister(FEBII::FPGA[n] | FEBII::EWTHI) << 16) |
				              this->readRegister(FEBII::FPGA[n] | FEBII::EWT);
				uint16_t ubCnt  = this->readRegister(FEBII::FPGA[n] | FEBII::EWTCount);
				uint16_t status = this->readRegister(FEBII::FPGA[n] | FEBII::Status);
				ostr << "Port " << std::dec << p << " FPGA " << n << ": align=0x"
				     << std::hex << align << " uptime=" << std::dec << uptime << " uB=0x"
				     << std::hex << std::setfill('0') << std::setw(8) << ub
				     << " uBcount=0x" << std::setw(4) << ubCnt << " status=0x"
				     << std::setw(4) << status << std::setfill(' ') << std::dec
				     << std::endl;
			}
			catch(...)
			{
				ostr << "Port " << std::dec << p << " FPGA " << n << ": FAIL (read error)"
				     << std::endl;
			}
		}
	}
	if(ostr.str().empty())
		ostr << "No active ports found." << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::BurstWriteTest(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, all active ports)", int, -1);
	uint16_t address   = __GET_ARG_IN__("address (Default: 0x35)", uint16_t, 0x35);
	uint16_t value     = __GET_ARG_IN__("value (Default: 0)", uint16_t, 0);
	int      count     = __GET_ARG_IN__("count (Default: 10)", int, 10);
	bool     broadcast = __GET_ARG_IN__("broadcast (Default: true)", bool, true);

	std::stringstream ostr;

	bool isFebAddress = (address >= ROC::FEB);

	if(isFebAddress && port != 0)
	{
		if(port < 0 && broadcast)
		{
			uint16_t writeAddress = (address & ~ROC::FEB) | ROC::FEB_Broadcast;

			ostr << "Broadcast: writing 0x" << std::hex << value << " to 0x"
			     << writeAddress << " x" << std::dec << count << std::endl;

			auto t0 = std::chrono::steady_clock::now();

			for(int i = 0; i < count; ++i)
				this->writeRegister(writeAddress, value);

			bool responsive = waitForFebResponsive();

			auto t1 = std::chrono::steady_clock::now();
			auto elapsed_ms =
			    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

			ostr << "  waitForFebResponsive: " << (responsive ? "ok" : "TIMEOUT")
			     << std::endl;
			ostr << "  Elapsed: " << elapsed_ms << " ms" << std::endl;
		}
		else
		{
			const uint32_t active = GetActivePorts();

			for(uint16_t p = 1; p <= 24; ++p)
			{
				if(port > 0 && p != port)
					continue;
				if(port < 0 && !(active & (0x00000001 << (p - 1))))
					continue;

				SetActivePort(p);

				ostr << "Port " << std::dec << p << ": writing 0x" << std::hex << value
				     << " to 0x" << address << " x" << std::dec << count << std::endl;

				auto t0 = std::chrono::steady_clock::now();

				for(int i = 0; i < count; ++i)
					this->writeRegister(address, value);

				bool responsive = waitForFebResponsive();

				auto t1 = std::chrono::steady_clock::now();
				auto elapsed_ms =
				    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0)
				        .count();

				ostr << "  waitForFebResponsive: " << (responsive ? "ok" : "TIMEOUT")
				     << std::endl;
				ostr << "  Elapsed: " << elapsed_ms << " ms" << std::endl;
			}
		}
	}
	else
	{
		ostr << "ROC: writing 0x" << std::hex << value << " to 0x" << address << " x"
		     << std::dec << count << std::endl;

		auto t0 = std::chrono::steady_clock::now();

		for(int i = 0; i < count; ++i)
			this->writeRegister(address, value);

		bool responsive = waitForFebResponsive();

		auto t1 = std::chrono::steady_clock::now();
		auto elapsed_ms =
		    std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

		ostr << "waitForFebResponsive: " << (responsive ? "ok" : "TIMEOUT") << std::endl;
		ostr << "Elapsed: " << elapsed_ms << " ms" << std::endl;
	}

	__SET_ARG_OUT__("response", ostr.str());
}

DEFINE_OTS_INTERFACE(ROCCosmicRayVetoInterface)
