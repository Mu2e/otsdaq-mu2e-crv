#include "otsdaq-mu2e-crv/FEInterfaces/ROCCosmicRayVetoInterface.h"
#include "otsdaq-mu2e-crv/FEInterfaces/ROC_Registers.h"
#include "otsdaq-mu2e-crv/FEInterfaces/FEB_Registers.h"

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

	__MCOUT_INFO__("ROCCosmicRayVetoInterface instantiated with link: "
	               << (int)linkID_ << " and EventWindowDelayOffset = " << delay_ << __E__);

	registerFEMacroFunction(
		"Do the CRV Dance",
			static_cast<FEVInterface::frontEndMacroFunction_t>(
					&ROCCosmicRayVetoInterface::DoTheCRV_Dance),
					std::vector<std::string>{"Which Step"},
					std::vector<std::string>{						
						"Random Result"},
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

    	registerFEMacroFunction("Configure ROC",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
					&ROCCosmicRayVetoInterface::RocConfigure),
					std::vector<std::string>{"send GR packages (Default: true)"},
					std::vector<std::string>{},
					1);  // requiredUserPermissions

		registerFEMacroFunction("Reset TX and Counters",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
					&ROCCosmicRayVetoInterface::SoftReset),
					std::vector<std::string>{},
					std::vector<std::string>{},
					1);  // requiredUserPermissions

        registerFEMacroFunction("Get Status",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
					&ROCCosmicRayVetoInterface::GetStatus),
					std::vector<std::string>{},
					std::vector<std::string>{
					"version", "git hash",
					"CR", "Send GR", "Loopback Mode", "PLL lock", "Active Ports", "Uptime", "Link Errors Loss", "Link Errors CRC", 
					"Test Cnt", "Marker Decoded Cnt", "Marker Delayed Cnt", "Heartbeat Rx Cnt", "Heartbeat Tx Cnt", "DR Cnt", "Injection Cnt", "Loopback Markers (fiber) Cnt",
					"Last Event Length (12.5ns)", "Injection Length (12.5ns)", "Injection Timestamp"
					},
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
        registerFEMacroFunction("Histogram",
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

}

//==========================================================================================
ROCCosmicRayVetoInterface::~ROCCosmicRayVetoInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it uses the
	// tree and it may already be destructed partially
	__COUT__ << FEVInterface::interfaceUID_ << " Destructor" << __E__;
}

////==================================================================================================
//void ROCCosmicRayVetoInterface::writeROCRegister(uint16_t address, uint16_t data_to_write)
//{
//	__FE_COUT__ << "Calling write ROC register: link number " << std::dec << (int)linkID_
//	            << ", address = " << address << ", write data = " << data_to_write
//	            << __E__;
//
//	return;
//}
//
////==================================================================================================
//uint16_t ROCCosmicRayVetoInterface::readROCRegister(uint16_t address)
//{
//	__FE_COUT__ << "Calling read ROC register: link number " << std::dec << linkID_
//	            << ", address = " << address << __E__;
//
//	return -1;
//}
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
//int ROCCosmicRayVetoInterface::readTimestamp() { return this->readRegister(12); }

////==================================================================================================
//void ROCCosmicRayVetoInterface::writeDelay(uint16_t delay)
//{
//	this->writeRegister(21, delay);
//	return;
//}

////==================================================================================================
//int ROCCosmicRayVetoInterface::readDelay() { return this->readRegister(7); }

////==================================================================================================
//int ROCCosmicRayVetoInterface::readDTCLinkLossCounter() { return this->readRegister(8); }

////==================================================================================================
//void ROCCosmicRayVetoInterface::resetDTCLinkLossCounter()
//{
//	this->writeRegister(24, 0x1);
//	return;
//}

