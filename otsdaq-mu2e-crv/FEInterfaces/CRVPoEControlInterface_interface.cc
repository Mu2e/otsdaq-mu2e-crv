#include "otsdaq-mu2e-crv/FEInterfaces/CRVPoEControlInterface.h"

#include <sys/wait.h>

#include <array>
#include <cstdio>

#include "otsdaq/Macros/InterfacePluginMacros.h"

using namespace ots;

namespace
{
const std::string DEFAULT_REMOTE_HOST       = "mu2e-dcs-01.fnal.gov";
const std::string DEFAULT_REMOTE_USER       = "mu2eshift";
const std::string DEFAULT_POE_CONTROL_SCRIPT = "python3 ~/CRVFirmware/scripts/poe_ctl.py";
const std::string DEFAULT_ROC_CMD_SCRIPT     = "python3 ~/CRVFirmware/scripts/rocCmd.py";

const std::string ARG_PORT     = "Port (1-24 or all)";
const std::string ARG_INJECTOR = "Injector (1, 2 or all)";
const std::string ARG_ROC      = "ROC (1, 2 or all)";
const std::string ARG_COMMAND  = "Command";

const unsigned int MAX_POE_PORT     = 24;
const unsigned int MAX_POE_INJECTOR = 9;
}  // namespace

//==============================================================================
CRVPoEControlInterface::CRVPoEControlInterface(
    const std::string&       interfaceUID,
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       interfaceConfigurationPath)
    : FEVInterface(interfaceUID, theXDAQContextConfigTree, interfaceConfigurationPath)
    , remoteHost_(readOptionalField(theXDAQContextConfigTree,
                                    interfaceConfigurationPath,
                                    "PoEControlHost",
                                    DEFAULT_REMOTE_HOST))
    , remoteUser_(readOptionalField(theXDAQContextConfigTree,
                                    interfaceConfigurationPath,
                                    "PoEControlUser",
                                    DEFAULT_REMOTE_USER))
    , poeControlScript_(readOptionalField(theXDAQContextConfigTree,
                                          interfaceConfigurationPath,
                                          "PoEControlScript",
                                          DEFAULT_POE_CONTROL_SCRIPT))
    , rocCmdScript_(readOptionalField(theXDAQContextConfigTree,
                                      interfaceConfigurationPath,
                                      "RocCmdScript",
                                      DEFAULT_ROC_CMD_SCRIPT))
{
	// The three settings end up inside a shell command line; refuse anything
	// that could break out of the quoting.
	for(const std::string* setting :
	    {&remoteHost_, &remoteUser_, &poeControlScript_, &rocCmdScript_})
		if(setting->find_first_of("'\"`$;&|<>\n") != std::string::npos)
		{
			__FE_SS__ << "Remote control setting '" << *setting
			          << "' contains a shell special character; refusing to use it."
			          << __E__;
			__FE_SS_THROW__;
		}

	__FE_COUT__ << "Remote control: " << remoteUser_ << "@" << remoteHost_ << "\n"
	            << "  PoE script: " << poeControlScript_ << "\n"
	            << "  ROC cmd:    " << rocCmdScript_ << __E__;

	registerFEMacroFunction(
	    "PoE Cycle",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::PoECycle),
	    std::vector<std::string>{ARG_PORT, ARG_INJECTOR},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Power-cycle PoE injector ports (off, wait, on) by running poe_ctl.py cycle on "
	    "the DCS host. FEB power is cut for the cycle delay (5 s by default). "
	    "Leave both inputs as Default for all ports on all injectors.");

	registerFEMacroFunction(
	    "PoE Off",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::PoEOff),
	    std::vector<std::string>{ARG_PORT, ARG_INJECTOR},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Switch PoE injector ports off (FEB power off) by running poe_ctl.py off on "
	    "the DCS host. Leave both inputs as Default for all ports on all injectors.");

	registerFEMacroFunction(
	    "PoE On",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::PoEOn),
	    std::vector<std::string>{ARG_PORT, ARG_INJECTOR},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Switch PoE injector ports on (FEB power on) by running poe_ctl.py on on "
	    "the DCS host. Leave both inputs as Default for all ports on all injectors.");

	registerFEMacroFunction(
	    "PoE Status",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::PoEStatus),
	    std::vector<std::string>{ARG_INJECTOR},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Read the PoE injector status by running poe_ctl.py status on the DCS host.");

	registerFEMacroFunction(
	    "CRV Cmd",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::CrvCmd),
	    std::vector<std::string>{ARG_COMMAND, ARG_ROC},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Send a command to the ROC uC console by running rocCmd.py on the DCS host. "
	    "The command string is forwarded verbatim (e.g. 'ub', 'rd 2').");

	registerFEMacroFunction(
	    "CRV Cmd Reset",
	    static_cast<FEVInterface::frontEndMacroFunction_t>(
	        &CRVPoEControlInterface::CrvCmdReset),
	    std::vector<std::string>{ARG_ROC},
	    std::vector<std::string>{"result", "exit status", "output", "command"},
	    1,    // requiredUserPermissions
	    "*",  // allowedCallingFEs
	    "Send the reset command ('wr 2 1') to the ROC uC by running rocCmd.py --reset "
	    "on the DCS host.");
}  // end constructor()

