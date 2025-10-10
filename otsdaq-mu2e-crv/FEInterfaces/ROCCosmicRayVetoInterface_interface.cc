#include <bitset>
#include "otsdaq-mu2e-crv/FEInterfaces/FEBII_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/FEB_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/ROCCosmicRayVetoInterface.h"
#include "otsdaq-mu2e-crv/FEInterfaces/ROC_Registers.h"
#
#include "otsdaq/Macros/InterfacePluginMacros.h"

using namespace ots;

#define TLVL_ROCConfig TLVL_DEBUG + 5
#define TLVL_FEBConfig TLVL_DEBUG + 6
#define TLVL_Start TLVL_DEBUG + 7

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FE-ROCCosmicRayVetoInterface"

//=========================================================================================
ROCCosmicRayVetoInterface::ROCCosmicRayVetoInterface(
    const std::string&       rocUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       theConfigurationPath)
    : ROCCoreVInterface(rocUID, theXDAQContextConfigTree, theConfigurationPath)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__COUT_INFO__ << "ROCCosmicRayVetoInterface instantiated with link: " << (int)linkID_
	              << " and EventWindowDelayOffset = " << delay_ << __E__;

	registerFEMacroFunction("Do the CRV Dance",
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

	registerFEMacroFunction("Configure CRV (FEBI)",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::Configure),
	                        std::vector<std::string>{"hard resets (Default: false)",
	                                                 "bias (Default: 0xaac)",
	                                                 "threshold (Default: 0xc)",
	                                                 "spill length (Default: 0xff)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions

	registerFEMacroFunction(
	    "Configure ROC",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::RocConfigure),
	    std::vector<std::string>{"send GR packages (Default: false)",
	                             "# of counter packages (Default: 0)",
	                             "uB offset (if not GR) (Default: 0xa)"},
	    std::vector<std::string>{},
	    1);  // requiredUserPermissions

	registerFEMacroFunction("FEB Configure",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebConfigure),
	                        std::vector<std::string>{},
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions

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
	                                                 "Injection Timestamp"},
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

	registerFEMacroFunction("FEB Get Status Pretty",
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

	registerFEMacroFunction("FEB Set Bias",
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
	registerFEMacroFunction(
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
	                            "port (Default: -1, current active)",
	                            "fpga [0,1,2,3], -1 all (Default)",
	                            "channel [0-15], -1 all (Default)",
	                            "threshold",
	                        },
	                        std::vector<std::string>{},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Bias",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetBias),
	                        std::vector<std::string>{
	                            "port (Default: -1, current active)",
	                            "fpga [0,1,2,3]",
	                            "number [0,1]",
	                            "bias",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Bias Trim",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetBiasTrim),
	                        std::vector<std::string>{
	                            "port (Default: -1, current active)",
	                            "fpga [0,1,2,3]",
	                            "channel [0-15]",
	                            "bias trim",
	                        },
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Gate OnSpill",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetGateOnSpill),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "gate start, 6.25ns (Default: 16)",
	                                                 "gate end, 6.25ns (Default: 255)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Gate OffSpill",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetGateOffSpill),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "gate start, 6.25ns (Default: 16)",
	                                                 "gate end, 6.25ns (Default: 15000)"},
	                        std::vector<std::string>{"response"},
	                        1);  // requiredUserPermissions
	registerFEMacroFunction("FEB II Set Channel",
	                        static_cast<FEVInterface::frontEndMacroFunction_t>(
	                            &ROCCosmicRayVetoInterface::FebIISetChannel),
	                        std::vector<std::string>{"port (Default: -1, current active)",
	                                                 "fpga [0,1,2,3], -1 all (Default)",
	                                                 "channel [0-15], -1 all (Default)",
	                                                 "fake (Default: false)",
	                                                 "off (Default: false)"},
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
	    std::vector<std::string>{"port (Default: -1, current active)",
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
	    "FEB II Reset and Align",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &ROCCosmicRayVetoInterface::FebIIAlign),
	    std::vector<std::string>{"port (Default: -1, current active)",
	                             "re-lock PLL (Default: true)",
	                             "sleep for lock [ms] (Default: 1000)",
	                             "sleep [us] (Default: 1000)"},
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
	bool gr = false;
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
	RocConfigure(gr);

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

	// __COUT_INFO__ << "......... Clear DCS FIFOs" << __E__;
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
{  // runNumber)
	ResetRxBuffers();
	// take pedestrals
	// this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::CSRBroadCast, 0x100);
	// TLOG(TLVL_Start) << "Taking pedestrals" << __E__;
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
	this->readRegister(ROC::Version);

	// first broadcast common settings
	// Set external trigger to RJ45
	// To make the PLL lock properly, we often need to shut the PLL off first
	this->writeRegister(FEB::AllFEB | FEB::TRIG, 0x1);
	usleep(0);  // seems to work, 2025-01-03
	this->writeRegister(FEB::AllFEB | FEB::TRIG, 0x0);

	// Enable self-triggering on spill gate
	// this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::IntTrgEn, 0x2); // doesn't seem
	// to work yet? 2025-01-03
	this->writeRegister(FEB::AllFPGA | FEB::IntTrgEn, 0x2);
	// Set number of ADC samples to 8, will be 12 moving forward
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::Samples, 0x8);

	// Reset DDR write/read pointers
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::RdPtrHi,
	                    0x0);  // not really needed
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::RdPtrLo,
	                    0x0);  // not really needed
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::WrPtrHi,
	                    0x0);  // not really needed
	this->writeRegister(FEB::AllFEB | FEB::AllFPGA | FEB::WrPtrLo,
	                    0x0);  // not really needed

	// Take Pedestrals
	// moved to start

	uint16_t TEMPFIX = 0xefff;
	if(useOtsConfig)
	{
		try
		{
			auto rocConfigs = getSelfNode()
			                      .getNode("ROCTypeLinkTable")
			                      .getNode("LinkToSubsystemCRVGroupedParametersTable")
			                      .getChildren();
			for(const auto& rocConfig : rocConfigs)
			{
				// Set on-spill gate @ 80MHz
				if(rocConfig.second.getNode("Name").getValueAsString() ==
				   "OnSpillGateLength")
				{
					uint16_t onSpillGateLength =
					    rocConfig.second.getNode("Value").getValue<uint16_t>();
					this->writeRegister(
					    (FEB::AllFEB | FEB::AllFPGA | FEB::OnSpillGate) & TEMPFIX,
					    onSpillGateLength);
					TLOG(TLVL_FEBConfig)
					    << "Broadcast 'OnSpillGateLength' 0x" << std::hex
					    << onSpillGateLength << " (80MHz) to all FEBs " << __E__;
					continue;
				}
				// Set off-spill gate @ 80MHz
				if(rocConfig.second.getNode("Name").getValueAsString() ==
				   "OffSpillGateLength")
				{
					uint16_t offSpillGateLength =
					    rocConfig.second.getNode("Value").getValue<uint16_t>();
					this->writeRegister(
					    (FEB::AllFEB | FEB::AllFPGA | FEB::OffSpillGate) & TEMPFIX,
					    offSpillGateLength);
					TLOG(TLVL_FEBConfig)
					    << "Broadcast 'OffSpillGateLength' 0x" << std::hex
					    << offSpillGateLength << " (80MHz) to all FEBs " << __E__;
					continue;
				}
				// Set pipeline delay
				if(rocConfig.second.getNode("Name").getValueAsString() ==
				   "HitPipelineDelay")
				{
					uint16_t hitPipelineDelay =
					    rocConfig.second.getNode("Value").getValue<uint16_t>();
					this->writeRegister(
					    (FEB::AllFEB | FEB::AllFPGA | FEB::Pipeline) & TEMPFIX,
					    hitPipelineDelay);
					TLOG(TLVL_FEBConfig) << "Broadcast 'HitPipelineDelay' 0x" << std::hex
					                     << hitPipelineDelay << " to all FEBs " << __E__;
					continue;
				}
			}
		}
		catch(...)
		{
			TLOG(TLVL_WARNING)
			    << "Missing 'ROCTypeLinkTable/LinkToSubsystemGroupedParametersTable', "
			       "skipping broadcasted FEB config. Use defaults."
			    << __E__;
		}
	}
	else
	{  // don't use the OTS config, for use in Macro Maker Mode
		this->writeRegister((FEB::AllFEB | FEB::AllFPGA | FEB::OnSpillGate) & TEMPFIX,
		                    0x0ff);  // WARNING, at 0xfff things seem to go wrong!
		this->writeRegister((FEB::AllFEB | FEB::AllFPGA | FEB::OffSpillGate) & TEMPFIX,
		                    0x0ff);
		this->writeRegister((FEB::AllFEB | FEB::AllFPGA | FEB::Pipeline) & TEMPFIX, 0x5);

		this->writeRegister(FEB::AllFPGA | FEB::Port, 0x1);
	}

	usleep(100000);  // 20ms doesn't work
	                 // loop through all active FEBs
	if(useOtsConfig)
	{
		auto febs = getSelfNode().getNode("LinkToFEBInterfaceTable").getChildren();
		for(const auto& feb : febs)
		{
			bool active = feb.second.getNode("Status").getValue<bool>();
			if(active)
			{
				uint16_t port = feb.second.getNode("Port").getValue<uint16_t>();
				TLOG(TLVL_FEBConfig) << "Configure FEB " << feb.first << " on port "
				                     << std::to_string(port) << __E__;
				SetActivePort(port, true);
				this->writeRegister(FEB::AllFPGA | FEB::Port, port);

				// loop through channels
				if(!feb.second.getNode("LinkToCRVChannelTable").isDisconnected())
				{
					for(const auto& ch :
					    feb.second.getNode("LinkToCRVChannelTable").getChildren())
					{
						uint16_t channel =
						    ch.second.getNode("Channel").getValue<uint16_t>();
						uint16_t biasTrim =
						    ch.second.getNode("BiasTrim").getValue<uint16_t>();
						uint16_t threshold =
						    ch.second.getNode("Threshold").getValue<uint16_t>();
						uint16_t fpga     = channel >> 4;
						uint16_t channel_ = channel & 0xf;
						TLOG(TLVL_FEBConfig)
						    << "Configure channel " << ch.first << " ch: " << channel
						    << "(fpga" << fpga << ":" << channel_ << ")"
						    << ", trim: 0x" << std::hex << biasTrim << ", threshold: 0x"
						    << std::hex << threshold << __E__;
						if(fpga > 3)
						{
							__FE_SS__ << "Channel number " << channel << " (fpga" << fpga
							          << ":" << channel_
							          << ") is not valid. Fpga needs to be in [0,1,2,3].";
							__SS_THROW__;
						}
						this->writeRegister(FEB::FPGA[fpga] | (FEB::BiasTrim + channel_),
						                    biasTrim);
						this->writeRegister(FEB::FPGA[fpga] | (FEB::Threshold + channel_),
						                    threshold);
					}
				}
				else
				{
					TLOG(TLVL_WARNING) << "Missing 'LinkToCRVChannelTable' link, "
					                      "skipping channel config."
					                   << __E__;
				}

				// loop through channel groups
				if(!feb.second.getNode("LinkToCRVChannelGroupTable").isDisconnected())
				{
					for(const auto& chg :
					    feb.second.getNode("LinkToCRVChannelGroupTable").getChildren())
					{
						uint16_t number =
						    chg.second.getNode("Number").getValue<uint16_t>();
						uint16_t bias = chg.second.getNode("Bias").getValue<uint16_t>();
						uint16_t fpga = number >> 1;
						uint16_t no   = number & 0x1;
						TLOG(TLVL_FEBConfig) << "Configure channel group " << chg.first
						                     << " number: " << number << "(fpga" << fpga
						                     << ":" << no << ")"
						                     << ", bias: 0x" << std::hex << bias << __E__;
						if(fpga > 3)
						{
							__FE_SS__ << "Number " << number << " (fpga" << fpga << ":"
							          << no
							          << ") is not valid. Fpga needs to be in [0,1,2,3].";
							__SS_THROW__;
						}
						this->writeRegister(FEB::FPGA[fpga] | (FEB::Bias + no), bias);
					}
				}
				else
				{
					TLOG(TLVL_WARNING) << "Missing 'LinkToCRVChannelGroupTable' link, "
					                      "skipping channel group config."
					                   << __E__;
				}
			}
			else
			{
				TLOG(TLVL_FEBConfig) << "Skip configuration of " << feb.first
				                     << " because its not active." << __E__;
			}
		}
	}
}

