#include "otsdaq-mu2e-crv/TablePlugins/SubsystemCRVRocTable.h"
#include "otsdaq/Macros/TablePluginMacros.h"

#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/file.h>

using namespace ots;

namespace
{
std::string getCRVDummyCSVFormat(std::map<uint16_t, uint16_t>& mapChannels)
{
	mapChannels.clear();

	std::stringstream offlineTable;
	offlineTable << "TABLE CRVBadChan" << __E__;

	for(uint16_t i = 0; i < 16; ++i)
	{
		mapChannels[i] = i;
		offlineTable << i << ", "
		             << "1"
		             << "\n";
	}

	return offlineTable.str();
}
}  // namespace

//==============================================================================
SubsystemCRVRocTable::SubsystemCRVRocTable(void) : TableBase("SubsystemCRVRocTable") {}

//==============================================================================
SubsystemCRVRocTable::~SubsystemCRVRocTable(void) {}

//==============================================================================
void SubsystemCRVRocTable::init(ConfigurationManager* configManager)
{
	isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();
	if(!isFirstAppInContext_)
		return;

	generateOfflineTableMap(configManager);

	const std::string dbserviceOnlinePath =
	    getenv("DBSERVICE_ONLINE_PATH") ? getenv("DBSERVICE_ONLINE_PATH") : "";
	if(dbserviceOnlinePath.empty())
		return;

	// Grab a non-blocking exclusive flock so only one context writes the files.
	// The lock is held for the lifetime of the process; subsequent contexts skip.
	const std::string lockFilePath = dbserviceOnlinePath + "/SubsystemCRVRocTable.lock";
	int lockFd = open(lockFilePath.c_str(), O_CREAT | O_WRONLY, 0644);
	if(lockFd < 0 || flock(lockFd, LOCK_EX | LOCK_NB) != 0)
	{
		if(lockFd >= 0)
			close(lockFd);
		__COUT__ << "SubsystemCRVRocTable: lock already held by another supervisor, skipping file write." << __E__;
		return;
	}
	// lockFd left open intentionally — released automatically when process exits.

	for(const auto& offlineTable : mapOfflineTables_)
	{
		const std::string fileName =
		    dbserviceOnlinePath + "/" + offlineTable.first + ".txt";
		std::ofstream out(fileName);
		if(!out)
		{
			__SS__ << "Failed to open file: " << fileName << __E__;
			__SS_THROW__;
		}
		out << offlineTable.second;
	}
}

//==============================================================================
void SubsystemCRVRocTable::generateOfflineTableMap(
    const ConfigurationManager* /*configManager*/)
{
	mapOfflineTables_.clear();
	mapOfflineTables_["CRVDummy"] = getCRVDummyCSVFormat(mapChannels_);
}

//==============================================================================
std::string SubsystemCRVRocTable::getStatusTableInCSVFormat(
    const ConfigurationManager* /*configManager*/, const std::string& OfflineCxxClassName)
{
	std::stringstream out;
	out << "TABLE " << OfflineCxxClassName << __E__;
	for(const auto& channelPair : mapChannels_)
		out << channelPair.first << "," << channelPair.second << "\n";
	return out.str();
}

//==============================================================================
std::string SubsystemCRVRocTable::getStructureAsJSON(
    const ConfigurationManager* configManager)
{
	if(mapOfflineTables_.empty())
		generateOfflineTableMap(configManager);

	std::stringstream out;
	out << "{";
	out << "\"DBServiceTables\": {";

	for(auto it = mapOfflineTables_.begin(); it != mapOfflineTables_.end(); ++it)
	{
		out << "\"" << it->first << "\": \""
		    << StringMacros::escapeJSONStringEntities(it->second) << "\"";
		if(std::next(it) != mapOfflineTables_.end())
			out << ",";
	}

	out << "}";
	out << "}";
	return out.str();
}

DEFINE_OTS_TABLE(SubsystemCRVRocTable)