//==================================================================================================
void ROCCosmicRayVetoInterface::configure(void) try
{
	__MCOUT_INFO__("configure CRV ROC");
	bool gr = false;
	try {
		auto rocConfigs = getSelfNode().getNode("ROCTypeLinkTable")
		                               .getNode("LinkToSubsystemCRVGroupedParametersTable").getChildren();
		for(const auto& rocConfig : rocConfigs) {
			if(rocConfig.second.getNode("Name").getValueAsString() == "ROCGR") {
				gr = rocConfig.second.getNode("Value").getValue<bool>();
				if(gr) __FE_COUT__ << "Enable CRV ROC GR mode" << __E__;
				break;
			}
		}
	} catch(...) { 
        TLOG(TLVL_WARNING) << "Missing 'ROCTypeLinkTable/LinkToSubsystemCRVGroupedParametersTable', GR mode not set, default to " << gr << __E__;
    }
	RocConfigure(gr);

	// ================================ FEB part ================================

	bool doConfigureFEBs = false;
    try {
		doConfigureFEBs = Configurable::getSelfNode()
                                        .getNode("EnableFEBConfigureStep")
                                        .getValue<bool>();
    } catch(...) {       
        __FE_COUT__ << "'EnableFEBConfigureStep' not found. Default to " << doConfigureFEBs << __E__;
    }  // ignore missing field
	if(doConfigureFEBs) FebConfigure();

	// __MCOUT_INFO__("......... Clear DCS FIFOs" << __E__);
}
catch(const std::runtime_error& e)
{
	__FE_MOUT__ << "Error caught: " << e.what() << __E__;
	throw;
}
catch(...)
{
	__FE_SS__ << "Unknown error caught. Check printouts!" << __E__;
	__FE_MOUT__ << ss.str();
	__FE_SS_THROW__;
}