void ROCCosmicRayVetoInterface::RocConfigure(bool gr, uint16_t grn, uint16_t uBoffset)
{
	TLOG(TLVL_ROCConfig) << "RocConfigure Start " << __E__;

	// set the ROC address
	this->writeRegister(ROC::ID, (uint16_t)linkID_);

	// Enable the onboard PLL (1 is power down)
	this->writeRegister(ROC::PLLStat, 0x0);
	// and configure PLL mux to read digital lock
	this->writeRegister(ROC::PLLMuxHigh, 0x12);
	this->writeRegister(ROC::PLLMuxHLow, 0x12);

	// enable package forwarding based on markers
	// this->writeRegister(ROC::CR, 0x20);
	usleep(1000000);
	// return;
	SetMarkerSync(true);

	this->writeRegister(ROC::Clk80MHz, 0x1);  // enable the 80MHz clock alignment

	// Set CSR of data-FPGAs
	// bit 3: FM Rx Enable
	// bit 5: DDR Write Sequencer Enable
	// bit 7: DDR read sequencer Enable
	this->writeRegister(ROC::Data_Broadcast | ROC::Data_CRC, 0xA8);  //

	// Reset input buffers
	ResetRxBuffers();

	// Reset DDR on Data FPGAs
	for(int i = 0; i < 3; ++i)
	{
		this->writeRegister(ROC::Data[i] | ROC::Data_DDR_WriteHigh, 0x0);
		this->writeRegister(ROC::Data[i] | ROC::Data_DDR_WriteLow, 0x0);
		this->writeRegister(ROC::Data[i] | ROC::Data_DDR_ReadHigh, 0x0);
		this->writeRegister(ROC::Data[i] | ROC::Data_DDR_ReadLow, 0x0);
	}

	// Set TRIG 1
	this->writeRegister(ROC::TRIG, 0x1);

	// Enable GR package return
	TLOG(TLVL_ROCConfig) << "Global Run Mode is " << (gr ? "enabled" : "disabled") << "."
	                     << __E__;
	if(gr)
	{
		this->writeRegister(ROC::sendGR, 0x1 + (grn << 8));
		// this->writeRegister(ROC::sendGR, 0x2);///

		// Disable send of active FEBs
		this->writeRegister(ROC::Data[0] | ROC::Data_LinkCtrl, 0x0);
		this->writeRegister(ROC::uBOffset, 0x0);
	}
	else
	{
		this->writeRegister(ROC::sendGR, 0x0);

		// Enable send of active FEBs
		this->writeRegister(ROC::Data[0] | ROC::Data_LinkCtrl, 0x0);
		this->writeRegister(ROC::uBOffset, uBoffset);
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
	RocConfigure(gr, grn, uBoffset);
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
	          .count() < 1000)
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
	int      port      = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga      = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t channel   = __GET_ARG_IN__("channel [0-15], -1 all (Default)", uint16_t, -1);
	uint16_t threshold = __GET_ARG_IN__("threshold", uint16_t, 0);
	if(port > 0)
		SetActivePort(port);

	if(fpga == uint16_t(-1))
	{
		for(uint16_t fpga_ = 0; fpga_ < 4; fpga_++)
		{
			if(channel == uint16_t(-1))
			{
				for(uint16_t ch_ = 0; ch_ < 16; ch_++)
				{
					this->writeRegister(
					    FEBII::FPGA[fpga_] | (FEBII::ThresholdBase + (ch_ & 0xF)),
					    threshold);
				}
			}
			else
			{
				this->writeRegister(
				    FEBII::FPGA[fpga_] | (FEBII::ThresholdBase + (channel & 0xF)),
				    threshold);
			}
		}
	}
	else
	{
		if(channel == uint16_t(-1))
		{
			for(uint16_t ch_ = 0; ch_ < 16; ch_++)
			{
				this->writeRegister(
				    FEBII::FPGA[fpga] | (FEBII::ThresholdBase + (ch_ & 0xF)), threshold);
			}
		}
		else
		{
			this->writeRegister(
			    FEBII::FPGA[fpga] | (FEBII::ThresholdBase + (channel & 0xF)), threshold);
		}
	}
}

