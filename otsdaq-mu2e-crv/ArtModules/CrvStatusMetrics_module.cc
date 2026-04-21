// CRV ROC Status Header metrics module
// Unpacks CRV ROC status headers from DTC fragments
// and publishes metrics via the artdaq MetricManager.
//
// Per-event metrics (LastPoint):
//   CRV.ROC[N].TriggerCount       - trigger count from ROC N
//   CRV.ROC[N].EventWindowTag     - event window tag from ROC N
//   CRV.ROC[N].ActiveFEBCount     - number of active FEBs on ROC N
//   CRV.ROC[N].MicroBunchStatus   - microbunch status word
//   CRV.ROC[N].WordCount          - controller event word count

#include <bitset>
#include <string>
#include <vector>

// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

// artdaq includes
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

// artdaq metric manager
#include "artdaq-utilities/Plugins/MetricManager.hh"
#include "artdaq/DAQdata/Globals.hh"

namespace ots
{

class CrvStatusMetrics : public art::EDAnalyzer
{
  public:
	explicit CrvStatusMetrics(fhicl::ParameterSet const& ps);
	~CrvStatusMetrics() override = default;

  private:
	void beginJob() override;
	void analyze(art::Event const& e) override;
	void endJob() override;

	// Send a metric safely (guards against null metricMan)
	template<typename T>
	void sendMetric(const std::string& name,
	                T                  value,
	                const std::string& units,
	                int                level,
	                artdaq::MetricMode mode) const;

	// fhicl parameters
	int diagLevel_;
	int metricLevel_;  // reporting level passed to MetricManager

	// Job-level counters
	size_t eventCount_{0};