//==============================================================================
CRVPoEControlInterface::~CRVPoEControlInterface(void) {}

//==============================================================================
// readOptionalField
//	Reads a string field from the FE interface row; returns the default when the
//	field or the linked type table is not there.
std::string CRVPoEControlInterface::readOptionalField(
    const ConfigurationTree& theXDAQContextConfigTree,
    const std::string&       interfaceConfigurationPath,
    const std::string&       fieldName,
    const std::string&       defaultValue)
{
	try
	{
		std::string value = theXDAQContextConfigTree.getNode(interfaceConfigurationPath)
		                        .getNode(fieldName)
		                        .getValue<std::string>();
		if(!value.empty())
			return value;
	}
	catch(...)
	{
		// field not present, use the default
	}
	return defaultValue;
}  // end readOptionalField()

//==============================================================================
void CRVPoEControlInterface::configure(void)
{
	__FE_COUT__ << "Configure (no hardware); remote control via " << remoteUser_ << "@"
	            << remoteHost_ << __E__;
}

//==============================================================================
void CRVPoEControlInterface::halt(void) {}
void CRVPoEControlInterface::pause(void) {}
void CRVPoEControlInterface::resume(void) {}
void CRVPoEControlInterface::start(std::string /*runNumber*/) {}
void CRVPoEControlInterface::stop(void) {}
bool CRVPoEControlInterface::running(void) { return false; }

//==============================================================================
void CRVPoEControlInterface::universalRead(char* /*address*/, char* /*returnValue*/)
{
	__FE_SS__ << "Universal read not defined for the PoE control interface." << __E__;
	__FE_SS_THROW__;
}

//==============================================================================
void CRVPoEControlInterface::universalWrite(char* /*address*/, char* /*writeValue*/)
{
	__FE_SS__ << "Universal write not defined for the PoE control interface." << __E__;
	__FE_SS_THROW__;
}

//==============================================================================
// validateSelector
//	A port or injector selector is "all" or a number from 1 to maxNumber.
//	Anything else is refused, because the value goes onto a command line.
void CRVPoEControlInterface::validateSelector(const std::string& argumentName,
                                              const std::string& value,
                                              unsigned int       maxNumber)
{
	if(value == "all")
		return;

	if(value.empty() || value.size() > 2 ||
	   value.find_first_not_of("0123456789") != std::string::npos)
	{
		__SS__ << "Input '" << argumentName << "' must be 'all' or a number from 1 to "
		       << maxNumber << ", not '" << value << "'." << __E__;
		__SS_THROW__;
	}

	unsigned int number = std::stoul(value);
	if(number < 1 || number > maxNumber)
	{
		__SS__ << "Input '" << argumentName << "' must be from 1 to " << maxNumber
		       << ", not " << number << "." << __E__;
		__SS_THROW__;
	}
}  // end validateSelector()

//==============================================================================
std::string CRVPoEControlInterface::buildRemoteCommand(
    const std::string& script,
    const std::string& scriptArguments) const
{
	return "ssh -K -o BatchMode=yes -o ConnectTimeout=10 " + remoteUser_ + "@" +
	       remoteHost_ + " '" + script + " " + scriptArguments + "'";
}  // end buildRemoteCommand()