void ROCCosmicRayVetoInterface::FebIISetBias(__ARGS__)
{
	int      port   = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga   = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t, 0);
	uint16_t number = __GET_ARG_IN__("number [0,1]", uint16_t, 0);
	uint16_t bias   = __GET_ARG_IN__("bias", uint16_t, 0);
	if(port > 0)
		SetActivePort(port);
	this->writeRegister(FEBII::FPGA[fpga] | (FEBII::BiasBase + (number & 0x1)),
	                    bias);  // Adjust register if needed
	std::stringstream ostr;
	ostr << "FPGA " << fpga << ", number " << number << ": Set CR=0x" << std::hex << bias
	     << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetBiasTrim(__ARGS__)
{
	int      port     = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga     = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t, 0);
	uint16_t channel  = __GET_ARG_IN__("channel [0-15]", uint16_t, 0);
	uint16_t biasTrim = __GET_ARG_IN__("bias trim", uint16_t, 0);
	if(port > 0)
		SetActivePort(port);
	this->writeRegister(FEBII::FPGA[fpga] | (FEBII::TrimBase + (channel & 0xF)),
	                    biasTrim);
	std::stringstream ostr;
	ostr << "FPGA " << fpga << ", channel " << channel << ": Set TrimBase=0x" << std::hex
	     << biasTrim << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetGateOnSpill(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t gateStart = __GET_ARG_IN__("gate start, 6.25ns (Default: 16)", uint16_t, 16);
	uint16_t gateEnd   = __GET_ARG_IN__("gate end, 6.25ns (Default: 255)", uint16_t, 255);
	if(port > 0)
		SetActivePort(port);
	this->writeRegister(FEBII::GateOnOnSpill, gateStart);
	this->writeRegister(FEBII::GateOffOnSpill, gateEnd);
	std::stringstream ostr;
	ostr << "Set GateOnOnSpill=0x" << std::hex << gateStart << ", GateOffOnSpill=0x"
	     << gateEnd << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetGateOffSpill(__ARGS__)
{
	int      port      = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t gateStart = __GET_ARG_IN__("gate start, 6.25ns (Default: 16)", uint16_t, 16);
	uint16_t gateEnd =
	    __GET_ARG_IN__("gate end, 6.25ns (Default: 15000)", uint16_t, 15000);
	if(port > 0)
		SetActivePort(port);
	this->writeRegister(FEBII::GateOnOffSpill, gateStart);
	this->writeRegister(FEBII::GateOffOffSpill, gateEnd);
	std::stringstream ostr;
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
	}
	ostr << "===============================" << std::endl;
	__SET_ARG_OUT__("response", ostr.str());
}