//==============================================================================
void ROCCosmicRayVetoInterface::halt(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::pause(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::resume(void) {}

//==============================================================================
void ROCCosmicRayVetoInterface::start(std::string) { // runNumber) 
    // take pedestrals
    this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::CSRBroadCast, 0x100);
    TLOG(TLVL_Start) << "Taking pedestrals" << __E__;
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
	__SET_ARG_OUT__("Random Result",0xA4);
	
} //end DoTheCRV_Dance()

void ROCCosmicRayVetoInterface::GetFirmwareVersion(__ARGS__)
{	
	__SET_ARG_OUT__("version", this->readRegister(ROC::Version));
	__SET_ARG_OUT__("git hash", 
	    (this->readRegister(ROC::GitHashHigh) << 16) +
		this->readRegister(ROC::GitHashLow)
	);
} //end GetFirmwareVersion()

void ROCCosmicRayVetoInterface::GetTestCounter(__ARGS__)
{	
	__SET_ARG_OUT__("counter", this->readRegister(ROC::TestCounter));
} //end GetTestCounter()

void ROCCosmicRayVetoInterface::SetTestCounter(__ARGS__)
{	
	uint16_t value = __GET_ARG_IN__("Set Counter (Default: 0)", uint16_t, 0);
	this->writeRegister(ROC::TestCounter, value);
} //end SetTestCounter()

void ROCCosmicRayVetoInterface::Reset() {
	this->writeRegister(ROC::Reset, 0x1);
}
void ROCCosmicRayVetoInterface::HardReset(__ARGS__) { Reset(); }

void  ROCCosmicRayVetoInterface::ResetTxBuffers() {
   this->writeRegister(ROC::GTP_CRC, 0x1);
   this->writeRegister(ROC::CRS, 0x300);
}
void ROCCosmicRayVetoInterface::SoftReset(__ARGS__)
{
    ResetTxBuffers();
}

void ROCCosmicRayVetoInterface::FebConfigure() {
	TLOG(TLVL_FEBConfig) << "FebConfigure start..." << __E__;
    this->readRegister(ROC::Version);

	// first broadcast common settings
	// Set external trigger to RJ45
	this->writeRegister(FEB::AllFEB|FEB::TRIG, 0x0);
	// Enable self-triggering on spill gate
	this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::IntTrgEn, 0x2);
	// Set number of ADC samples to 8, will be 12 moving forward
	this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::Samples, 0x8);
	// Reset DDR write/read pointers
	this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::RdPtrHi, 0x0); // not really needed
	this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::RdPtrLo, 0x0); // not really needed


    try {   
        auto rocConfigs = getSelfNode().getNode("ROCTypeLinkTable") 
                                       .getNode("LinkToSubsystemCRVGroupedParametersTable").getChildren();
        for(const auto& rocConfig : rocConfigs) {       
            // Set on-spill gate @ 80MHz
            uint16_t TEMPFIX = 0xefff;
			if(rocConfig.second.getNode("Name").getValueAsString() == "OnSpillGateLength") {
                uint16_t onSpillGateLength = rocConfig.second.getNode("Value").getValue<uint16_t>();
                this->writeRegister((FEB::AllFEB|FEB::AllFPGA|FEB::OnSpillGate)&TEMPFIX, onSpillGateLength);
                TLOG(TLVL_FEBConfig) << "Broadcast 'OnSpillGateLength' 0x" << std::hex << onSpillGateLength << " (80MHz) to all FEBs " << __E__;
                continue;
			}
			// Set off-spill gate @ 80MHz
            if(rocConfig.second.getNode("Name").getValueAsString() == "OffSpillGateLength") {
                uint16_t offSpillGateLength = rocConfig.second.getNode("Value").getValue<uint16_t>();
				this->writeRegister((FEB::AllFEB|FEB::AllFPGA|FEB::OffSpillGate)&TEMPFIX, offSpillGateLength);
                TLOG(TLVL_FEBConfig) << "Broadcast 'OffSpillGateLength' 0x" << std::hex << offSpillGateLength << " (80MHz) to all FEBs " << __E__;
                continue;
			}
			// Set pipeline delay
            if(rocConfig.second.getNode("Name").getValueAsString() == "HitPipelineDelay") {
                uint16_t hitPipelineDelay = rocConfig.second.getNode("Value").getValue<uint16_t>();
				this->writeRegister((FEB::AllFEB|FEB::AllFPGA|FEB::Pipeline)&TEMPFIX, hitPipelineDelay);
                TLOG(TLVL_FEBConfig) << "Broadcast 'HitPipelineDelay' 0x" << std::hex << hitPipelineDelay << " to all FEBs " << __E__;
                continue;
			}
		}
     } catch(...) {
            TLOG(TLVL_WARNING) << "Missing 'ROCTypeLinkTable/LinkToSubsystemGroupedParametersTable', skipping broadcasted FEB config." << __E__;
    }
    
	usleep(100000); // 20ms doesn't work
	// loop through all active FEBs
    auto febs = getSelfNode().getNode("LinkToFEBInterfaceTable").getChildren();
    for(const auto& feb : febs) {
        bool active = feb.second.getNode("Status").getValue<bool>();
        if(active) {       
            uint16_t port = feb.second.getNode("Port").getValue<uint16_t>();
            TLOG(TLVL_FEBConfig) << "Configure FEB " << feb.first << " on port " << std::to_string(port) << __E__;
			SetActivePort(port, true);
			this->writeRegister(FEB::AllFPGA|FEB::Addres, port);

			// loop through channels
			if(!feb.second.getNode("LinkToCRVChannelTable").isDisconnected()) {
			    for(const auto& ch : feb.second.getNode("LinkToCRVChannelTable").getChildren()) {
					uint16_t channel   = ch.second.getNode("Channel").getValue<uint16_t>();
					uint16_t biasTrim  = ch.second.getNode("BiasTrim").getValue<uint16_t>();
					uint16_t threshold = ch.second.getNode("Threshold").getValue<uint16_t>();
					uint16_t fpga = channel >> 4;
					uint16_t channel_ = channel & 0xf;
					TLOG(TLVL_FEBConfig) << "Configure channel " << ch.first
					                     << " ch: " << channel << "(fpga" << fpga << ":" << channel_ << ")"
                                         << ", trim: 0x" << std::hex << biasTrim
                                         << ", threshold: 0x" << std::hex << threshold
                                         << __E__;
					if(fpga > 3) {
						__FE_SS__  << "Channel number " << channel << " (fpga" << fpga << ":" << channel_ << ") is not valid. Fpga needs to be in [0,1,2,3].";
			            __SS_THROW__;
					}
					this->writeRegister(FEB::FPGA[fpga]|(FEB::BiasTrim + channel_), biasTrim);
					this->writeRegister(FEB::FPGA[fpga]|(FEB::Threshold + channel_), threshold);
				}
			} else {
                TLOG(TLVL_WARNING) << "Missing 'LinkToCRVChannelTable' link, skipping channel config." << __E__;
            }

			// loop through channel groups
			if(!feb.second.getNode("LinkToCRVChannelGroupTable").isDisconnected()) {
				for(const auto& chg : feb.second.getNode("LinkToCRVChannelGroupTable").getChildren()) {
					uint16_t number   = chg.second.getNode("Number").getValue<uint16_t>();
					uint16_t bias  = chg.second.getNode("Bias").getValue<uint16_t>();
					uint16_t fpga = number >> 1;
					uint16_t no = number & 0x1;
                    TLOG(TLVL_FEBConfig) << "Configure channel group " << chg.first
                                         << " number: " << number << "(fpga" << fpga << ":" << no << ")"
                                         << ", bias: 0x" << std::hex << bias
                                         << __E__;
					if(fpga > 3) {
						__FE_SS__  << "Number " << number << " (fpga" << fpga << ":" << no << ") is not valid. Fpga needs to be in [0,1,2,3].";
			            __SS_THROW__;
					}
					this->writeRegister(FEB::FPGA[fpga]|(FEB::Bias + no), bias);
				}
			} else {
                TLOG(TLVL_WARNING) << "Missing 'LinkToCRVChannelGroupTable' link, skipping channel group config." << __E__;
            }
        } else {
            TLOG(TLVL_FEBConfig) << "Skip configuration of " << feb.first << " because its not active." << __E__;
        }
	}
}


