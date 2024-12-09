#ifndef _ots_ROCCosmicRayVetoInterface_h_
#define _ots_ROCCosmicRayVetoInterface_h_

#include <sstream>
#include <string>
// #include "ROC_Registers.h"
#include "dtcInterfaceLib/DTC.h"
#include "otsdaq-mu2e/ROCCore/ROCCoreVInterface.h"

namespace ots
{

class ROCCosmicRayVetoInterface : public ROCCoreVInterface
{
	// clang-format off
public:
	ROCCosmicRayVetoInterface(const std::string &rocUID,
							const ConfigurationTree &theXDAQContextConfigTree,
							const std::string &interfaceConfigurationPath);

	~ROCCosmicRayVetoInterface(void);

	// state machine
	//----------------
	void 									configure				(void) override;
	void 									halt					(void) override;
	void 									pause					(void) override;
	void 									resume					(void) override;
	void 									start					(std::string runNumber) override;
	void 									stop					(void) override;
	bool 									running					(void) override;

	// write and read to registers
	//virtual void 							writeROCRegister		(uint16_t address, uint16_t data_to_write) override;
	//virtual uint16_t  					readROCRegister			(uint16_t address) override;
	virtual void 							writeEmulatorRegister	(uint16_t address, uint16_t data_to_write) override;
	virtual uint16_t						readEmulatorRegister	(uint16_t address) override;

	// FEB realted functions
	uint32_t GetActivePorts();
	void SetActivePort(uint16_t port, bool check = true); // starts at 1, max 24                     

	//virtual void 							readROCBlock			(std::vector<uint16_t>& data, uint16_t address, uint16_t numberOfReads, bool incrementAddress) override { }
	//virtual void 							readEmulatorBlock		(std::vector<uint16_t>& data, uint16_t address, uint16_t numberOfReads, bool incrementAddress) override { }


	// specific ROC functions
	//virtual int  							readTimestamp			(void) ;
	//virtual void 							writeDelay				(uint16_t delay) override;  // 5ns steps
	//virtual int  							readDelay				(void) override;            // 5ns steps

	//virtual int  							readDTCLinkLossCounter	(void) override;
	//virtual void 							resetDTCLinkLossCounter	(void) override;

    // CRV ROC specific functions
    void Reset();
	void RocConfigure(bool gr=false);
	void FebConfigure();
	void ResetTxBuffers();

public:
	void 									DoTheCRV_Dance			(__ARGS__);
	void                                    GetFirmwareVersion 	    (__ARGS__);
	void                                    GetTestCounter        	(__ARGS__);
	void                                    SetTestCounter        	(__ARGS__);
	void                                    HardReset             	(__ARGS__);
	void                                    SoftReset            	(__ARGS__);
	void                                    RocConfigure        	(__ARGS__);
	void                                    GetStatus            	(__ARGS__);
	void                                    FiberRx                 (__ARGS__);
	void                                    FiberTx                 (__ARGS__);
	void                                    SetLoopbackMode         (__ARGS__);
	//void                                    FebGetStatus            (__ARGS__);
	void                                    FebSetBias              (__ARGS__);
	void                                    FebSetBiasTrim          (__ARGS__);
	void                                    FebSetThreshold         (__ARGS__);
	void                                    FebSetPipeline          (__ARGS__);
    void                                    FebCMBENA               (__ARGS__);
    void                                    PWRRST                  (__ARGS__);
    void                                    GetHistograms           (__ARGS__);
	// clang-format on
};

}  // namespace ots

#endif