void ROCCosmicRayVetoInterface::FebIISetChannel(__ARGS__)
{
	int      port    = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga    = __GET_ARG_IN__("fpga [0,1,2,3], -1 all (Default)", uint16_t, -1);
	uint16_t channel = __GET_ARG_IN__("channel [0-15], -1 all (Default)", uint16_t, -1);
	bool     fake    = __GET_ARG_IN__("fake (Default: false)", bool, false);
	bool     off     = __GET_ARG_IN__("off (Default: false)", bool, false);
	if(port > 0)
		SetActivePort(port);
	// Example: Write to Status register to indicate channel set (replace with correct
	// register if needed)
	std::stringstream ostr;
	ostr << "Set ";
	if(fpga == uint16_t(-1))
	{
		ostr << "all FPGAs, ";
		for(unsigned int fpga_ = 0; fpga_ < 4; fpga_++)
		{
			if(channel == uint16_t(-1))
			{
				ostr << "all channels ";
				for(unsigned int ch_ = 0; ch_ < 16; ++ch_)
				{
					uint16_t val = fake ? 0x10 : (off ? 0x11 : ch_);
					this->writeRegister(
					    FEBII::FPGA[fpga_] | (FEBII::ChannelMapBase + (ch_ & 0xF)), val);
				}
			}
			else
			{
				ostr << "channel " << channel << " ";
				uint16_t val = fake ? 0x10 : (off ? 0x11 : channel);
				this->writeRegister(
				    FEBII::FPGA[fpga_] | (FEBII::ChannelMapBase + (channel & 0xF)), val);
			}
		}
	}
	else
	{
		ostr << "FPGA, " << fpga << " ";
		if(channel == uint16_t(-1))
		{
			ostr << "all channels ";
			for(unsigned int ch_ = 0; ch_ < 16; ++ch_)
			{
				uint16_t val = fake ? 0x10 : (off ? 0x11 : ch_);
				this->writeRegister(
				    FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (ch_ & 0xF)), val);
			}
		}
		else
		{
			ostr << "channel " << channel << " ";
			uint16_t val = fake ? 0x10 : (off ? 0x11 : channel);
			this->writeRegister(
			    FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (channel & 0xF)), val);
		}
	}
	ostr << "to " << (fake ? "0x10 (fake mode)" : (off ? "0x11 (off)" : "ch-idx")) << "."
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