void ROCCosmicRayVetoInterface::RocConfigure(bool gr) {
	TLOG(TLVL_ROCConfig) << "RocConfigure Start " << __E__;

	// Enable the onboard PLL
	this->writeRegister(ROC::PLLStat,     0x0);
	// and configure PLL mux to read digital lock
	this->writeRegister(ROC::PLLMuxHigh, 0x12);
	this->writeRegister(ROC::PLLMuxHLow, 0x12);

	// enable package forwarding based on markers
	this->writeRegister(ROC::CR, 0x20);

	// Set CSR of data-FPGAs
	this->writeRegister(ROC::Data_Broadcast|ROC::Data_CRC, 0xA8);

    // Reset input buffers
    ResetTxBuffers();

	// Reset DDR on Data FPGAs
	for (int i = 0; i < 3; ++i) {
	    this->writeRegister(ROC::Data[i]|ROC::Data_DDR_WriteHigh,0x0);
	    this->writeRegister(ROC::Data[i]|ROC::Data_DDR_WriteHigh,0x0);
	    this->writeRegister(ROC::Data[i]|ROC::Data_DDR_ReadHigh, 0x0);
	    this->writeRegister(ROC::Data[i]|ROC::Data_DDR_ReadLow, 0x0);
	}

    	// Set TRIG 1
	this->writeRegister(ROC::TRIG, 0x1);

    	// set the ROC address
	this->writeRegister(ROC::ID, (uint16_t)linkID_);

	// Enable GR package return
	TLOG(TLVL_ROCConfig) << "Global Run Mode is " << (gr ? "enabled" : "disabled") << "." << __E__;
	if(gr) this->writeRegister(ROC::sendGR, 0x1);
	else this->writeRegister(ROC::sendGR, 0x0);
}

void ROCCosmicRayVetoInterface::RocConfigure(__ARGS__)
{
	bool gr = __GET_ARG_IN__("send GR packages (Default: true)", bool, true);
    RocConfigure(gr);
}

