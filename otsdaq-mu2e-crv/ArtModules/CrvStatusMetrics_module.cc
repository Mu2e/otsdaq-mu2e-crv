// CRV ROC Status Header metrics module
// Fills mu2e::CRVStatusDQM (histograms + firmware error bits) and publishes
// the same LastPoint scalars as before via the artdaq MetricManager.
//
// Per-event metrics (LastPoint) — names unchanged:
//   CRV.DTC<n>.ROC<m>.TriggerCount
//   CRV.DTC<n>.ROC<m>.EventWindowTag
//   CRV.DTC<n>.ROC<m>.ActiveFEBCount
//   CRV.DTC<n>.ROC<m>.MicroBunchStatus
//   CRV.DTC<n>.ROC<m>.WordCount
//
// Prefers CrvStatus / CrvDAQerror products when present (after unpack).
// Falls back to DTC-fragment decode for LastPoint if the product is missing.

#include <bitset>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Services/Registry/ServiceRegistry.h"
#include "art_root_io/TFileService.h"

#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/Decoders/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-utilities/Plugins/MetricManager.hh"
#include "artdaq/DAQdata/Globals.hh"

#include "otsdaq-mu2e/ArtModules/HistoSender.hh"

#include "Offline/DQMHelpers/inc/CRVStatusDQM.hh"
#include "Offline/RecoDataProducts/inc/CrvDAQerror.hh"
#include "Offline/RecoDataProducts/inc/CrvStatus.hh"

#include "TH1.h"

namespace ots
{

class CrvStatusMetrics : public art::EDAnalyzer
{
  public:
	explicit CrvStatusMetrics(fhicl::ParameterSet const& ps);
	~CrvStatusMetrics() override = default;

  private:
	static mu2e::CRVStatusDQM::Config makeHelperConfig(fhicl::ParameterSet const& ps);

	void beginJob() override;
	void analyze(art::Event const& e) override;
	void endSubRun(art::SubRun const& sr) override;
	void endJob() override;

	void Send();
	void sendLastPointFromHelper();
	void fillLastPointFromFragments(art::Event const& e);

	template<typename T>
	void sendMetric(const std::string& name,
	                T                  value,
	                const std::string& units,
	                int                level,
	                artdaq::MetricMode mode) const;

	art::InputTag crvStatusTag_;
	art::InputTag crvDaqErrorTag_;
	int           diagLevel_;
	int           metricLevel_;
	std::string   outputTag_;
	bool          sendHists_;
	int           port_;
	std::string   address_;
	float         sendIntervalSec_;

	std::unique_ptr<HistoSender> histoSender_;
	std::chrono::time_point<std::chrono::steady_clock> lastSendTime_;

	mu2e::CRVStatusDQM dqm_;

