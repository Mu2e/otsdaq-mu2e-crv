#ifndef _ots_CRVPoEControlInterface_h_
#define _ots_CRVPoEControlInterface_h_

#include <string>

#include "otsdaq/FECore/FEVInterface.h"

namespace ots
{
// Hardware-free front end that runs the CRV PoE injector control script
// (poe_ctl.py, the "crv-poe" alias) on the DCS host over ssh, so the CRV web
// page can power-cycle FEB ports from a button.
//
// The ssh uses the Kerberos ticket the ots launch saved under ots_ops/tmp,
// the same way the Subsystem Launch power buttons reach remote hosts.
//
// Optional fields on the FE interface row (defaults in the constructor):
//   PoEControlHost     ssh target host
//   PoEControlUser     ssh user on that host
//   PoEControlScript   command that runs poe_ctl.py on that host
class CRVPoEControlInterface : public FEVInterface
{
  public:
	CRVPoEControlInterface(const std::string&       interfaceUID,
	                       const ConfigurationTree& theXDAQContextConfigTree,
	                       const std::string&       interfaceConfigurationPath);
	virtual ~CRVPoEControlInterface(void);

	// state machine (nothing to do, no hardware)
	void configure(void) override;
	void halt(void) override;
	void pause(void) override;
	void resume(void) override;
	void start(std::string runNumber) override;
	void stop(void) override;
	bool running(void) override;

	void universalRead(char* address, char* returnValue) override;
	void universalWrite(char* address, char* writeValue) override;

	// FE macros — PoE injector control
	void PoECycle(__ARGS__);
	void PoEOff(__ARGS__);
	void PoEOn(__ARGS__);
	void PoEStatus(__ARGS__);

	// FE macros — ROC uC commands
	void CrvCmd(__ARGS__);
	void CrvCmdReset(__ARGS__);

  private:
	static std::string readOptionalField(
	    const ConfigurationTree& theXDAQContextConfigTree,
	    const std::string&       interfaceConfigurationPath,
	    const std::string&       fieldName,
	    const std::string&       defaultValue);
	static void validateSelector(const std::string& argumentName,
	                             const std::string& value,
	                             unsigned int       maxNumber);

	std::string buildRemoteCommand(const std::string& script,
	                               const std::string& scriptArguments) const;
	void        runRemoteCommand(const std::string&                script,
	                             const std::string&                scriptArguments,
	                             FEVInterface::frontEndMacroArgs_t argsOut);

	static void validateRocSelector(const std::string& value);

	std::string remoteHost_;
	std::string remoteUser_;
	std::string poeControlScript_;
	std::string rocCmdScript_;
};

}  // namespace ots

#endif