void ROCCosmicRayVetoInterface::GetStatus(__ARGS__)
{
    __SET_ARG_OUT__("version", this->readRegister(ROC::Version));
	__SET_ARG_OUT__("git hash", 
	    ((this->readRegister(ROC::GitHashHigh) << 16) +
		this->readRegister(ROC::GitHashLow)) & 0xffffffff
	);
	__SET_ARG_OUT__("CR", this->readRegister(ROC::CR));
	__SET_ARG_OUT__("Send GR", this->readRegister(ROC::sendGR) & 0x1);
	__SET_ARG_OUT__("Loopback Mode", this->readRegister(ROC::LoopbackMode));
	__SET_ARG_OUT__("PLL lock", ((this->readRegister(ROC::PLLStat)) >> 4) & 0x1 );
	__SET_ARG_OUT__("Active Ports", 
	    (this->readRegister(ROC::ActivePortsHigh) << 16) +
		this->readRegister(ROC::ActivePortsLow));
	__SET_ARG_OUT__("Uptime", 
	    (this->readRegister(ROC::UpTimeHigh) << 16) +
		this->readRegister(ROC::UpTimeLow));
	__SET_ARG_OUT__("Link Errors Loss", this->readRegister(ROC::LinkErrors) & 0xff);
	__SET_ARG_OUT__("Link Errors CRC", this->readRegister(ROC::LinkErrors) >> 12);

	// Counters
	__SET_ARG_OUT__("Test Cnt", this->readRegister(ROC::TestCounter));
	__SET_ARG_OUT__("Marker Decoded Cnt", this->readRegister(ROC::MarkerCnt) & 0xff);
	__SET_ARG_OUT__("Marker Delayed Cnt", (this->readRegister(ROC::MarkerCnt) >> 8));
	__SET_ARG_OUT__("Heartbeat Rx Cnt", this->readRegister(ROC::HeartBeat) & 0xff);
	__SET_ARG_OUT__("Heartbeat Tx Cnt", this->readRegister(ROC::HeartBeat) >> 8);
	__SET_ARG_OUT__("DR Cnt", 
	    (this->readRegister(ROC::DRCntHigh) << 16) +
		this->readRegister(ROC::DRCnLow));
	__SET_ARG_OUT__("Injection Cnt", this->readRegister(ROC::InjectionCnt));
	__SET_ARG_OUT__("Loopback Markers (fiber) Cnt", this->readRegister(ROC::LoopbackMarkerCnt));

	// Event Lengths
	__SET_ARG_OUT__("Last Event Length (12.5ns)", this->readRegister(ROC::LastEventLength));
	__SET_ARG_OUT__("Injection Length (12.5ns)", this->readRegister(ROC::InjectionLength));
	__SET_ARG_OUT__("Injection Timestamp", this->readRegister(ROC::InjectionTS));
	
}

void ROCCosmicRayVetoInterface::FiberRx(__ARGS__) {
	int n = __GET_ARG_IN__("number of packages (Default: 10)", int, 10);

	std::stringstream o;
	o << std::endl;
	for(int i = 0; i < n; ++i) { // n packages
	    for(int k = 0; k < 10; ++k) { // 10 words per package
            o << std::hex << std::setfill('0') << std::setw(4) << this->readRegister(ROC::GTPRxRead) << " ";
		}
		o << std::endl;
	}
	__SET_ARG_OUT__("buffer", o.str());
}

void ROCCosmicRayVetoInterface::FiberTx(__ARGS__) {
	int n = __GET_ARG_IN__("number of packages (Default: 10)", int, 10);

	std::stringstream o;
	o << std::endl;
	for(int i = 0; i < n; ++i) { // n packages
	    for(int k = 0; k < 10; ++k) { // 10 words per package
            o << std::hex << std::setfill('0') << std::setw(4) << this->readRegister(ROC::GTPTxRead) << " ";
		}
		o << std::endl;
	}
	__SET_ARG_OUT__("buffer", o.str());
}

void ROCCosmicRayVetoInterface::FebSetBias(__ARGS__) {
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t number = __GET_ARG_IN__("number [0,1]", uint16_t);
	uint16_t bias = __GET_ARG_IN__("bias", uint16_t, 0);

	if(port>0) {
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga]|(FEB::Bias + (number & 0x1)), bias);
}

void ROCCosmicRayVetoInterface::FebSetBiasTrim(__ARGS__) {
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t channel = __GET_ARG_IN__("channel [0-15]", uint16_t);
	uint16_t biasTrim = __GET_ARG_IN__("bias trim", uint16_t, 0);

	if(port>0) {
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga]|(FEB::BiasTrim + (channel & 0xf)), biasTrim);
}