	std::string outputPrefix_;
};

// -----------------------------------------------------------------------
CrvStatusMetrics::CrvStatusMetrics(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , metricLevel_(ps.get<int>("metricLevel", 3))
{
	outputPrefix_ = "[CrvStatusMetrics] ";
}

// -----------------------------------------------------------------------
void CrvStatusMetrics::beginJob()
{
	if(diagLevel_ > 0)
	{
		std::cout << outputPrefix_ << "beginJob: diagLevel=" << diagLevel_
		          << " metricLevel=" << metricLevel_ << std::endl;
	}
}

// -----------------------------------------------------------------------
template<typename T>
void CrvStatusMetrics::sendMetric(const std::string& name,
                                  T                  value,
                                  const std::string& units,
                                  int                level,
                                  artdaq::MetricMode mode) const
{
	if(metricMan != nullptr)
	{
		metricMan->sendMetric(name, value, units, level, mode);
	}
}

// -----------------------------------------------------------------------
void CrvStatusMetrics::analyze(art::Event const& e)
{
	++eventCount_;

	// ---- Collect fragments (same pattern as CrvPrint) ------------------
	std::vector<art::Handle<artdaq::Fragments>> fragmentHandles =
	    e.getMany<std::vector<artdaq::Fragment>>();

	artdaq::FragmentPtrs containerFragments;
	artdaq::Fragments    fragments;

	for(const auto& handle : fragmentHandles)
	{
		if(!handle.isValid() || handle->empty())
			continue;

		if(handle->front().type() == artdaq::Fragment::ContainerFragmentType)
		{
			for(const auto& cont : *handle)
			{
				artdaq::ContainerFragment contf(cont);
				if(contf.fragment_type() != mu2e::FragmentType::DTCEVT)
					continue;
				for(size_t i = 0; i < contf.block_count(); ++i)
				{
					containerFragments.push_back(contf[i]);
					fragments.push_back(*containerFragments.back());
				}
			}
		}
		else if(handle->front().type() == mu2e::FragmentType::DTCEVT)
		{
			for(const auto& frag : *handle)
				fragments.emplace_back(frag);
		}
	}

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << e.id() << " - " << fragments.size()
		          << " DTC fragments" << std::endl;
	}

	// ---- Process each fragment -----------------------------------------
	for(const auto& frag : fragments)
	{
		try
		{
			mu2e::DTCEventFragment dtcFrag(frag);
			DTCLib::DTC_Event      dtcEvent = dtcFrag.getData();

			for(unsigned int iSub = 0; iSub < dtcEvent.GetSubEventCount(); ++iSub)
			{
				DTCLib::DTC_SubEvent& subevent = *(dtcEvent.GetSubEvent(iSub));

				// One decoder per subevent
				mu2e::CRVDataDecoder decoder(subevent);

				for(size_t iBlock = 0; iBlock < subevent.GetDataBlockCount(); ++iBlock)
				{
					auto blockHeader = subevent.GetDataBlock(iBlock)->GetHeader();

					// Only process CRV blocks (subsystem == 2)
					if(blockHeader->GetSubsystem() != 2)
						continue;

					if(diagLevel_ > 1)
					{
						std::cout
						    << outputPrefix_ << "  CRV block [" << iBlock
						    << "] version=0x" << std::hex
						    << (int)blockHeader->GetVersion() << std::dec
						    << " linkID=" << (int)blockHeader->GetLinkID() << " EWT="
						    << blockHeader->GetEventWindowTag().GetEventWindowTag(true)
						    << std::endl;
					}

					// Metric name prefix keyed by DTC id and link id
					const std::string rocPrefix =
					    "CRV.DTC" +
					    std::to_string(static_cast<int>(blockHeader->GetID())) + ".ROC" +
					    std::to_string(static_cast<int>(blockHeader->GetLinkID())) + ".";

					// ---- Decode FEB-II status packet -------------------------
					const mu2e::CRVDataDecoder::CRVROCStatusPacketFEBII* status =
					    decoder.GetCRVROCStatusPacketFEBII(iBlock);

					if(status != nullptr)
					{
						const uint32_t ewt       = status->GetEventWindowTag();
						const uint16_t trigCount = status->TriggerCount;
						const uint16_t wordCount = status->ControllerEventWordCount;
						const uint32_t ubStatus  = status->GetMicroBunchStatus();
						const std::bitset<24> activeFEBs = status->GetActiveFEBFlags();
						const int nActiveFEBs = static_cast<int>(activeFEBs.count());

						if(diagLevel_ > 0)
						{
							std::cout << outputPrefix_ << "  " << rocPrefix
							          << "TriggerCount=" << trigCount << " EWT=" << ewt
							          << " ActiveFEBs=" << activeFEBs.to_string() << " ("
							          << nActiveFEBs << " active)"
							          << " MicroBunchStatus=0x" << std::hex << ubStatus
							          << std::dec << " WordCount=" << wordCount
							          << std::endl;
						}

						sendMetric(rocPrefix + "TriggerCount",
						           static_cast<uint64_t>(trigCount),
						           "counts",
						           metricLevel_,
						           artdaq::MetricMode::LastPoint);

						sendMetric(rocPrefix + "EventWindowTag",
						           static_cast<uint64_t>(ewt),
						           "EWT",
						           metricLevel_,
						           artdaq::MetricMode::LastPoint);

						sendMetric(rocPrefix + "ActiveFEBCount",
						           static_cast<uint64_t>(nActiveFEBs),
						           "FEBs",
						           metricLevel_,
						           artdaq::MetricMode::LastPoint);

						sendMetric(rocPrefix + "MicroBunchStatus",
						           static_cast<uint64_t>(ubStatus),
						           "status",
						           metricLevel_,
						           artdaq::MetricMode::LastPoint);

						sendMetric(rocPrefix + "WordCount",
						           static_cast<uint64_t>(wordCount),
						           "words",
						           metricLevel_,
						           artdaq::MetricMode::LastPoint);
					}
					else if(diagLevel_ > 1)
					{
						std::cout << outputPrefix_
						          << "  No FEB-II status packet for CRV block [" << iBlock
						          << "]" << std::endl;
					}
				}  // iBlock
			}      // iSub
		}
		catch(const std::exception& ex)
		{
			if(diagLevel_ > 0)
			{
				std::cerr << outputPrefix_
				          << "Exception processing fragment: " << ex.what() << std::endl;
			}
		}
	}  // fragments
}

// -----------------------------------------------------------------------
void CrvStatusMetrics::endJob()
{
	std::cout << outputPrefix_ << "========== End Job Summary ==========" << std::endl;
	std::cout << outputPrefix_ << "Events processed: " << eventCount_ << std::endl;
	std::cout << outputPrefix_ << "=====================================" << std::endl;
}

DEFINE_ART_MODULE(ots::CrvStatusMetrics)
}  // namespace ots