//==============================================================================
void CRVPoEControlInterface::runRemoteCommand(const std::string& script,
                                              const std::string& scriptArguments,
                                              FEVInterface::frontEndMacroArgs_t argsOut)
{
	std::string command = buildRemoteCommand(script, scriptArguments);
	__FE_COUT__ << "Running: " << command << __E__;

	std::string           output;
	std::array<char, 256> buffer;
	FILE*                 pipe = popen((command + " 2>&1").c_str(), "r");
	if(!pipe)
	{
		__FE_SS__ << "popen() failed for command: " << command << __E__;
		__FE_SS_THROW__;
	}
	while(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
		output += buffer.data();

	int rawStatus  = pclose(pipe);
	int exitStatus = -1;
	if(rawStatus != -1 && WIFEXITED(rawStatus))
		exitStatus = WEXITSTATUS(rawStatus);

	__FE_COUT__ << "Exit status " << exitStatus << ", output:\n" << output << __E__;

	__SET_ARG_OUT__("result",
	                exitStatus == 0 ? std::string("OK") : std::string("FAILED"));
	__SET_ARG_OUT__("exit status", exitStatus);
	__SET_ARG_OUT__("output", output);
	__SET_ARG_OUT__("command", command);
}  // end runRemoteCommand()

//==============================================================================
void CRVPoEControlInterface::PoECycle(__ARGS__)
{
	std::string port     = __GET_ARG_IN__(ARG_PORT, std::string, std::string("all"));
	std::string injector = __GET_ARG_IN__(ARG_INJECTOR, std::string, std::string("all"));
	validateSelector(ARG_PORT, port, MAX_POE_PORT);
	validateSelector(ARG_INJECTOR, injector, MAX_POE_INJECTOR);

	runRemoteCommand(poeControlScript_, "cycle " + port + " --id " + injector, argsOut);
}  // end PoECycle()

//==============================================================================
void CRVPoEControlInterface::PoEOff(__ARGS__)
{
	std::string port     = __GET_ARG_IN__(ARG_PORT, std::string, std::string("all"));
	std::string injector = __GET_ARG_IN__(ARG_INJECTOR, std::string, std::string("all"));
	validateSelector(ARG_PORT, port, MAX_POE_PORT);
	validateSelector(ARG_INJECTOR, injector, MAX_POE_INJECTOR);

	runRemoteCommand(poeControlScript_, "off " + port + " --id " + injector, argsOut);
}  // end PoEOff()

//==============================================================================
void CRVPoEControlInterface::PoEOn(__ARGS__)
{
	std::string port     = __GET_ARG_IN__(ARG_PORT, std::string, std::string("all"));
	std::string injector = __GET_ARG_IN__(ARG_INJECTOR, std::string, std::string("all"));
	validateSelector(ARG_PORT, port, MAX_POE_PORT);
	validateSelector(ARG_INJECTOR, injector, MAX_POE_INJECTOR);

	runRemoteCommand(poeControlScript_, "on " + port + " --id " + injector, argsOut);
}  // end PoEOn()

//==============================================================================
void CRVPoEControlInterface::PoEStatus(__ARGS__)
{
	std::string injector = __GET_ARG_IN__(ARG_INJECTOR, std::string, std::string("all"));
	validateSelector(ARG_INJECTOR, injector, MAX_POE_INJECTOR);

	runRemoteCommand(poeControlScript_, "status --id " + injector, argsOut);
}  // end PoEStatus()

//==============================================================================
void CRVPoEControlInterface::validateRocSelector(const std::string& value)
{
	if(value == "all" || value == "1" || value == "2")
		return;
	__SS__ << "Input '" << ARG_ROC << "' must be '1', '2' or 'all', not '" << value
	       << "'." << __E__;
	__SS_THROW__;
}  // end validateRocSelector()

//==============================================================================
void CRVPoEControlInterface::CrvCmd(__ARGS__)
{
	std::string command = __GET_ARG_IN__(ARG_COMMAND, std::string, std::string(""));
	std::string roc     = __GET_ARG_IN__(ARG_ROC, std::string, std::string("all"));
	validateRocSelector(roc);

	if(command.empty())
	{
		__FE_SS__ << "Command must not be empty." << __E__;
		__FE_SS_THROW__;
	}

	// Only allow alphanumerics, spaces and a few safe punctuation marks.
	if(command.find_first_not_of(
	       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ._-") !=
	   std::string::npos)
	{
		__FE_SS__ << "Command contains disallowed characters: '" << command << "'."
		          << __E__;
		__FE_SS_THROW__;
	}

	runRemoteCommand(rocCmdScript_, "-r " + roc + " " + command, argsOut);
}  // end CrvCmd()

//==============================================================================
void CRVPoEControlInterface::CrvCmdReset(__ARGS__)
{
	std::string roc = __GET_ARG_IN__(ARG_ROC, std::string, std::string("all"));
	validateRocSelector(roc);

	runRemoteCommand(rocCmdScript_, "--reset -r " + roc, argsOut);
}  // end CrvCmdReset()

DEFINE_OTS_INTERFACE(CRVPoEControlInterface)