void ROCCosmicRayVetoInterface::FebSetThreshold(__ARGS__) {
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
	uint16_t fpga = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
	uint16_t channel = __GET_ARG_IN__("channel [0-15]", uint16_t);
	uint16_t threshold = __GET_ARG_IN__("threshold", uint16_t, 0);

	if(port>0) {
		SetActivePort(port);
	}
	this->writeRegister(FEB::FPGA[fpga]|(FEB::Threshold + (channel & 0xf)), threshold);
}

void ROCCosmicRayVetoInterface::FebSetPipeline(__ARGS__) {
	uint16_t hitPipelineDelay = __GET_ARG_IN__("pipeline delay (Default 5)", uint16_t, 5);
	this->writeRegister(FEB::AllFEB|FEB::AllFPGA|FEB::Pipeline, hitPipelineDelay);
}

void ROCCosmicRayVetoInterface::SetLoopbackMode(__ARGS__) {
	int16_t mode = __GET_ARG_IN__("loopback mode (Default: 0)", int16_t, 0);
	this->writeRegister(ROC::LoopbackMode, mode);
}

void ROCCosmicRayVetoInterface::FebCMBENA(__ARGS__) {
	int16_t value = __GET_ARG_IN__("value (Default 1)", int16_t, 1);
	this->writeRegister(FEB::AllFEB|FEB::CMBENA, value);
}

void ROCCosmicRayVetoInterface::PWRRST(__ARGS__) {
	int16_t port = __GET_ARG_IN__("port (Default 25 - all)", int16_t, 25);
	this->writeRegister(ROC::PWRRST, port);
}

void ROCCosmicRayVetoInterface::GetHistograms(__ARGS__) {
	int port = __GET_ARG_IN__("port (Default: -1, current active)", int, -1);
        int16_t interval = __GET_ARG_IN__("interval (Default 2s) [ms]", int16_t, 2000);
        uint16_t fpga = __GET_ARG_IN__("fpga [0,1,2,3]", uint16_t);
        uint16_t channel = __GET_ARG_IN__("channel [0-15]", uint16_t);
        std::string filename = __GET_ARG_IN__("filename (Default: histogram.csv)", std::string, "histogram.csv");
        uint16_t nbins = __GET_ARG_IN__("number of bins (Default all: 0x400)", uint16_t, 0x400);

        uint16_t histRunParam = channel&0x7;
        if(channel&0x8) histRunParam+=0x40; else histRunParam+=0x20;

        if(port>0) SetActivePort(port);
	    this->writeRegister(FEB::FPGA[fpga]|FEB::HistInterval, interval);
	    this->writeRegister(FEB::FPGA[fpga]|FEB::HistRun, histRunParam);
	    this->writeRegister(FEB::FPGA[fpga]|(FEB::HistPointer + (channel&0x8 ? 0x1 : 0x0)), 0);

        std::stringstream o;
	    //o << "DEBUG: interval: " << interval << ", port: " << port << ", fpga: " << fpga << ", channel: " << channel << ", filename: " << filename << std::endl;
        //__SET_ARG_OUT__("buffer", o.str());

//        __SET_ARG_OUT__("buffer", "Waiting for histogram to be filled ...");  //is there a way to add additional text to the output window while the macro is running?
        //this->writeRegister(ROC::POOLENA, 0x0); // disaple the pooling
        TLOG(TLVL_DEBUG) << "Sleep for " << interval/1000 << "s to fill the histogram" << __E__;
        sleep(interval/1000);
        while(1) {
	    uint16_t histStatus = this->readRegister(FEB::FPGA[fpga]|FEB::HistRun);
        TLOG(TLVL_DEBUG) << "Test if the histogram is done: status = 0x" << std::hex << histStatus << __E__;
        if(histStatus&0x20 || histStatus&0x40) sleep(1);
        else break;
        }
//        __SET_ARG_OUT__("buffer", "Done!");

/*
//ReadBlock results in errors here and in the existing MacroMaker function
        std::vector<uint16_t> histMemory;
        this->readBlock(histMemory, FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ? 0x1 : 0x0)), 0x800, false);

        std::ofstream histFile;
        histFile.open(filename);
        for(size_t i=0; i<histMemory.size(); i+=2)
        {
          uint32_t binContent = (((uint32_t)histMemory.at(i))<<16) + histMemory.at(i+1);
          histFile << binContent << std::endl;
        }
        histFile.close();
*/
//ReadRegister results in errors here unless increasing the timeout in ROCCoreVInterface::readROCRegister to 1000ms
        std::ofstream histFile;
        histFile.open(filename);
        TLOG(TLVL_DEBUG) << "Open '" << filename << "' for histogram. Start read out." << __E__;
        for(size_t i=0; i<nbins*2; i+=2)
        {
            TLOG(TLVL_DEBUG) << "Read histogram bin " << i << __E__;
            //auto read0 = chrono::high_resolution_clock::now();
            //auto read1 = chrono::high_resolution_clock::now();
            //auto read2 = chrono::high_resolution_clock::now();
            //auto read3 = chrono::high_resolution_clock::now();
            uint32_t binContent = 0;
            try{
                auto read0 = chrono::high_resolution_clock::now();
                binContent = this->readRegister(FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ? 0x1 : 0x0))) << 16;
                auto read1 = chrono::high_resolution_clock::now();
                // read2 = chrono::high_resolution_clock::now();
                binContent += this->readRegister(FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ? 0x1 : 0x0)));
                auto read3 = chrono::high_resolution_clock::now();
                TLOG(TLVL_DEBUG) << "Read times " << chrono::duration_cast<chrono::milliseconds>(read1 - read0).count() << "ms, " 
                                                << chrono::duration_cast<chrono::milliseconds>(read3 - read1).count() << "ms" <<  __E__;
                histFile << binContent << std::endl;
                o << binContent << ",";
            } catch(...) {
                // assume one missed DCS from the FEB
                TLOG(TLVL_ERROR) << "Did we miss one DCS read from the FEB?" << __E__;
                //auto read4 = chrono::high_resolution_clock::now();
                //TLOG(TLVL_DEBUG) << "Read times " << chrono::duration_cast<chrono::milliseconds>(read1 - read0).count() << "ms, " 
                //                                  << chrono::duration_cast<chrono::milliseconds>(read3 - read2).count() << "ms, " 
                //                                  << chrono::duration_cast<chrono::milliseconds>(read4 - read0).count() << "ms (since start of iteration)" <<  __E__;
                //sleep(1.); // 20ms is a typical 
                //TLOG(TLVL_DEBUG) << "Waited 1s, try to read again" << __E__;
                //this->readRegister(FEB::FPGA[fpga]|(FEB::HistMemory + (channel&0x8 ? 0x1 : 0x0))); // to clean out
                //histFile << -1 << std::endl;
            }
        }
        histFile.close();
        __SET_ARG_OUT__("buffer", o.str());
        
}
// FEB related functions