	size_t      eventCount_{0};
	std::string outputPrefix_;
};

mu2e::CRVStatusDQM::Config CrvStatusMetrics::makeHelperConfig(
    fhicl::ParameterSet const& ps)
{
	mu2e::CRVStatusDQM::Config c;
	c.nBinsLatency      = ps.get<int>("nBinsLatency", 1024);
	c.maxLinkLatency    = ps.get<float>("maxLinkLatency", 4096.f);
	c.nBinsTriggerCount = ps.get<int>("nBinsTriggerCount", 256);
	c.maxTriggerCount   = ps.get<float>("maxTriggerCount", 65535.f);
	c.nBinsWordCount    = ps.get<int>("nBinsWordCount", 256);
	c.maxWordCount      = ps.get<float>("maxWordCount", 65535.f);
	c.nBinsEwtMismatch  = ps.get<int>("nBinsEwtMismatch", 201);
	c.maxEwtMismatch    = ps.get<float>("maxEwtMismatch", 100.f);
	return c;
}

CrvStatusMetrics::CrvStatusMetrics(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , crvStatusTag_(ps.get<std::string>("crvStatusTag", "crvdigi"))
    , crvDaqErrorTag_(ps.get<std::string>("crvDaqErrorTag", "crvdigi"))
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , metricLevel_(ps.get<int>("metricLevel", 3))
    , outputTag_(ps.get<std::string>("outputTag", "CRVStatusDQM"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5f))
    , dqm_(makeHelperConfig(ps))
{
	outputPrefix_ = "[CrvStatusMetrics] ";
}

void CrvStatusMetrics::beginJob()
{
	if(diagLevel_ > 0)
	{
		std::cout << outputPrefix_ << "beginJob: diagLevel=" << diagLevel_
		          << " metricLevel=" << metricLevel_
		          << " sendHists=" << sendHists_ << std::endl;
	}

	if(sendHists_)
	{
		try
		{
			histoSender_ = std::make_unique<HistoSender>(address_, port_);
			std::cout << outputPrefix_ << "Successfully connected HistoSender to "
			          << address_ << ":" << port_ << std::endl;
		}
		catch(const std::exception& e)
		{
			std::cout << outputPrefix_ << "Failed to initialise HistoSender: " << e.what()
			          << std::endl;
			sendHists_ = false;
		}
	}

	if(art::ServiceRegistry::isAvailable<art::TFileService>())
	{
		art::ServiceHandle<art::TFileService> tfs;
		dqm_.Book(tfs->mkdir(outputTag_));
	}
	else if(diagLevel_ > 0)
	{
		std::cout << outputPrefix_
		          << "No TFileService; histograms disabled, LastPoint still sent"
		          << std::endl;
	}

	lastSendTime_ = std::chrono::steady_clock::now();
}

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

void CrvStatusMetrics::sendLastPointFromHelper()
{
	for(const auto& snap : dqm_.lastEventRocs())
	{
		const std::string rocPrefix = "CRV.DTC" +
		                              std::to_string(static_cast<int>(snap.dtcId)) +
		                              ".ROC" +
		                              std::to_string(static_cast<int>(snap.linkId)) + ".";

		if(diagLevel_ > 0)
		{
			std::cout << outputPrefix_ << "  " << rocPrefix
			          << "TriggerCount=" << snap.triggerCount << " EWT=" << snap.ewt
			          << " ActiveFEBs=" << snap.activeFebCount
			          << " MicroBunchStatus=0x" << std::hex << snap.microBunchStatus
			          << std::dec << " WordCount=" << snap.wordCount << std::endl;
		}

		sendMetric(rocPrefix + "TriggerCount",
		           static_cast<uint64_t>(snap.triggerCount),
		           "counts",
		           metricLevel_,
		           artdaq::MetricMode::LastPoint);
		sendMetric(rocPrefix + "EventWindowTag",
		           static_cast<uint64_t>(snap.ewt),
		           "EWT",
		           metricLevel_,
		           artdaq::MetricMode::LastPoint);
		sendMetric(rocPrefix + "ActiveFEBCount",
		           static_cast<uint64_t>(snap.activeFebCount),
		           "FEBs",
		           metricLevel_,
		           artdaq::MetricMode::LastPoint);
		sendMetric(rocPrefix + "MicroBunchStatus",
		           static_cast<uint64_t>(snap.microBunchStatus),
		           "status",
		           metricLevel_,
		           artdaq::MetricMode::LastPoint);
		sendMetric(rocPrefix + "WordCount",
		           static_cast<uint64_t>(snap.wordCount),
		           "words",
		           metricLevel_,
		           artdaq::MetricMode::LastPoint);
	}
}

void CrvStatusMetrics::fillLastPointFromFragments(art::Event const& e)
{
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
		          << " DTC fragments (LastPoint fallback)" << std::endl;
	}

	for(const auto& frag : fragments)
	{
		try
		{
			mu2e::DTCEventFragment dtcFrag(frag);
			DTCLib::DTC_Event      dtcEvent = dtcFrag.getData();

			for(unsigned int iSub = 0; iSub < dtcEvent.GetSubEventCount(); ++iSub)
			{
				DTCLib::DTC_SubEvent& subevent = *(dtcEvent.GetSubEvent(iSub));
				mu2e::CRVDataDecoder  decoder(subevent);

				for(size_t iBlock = 0; iBlock < subevent.GetDataBlockCount(); ++iBlock)
				{
					auto blockHeader = subevent.GetDataBlock(iBlock)->GetHeader();
					if(blockHeader->GetSubsystem() !=
					   DTCLib::DTC_Subsystem::DTC_Subsystem_CRV)
						continue;

					const std::string rocPrefix =
					    "CRV.DTC" +
					    std::to_string(static_cast<int>(blockHeader->GetID())) + ".ROC" +
					    std::to_string(static_cast<int>(blockHeader->GetLinkID())) + ".";

					const mu2e::CRVDataDecoder::CRVROCStatusPacketFEBII* status =
					    decoder.GetCRVROCStatusPacketFEBII(iBlock);

					if(status == nullptr)
						continue;

					const uint32_t        ewt       = status->GetEventWindowTag();
					const uint16_t        trigCount = status->TriggerCount;
					const uint16_t        wordCount = status->ControllerEventWordCount;
					const uint32_t        ubStatus  = status->GetMicroBunchStatus();
					const std::bitset<24> activeFEBs = status->GetActiveFEBFlags();
					const int nActiveFEBs = static_cast<int>(activeFEBs.count());

					if(diagLevel_ > 0)
					{
						std::cout << outputPrefix_ << "  " << rocPrefix
						          << "TriggerCount=" << trigCount << " EWT=" << ewt
						          << " ActiveFEBs=" << activeFEBs.to_string() << " ("
						          << nActiveFEBs << " active)"
						          << " MicroBunchStatus=0x" << std::hex << ubStatus
						          << std::dec << " WordCount=" << wordCount << std::endl;
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
			}
		}
		catch(const std::exception& ex)
		{
			if(diagLevel_ > 0)
			{
				std::cerr << outputPrefix_
				          << "Exception processing fragment: " << ex.what() << std::endl;
			}
		}
	}
}

void CrvStatusMetrics::analyze(art::Event const& e)
{
	++eventCount_;

	art::Handle<mu2e::CrvStatusCollection> statusHandle;
	e.getByLabel(crvStatusTag_, statusHandle);
	const bool haveProduct =
	    statusHandle.isValid() && statusHandle.product() != nullptr;

	if(haveProduct)
	{
		art::Handle<mu2e::CrvDAQerrorCollection> daqHandle;
		e.getByLabel(crvDaqErrorTag_, daqHandle);
		if(daqHandle.isValid() && daqHandle.product() != nullptr)
		{
			dqm_.Fill(*statusHandle, *daqHandle);
		}
		else
		{
			dqm_.Fill(*statusHandle);
		}
		sendLastPointFromHelper();
	}
	else
	{
		if(diagLevel_ > 1)
		{
			std::cout << outputPrefix_ << e.id()
			          << " no CrvStatus at " << crvStatusTag_
			          << "; LastPoint from fragments" << std::endl;
		}
		fillLastPointFromFragments(e);
	}

	auto                             currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed     = currentTime - lastSendTime_;
	if(elapsed.count() >= sendIntervalSec_)
	{
		Send();
		lastSendTime_ = currentTime;
	}
}

void CrvStatusMetrics::Send()
{
	if(!sendHists_ || histoSender_ == nullptr)
		return;
	if(!dqm_.booked())
		return;

	std::map<std::string, std::vector<TH1*>> hists;
	if(dqm_.errorBitsVsRoc())
		hists["crv/errorBitsVsRoc:replace"] = {dqm_.errorBitsVsRoc()};
	if(dqm_.errorBits())
		hists["crv/errorBits:replace"] = {dqm_.errorBits()};
	if(dqm_.nRocHeaders())
		hists["crv/nRocHeaders:replace"] = {dqm_.nRocHeaders()};
	if(dqm_.activeFebCount())
		hists["crv/activeFebCount:replace"] = {dqm_.activeFebCount()};
	if(dqm_.linkLatency())
		hists["crv/linkLatency:replace"] = {dqm_.linkLatency()};
	if(dqm_.portFlags())
		hists["crv/portFlags:replace"] = {dqm_.portFlags()};
	if(dqm_.daqErrorCode())
		hists["crv/daqErrorCode:replace"] = {dqm_.daqErrorCode()};
	if(dqm_.eventHasError())
		hists["crv/eventHasError:replace"] = {dqm_.eventHasError()};
	if(dqm_.rocCensus())
		hists["crv/rocCensus:replace"] = {dqm_.rocCensus()};
	if(dqm_.triggerCount())
		hists["crv/triggerCount:replace"] = {dqm_.triggerCount()};
	if(dqm_.wordCount())
		hists["crv/wordCount:replace"] = {dqm_.wordCount()};

	for(const auto& [key, h] : dqm_.linkLatencyByRoc())
	{
		if(h == nullptr)
			continue;
		hists["crv/linkLatencyByRoc:replace"].push_back(h);
	}

	histoSender_->sendHistograms(hists);

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Sent histograms to " << address_ << ":" << port_
		          << std::endl;
	}
}

void CrvStatusMetrics::endSubRun(art::SubRun const& sr)
{
	dqm_.EndSubRun(static_cast<int>(sr.run()), static_cast<int>(sr.subRun()));
}

void CrvStatusMetrics::endJob()
{
	dqm_.WriteGraphs();
	if(sendHists_ && histoSender_ != nullptr)
	{
		Send();
		histoSender_.reset();
	}

	std::cout << outputPrefix_ << "========== End Job Summary ==========" << std::endl;
	std::cout << outputPrefix_ << "Events processed: " << eventCount_ << std::endl;
	std::cout << outputPrefix_ << "Helper events: " << dqm_.nEvents() << std::endl;
	std::cout << outputPrefix_ << "Events with ROC header: "
	          << dqm_.nEventsWithRocHeader() << std::endl;
	std::cout << outputPrefix_ << "Events with firmware error bit: "
	          << dqm_.nEventsWithAnyErrorBit() << std::endl;
	std::cout << outputPrefix_ << "Distinct ROCs: " << dqm_.seenRocs().size()
	          << std::endl;
	for(int b = 0; b < mu2e::CRVStatusDQM::kNErrorBits; ++b)
	{
		std::cout << outputPrefix_ << "  " << mu2e::CRVStatusDQM::errorBitLabel(b)
		          << ": " << dqm_.errorBitCount(b) << std::endl;
	}
	std::cout << outputPrefix_ << "=====================================" << std::endl;
}

DEFINE_ART_MODULE(ots::CrvStatusMetrics)
} // namespace ots
