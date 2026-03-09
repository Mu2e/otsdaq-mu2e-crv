#ifndef _ots_SubsystemCRVRocTable_h_
#define _ots_SubsystemCRVRocTable_h_

#include <cstdint>
#include <map>
#include <string>

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
// clang-format off
class SubsystemCRVRocTable : public TableBase
{

  public:
	SubsystemCRVRocTable(void);
	virtual ~SubsystemCRVRocTable(void);

	// Methods
	void init(ConfigurationManager* configManager);
	std::string getStructureAsJSON(const ConfigurationManager* configManager) override;
	virtual std::string getStatusTableInCSVFormat(const ConfigurationManager* configManager,
												  const std::string&          OfflineCxxClassName);
	void generateOfflineTableMap(const ConfigurationManager* configManager);


  private:

	std::map<std::string, std::string> mapOfflineTables_;
	std::map<uint16_t, uint16_t> mapChannels_;

};
// clang-format on
}  // namespace ots
#endif