uint32_t ROCCosmicRayVetoInterface::GetActivePorts() {
	uint32_t activeHigh = this->readRegister(ROC::ActivePortsHigh);
	uint32_t activeLow  = this->readRegister(ROC::ActivePortsLow);
    return (activeHigh << 16) | (activeLow);
}

void ROCCosmicRayVetoInterface::SetActivePort(uint16_t port, bool check) {
    if(check) {
        uint32_t active = GetActivePorts();
        if( !(active & (0x00000001<<(port-1))) ) { // throuw exception if selected port is not activr
            //std::stringstream ss;
            __FE_SS__  << "Error selecting port " << +port << ", port is not active: 0x" << std::hex << active;
			__SS_THROW__;
            //throw std::runtime_error(ss.str());
        }
    }
	this->writeRegister(ROC::LP, port);

    auto startTime = std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<std::chrono::milliseconds>(
		  std::chrono::high_resolution_clock::now() - startTime).count() < 1000) {
		try {
			auto activePort = this->readRegister(ROC::LP);
			if(activePort == port) {
                TLOG(TLVL_DEBUG) << "Port " << activePort << " is active (requested " << port << "). Took " 
				<< std::chrono::duration_cast<std::chrono::milliseconds>(
		           std::chrono::high_resolution_clock::now() - startTime).count() << " ms." << __E__;
                return;
			}
		} catch(...) {
			usleep(5000); // 5ms before retry
		}
	}
}

DEFINE_OTS_INTERFACE(ROCCosmicRayVetoInterface)