void ROCCosmicRayVetoInterface::FebIITrigBaselines(__ARGS__)
{
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	int ch   = __GET_ARG_IN__("channel [0-63] (Default: -1, all)", int, -1);
	if(port > 0)
		SetActivePort(port);

	std::stringstream ostr;
	ostr << "Trigger baseline measurment ";

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

void ROCCosmicRayVetoInterface::ResetPLL(int sleep_ms)
{
	this->writeRegister(FEBII::EWTFakeMode, 0x1);  // fake mode, Ph_det off
	usleep(sleep_ms * 1000);
	this->writeRegister(FEBII::EWTFakeMode,
	                    0x0);  // back to extenral, Ph_det enabled again
	usleep(sleep_ms * 1000);
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

void ROCCosmicRayVetoInterface::FebIIAlign(__ARGS__)
{
	int  port       = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	bool lock       = __GET_ARG_IN__("re-lock PLL (Default: true)", bool, true);
	int  sleepReset = __GET_ARG_IN__("sleep for lock [ms] (Default: 1000)", int, 1000);
	int  sleep      = __GET_ARG_IN__("sleep [us] (Default: 1000)", int, 1000);

	if(port > 0)
		SetActivePort(port);
	std::stringstream ostr;
	ostr << std::endl;

	// force PLL to relock
	if(lock)
	{
		ResetPLL(sleepReset);
		// this->writeRegister(FEBII::EWTFakeMode, 0x1); // fake mode, Ph_det off
		// usleep(sleepReset*1000);
		// this->writeRegister(FEBII::EWTFakeMode, 0x0); // back to extenral, Ph_det
		// enabled again usleep(sleepReset*1000);
		ostr << "PLL relock triggered" << std::endl;
	}

	for(unsigned int fpga = 0; fpga < 4; fpga++)
	{
		this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR, 0x4);  // force AFE realignment
		usleep(sleep);
		this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR, 0x0);  // reset AFE realignment
		usleep(sleep);
		uint16_t status = this->readRegister(FEBII::FPGA[fpga] | FEBII::Status);
		// ostr << "FPGA " << fpga << ": status=0x" << std::hex << status << std::endl;
		if(status & 0x200)
		{
			this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR,
			                    0x204);  // force AFE realignment, invert 80MHz phase
			usleep(sleep);
			this->writeRegister(FEBII::FPGA[fpga] | FEBII::CR,
			                    0x200);  // reset AFE realignment
			usleep(sleep);
			status = this->readRegister(FEBII::FPGA[fpga] | FEBII::Status);
			ostr << "FPGA " << fpga << " - inverted phase: status=0x" << std::hex
			     << status << std::endl;
		}
		else
		{
			ostr << "FPGA " << fpga << " - default phase: status=0x" << std::hex << status
			     << std::endl;
		}
	}
	__SET_ARG_OUT__("response", ostr.str());
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
	int      port        = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t bias        = __GET_ARG_IN__("bias (Default: 0xaaf)", uint16_t, 0xaaf);
	bool     skip_bias   = __GET_ARG_IN__("skip bias (Default: false)", bool, false);
	uint16_t threshold   = __GET_ARG_IN__("threshold (Default: 0x50)", uint16_t, 0x50);
	bool update_baseline = __GET_ARG_IN__("update baseline (Default: true)", bool, true);
	uint16_t onSpillGateEnd =
	    __GET_ARG_IN__("onSpillGateEnd (Default: 255, 6.25ns)", uint16_t, 255);
	uint16_t offSpillGateEnd =
	    __GET_ARG_IN__("offSpillGateEnd (Default: 15000, 6.25ns)", uint16_t, 15000);
	bool pll_reset = __GET_ARG_IN__("pll reset (Default: true)", bool, true);

	if(port > 0)
		SetActivePort(port);
	std::stringstream ostr;

	// Reset PLL
	if(pll_reset)
	{
		ostr << "Resetting PLL (wait 1s)" << std::endl;
		ResetPLL(1000);
	}

	// forcer AFE to realign
	uint16_t status = Realign(1000);
	ostr << "Forcing AFE realignment: " << ((status & 0x1) ? "inverted" : "default")
	     << ", " << ((status & 0x2) ? "inverted" : "default") << ", "
	     << ((status & 0x4) ? "inverted" : "default") << ", "
	     << ((status & 0x8) ? "inverted" : "default") << std::endl;

	// set port
	if(port == -1)
	{
		//    int16_t aport = GetActivePorts();
		//    for(unsigned int fpga = 0; fpga < 4; fpga++) {
		//        this->writeRegister(FEBII::FPGA[fpga] | FEBII::Port, aport);
		//    }
	}
	else
	{
		for(unsigned int fpga = 0; fpga < 4; fpga++)
		{  // TODO, when avaiable use 0x329
			this->writeRegister(FEBII::FPGA[fpga] | FEBII::Port, port);
		}
	}

	// bias
	if(!skip_bias)
	{
		ostr << "Ramping all bias to " << std::hex << bias << "with spacing of 5s"
		     << std::endl;
		for(uint16_t fpga = 0; fpga < 4; ++fpga)
		{
			for(uint16_t idx = 0; idx < 2; ++idx)
			{
				this->writeRegister(FEBII::FPGA[fpga] | (FEBII::BiasBase + (idx & 0x1)),
				                    bias);
				sleep(5);
			}
		}
	}

	// Enable channels, and thresholds, trigger baseline
	ostr << "Enabling channels, and thresholds";
	if(update_baseline)
		ostr << ", and triggering baselines";
	ostr << std::endl;
	for(unsigned int fpga = 0; fpga < 4; fpga++)
	{
		for(uint16_t ch = 0; ch < 16; ++ch)
		{
			this->writeRegister(FEBII::FPGA[fpga] | (FEBII::ChannelMapBase + (ch & 0xF)),
			                    ch);
			this->writeRegister(FEBII::FPGA[fpga] | (FEBII::ThresholdBase + (ch & 0xF)),
			                    threshold);
			if(update_baseline)
			{
				this->writeRegister(
				    FEBII::FPGA[fpga] | (FEBII::BaselineBase + (ch & 0xF)), 0x1);
			}
		}
	}

	ostr << "Setting on-spill gate end to " << onSpillGateEnd
	     << "(6.25ns) and off-spill gate end to " << offSpillGateEnd << "(6.25ns)"
	     << std::endl;
	this->writeRegister(FEBII::GateOffOnSpill, onSpillGateEnd);
	this->writeRegister(FEBII::GateOffOffSpill, offSpillGateEnd);

	__SET_ARG_OUT__("response", ostr.str());
}

DEFINE_OTS_INTERFACE(ROCCosmicRayVetoInterface)
