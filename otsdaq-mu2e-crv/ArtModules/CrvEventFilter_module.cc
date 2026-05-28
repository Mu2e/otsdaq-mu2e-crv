#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "TRACE/tracemf.h"
#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME "CrvEventFilter"

#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ots
{
class CrvEventFilter : public art::EDFilter
{
  public:
	// clang-format off
	struct Config
	{
		fhicl::Atom<unsigned> debug_every
		    {fhicl::Name("debugEvery"),
		     fhicl::Comment("Print running stats every N events (0 disables periodic prints)"),
		     10000};
		fhicl::Atom<unsigned> status_packets_per_block
		    {fhicl::Name("statusPacketsPerBlock"),
		     fhicl::Comment("Number of status packets expected per CRV block (subtracted from packet count)"),
		     1};
		fhicl::Atom<unsigned> min_extra_packets
		    {fhicl::Name("minExtraPackets"),
		     fhicl::Comment("Require at least this many non-status packets in any CRV block"),
		     5};
		fhicl::Atom<bool> print_histogram
		    {fhicl::Name("printHistogram"),
		     fhicl::Comment("Print extra-packet histogram in periodic and endJob logs"),
		     false};
	};
	// clang-format on

	explicit CrvEventFilter(const art::EDFilter::Table<Config>& config);

	void beginJob() override;
	void endJob() override;
	bool filter(art::Event& event) override;

  private:
	artdaq::Fragments getFragments(art::Event& event);
	std::string       buildHistogram_(const std::vector<size_t>& hist) const;
	void              resetInterval_();

	unsigned debugEvery_{};
	unsigned statusPacketsPerBlock_{};
	unsigned minExtraPackets_{};
	bool     printHistogram_{};

	// cumulative (reported in endJob)
	size_t              eventsSeen_{};
	size_t              eventsPassed_{};
	size_t              eventsWithDtcevt_{};
	size_t              totalCrvBlocks_{};
	size_t              totalPackets_{};
	size_t              totalExtraPackets_{};
	unsigned            maxPacketsInBlock_{};
	std::vector<size_t> totalExtraHist_;  // bin width=5, index=extraPkts/5

	// interval (reset after each periodic print)
	size_t              ivBlocks_{};
	size_t              ivPackets_{};
	size_t              ivExtraPackets_{};
	size_t              ivMaxSum_{};            // sum of per-event max packet count
	size_t              ivEventsWithBlocks_{};  // events with >=1 CRV block in interval
	std::vector<size_t> ivExtraHist_;
};

CrvEventFilter::CrvEventFilter(const art::EDFilter::Table<Config>& config)
    : art::EDFilter{config}
{
	debugEvery_            = config().debug_every();
	statusPacketsPerBlock_ = config().status_packets_per_block();
	minExtraPackets_       = config().min_extra_packets();
	printHistogram_        = config().print_histogram();
}

void CrvEventFilter::beginJob()
{
	TLOG(TLVL_INFO) << "CrvEventFilter beginJob: debugEvery=" << debugEvery_
	                << ", statusPacketsPerBlock=" << statusPacketsPerBlock_
	                << ", minExtraPackets=" << minExtraPackets_
	                << ", printHistogram=" << (printHistogram_ ? "true" : "false");
}

void CrvEventFilter::endJob()
{
	const double passRate = eventsSeen_ > 0 ? 100.0 * eventsPassed_ / eventsSeen_ : 0.0;
	TLOG(TLVL_INFO) << "CrvEventFilter summary: seen=" << eventsSeen_
	                << ", withDTCEVT=" << eventsWithDtcevt_
	                << ", passed=" << eventsPassed_ << " (" << std::fixed
	                << std::setprecision(1) << passRate << "%)"
	                << ", crvBlocks=" << totalCrvBlocks_ << ", sumPkts=" << totalPackets_
	                << ", sumExtraPkts=" << totalExtraPackets_
	                << ", maxPktsInBlock=" << maxPacketsInBlock_
	                << (printHistogram_ ? buildHistogram_(totalExtraHist_) : "");
}

bool CrvEventFilter::filter(art::Event& event)
{
	++eventsSeen_;

	auto fragments = getFragments(event);
	if(!fragments.empty())
		++eventsWithDtcevt_;

	bool     pass           = false;
	unsigned eventMaxPkts   = 0;
	bool     eventHasBlocks = false;

	for(const auto& frag : fragments)
	{
		mu2e::DTCEventFragment eventFragment(frag);
		const auto&            crvSubEvents =
		    eventFragment.getSubsystemData(DTCLib::DTC_Subsystem::DTC_Subsystem_CRV);

		for(const auto& subevent : crvSubEvents)
		{
			for(const auto& dataBlock : subevent.GetDataBlocks())
			{
				const unsigned packetCount  = dataBlock.GetHeader()->GetPacketCount();
				const unsigned extraPackets = (packetCount > statusPacketsPerBlock_)
				                                  ? (packetCount - statusPacketsPerBlock_)
				                                  : 0u;

				eventHasBlocks = true;
				if(packetCount > eventMaxPkts)
					eventMaxPkts = packetCount;

				// cumulative
				++totalCrvBlocks_;
				totalPackets_ += packetCount;
				totalExtraPackets_ += extraPackets;
				if(packetCount > maxPacketsInBlock_)
					maxPacketsInBlock_ = packetCount;
				const size_t bin = extraPackets / 5;
				if(bin >= totalExtraHist_.size())
					totalExtraHist_.resize(bin + 1, 0);
				totalExtraHist_[bin] += extraPackets;

				// interval
				++ivBlocks_;
				ivPackets_ += packetCount;
				ivExtraPackets_ += extraPackets;
				if(bin >= ivExtraHist_.size())
					ivExtraHist_.resize(bin + 1, 0);
				ivExtraHist_[bin] += extraPackets;

				if(extraPackets >= minExtraPackets_)
					pass = true;
			}
		}
	}

	if(eventHasBlocks)
	{
		++ivEventsWithBlocks_;
		ivMaxSum_ += eventMaxPkts;
	}

	if(pass)
		++eventsPassed_;

	if(debugEvery_ != 0 && (eventsSeen_ % debugEvery_) == 0)
	{
		const double passRate =
		    eventsSeen_ > 0 ? 100.0 * eventsPassed_ / eventsSeen_ : 0.0;

		TLOG(TLVL_INFO) << "CrvEventFilter stats: seen=" << eventsSeen_
		                << ", passed=" << eventsPassed_ << " (" << std::fixed
		                << std::setprecision(1) << passRate << "%)"
		                << " | last " << debugEvery_ << " evts:"
		                << " sumPkts=" << ivPackets_
		                << " sumExtraPkts=" << ivExtraPackets_
		                << " sumMaxPkts=" << ivMaxSum_
		                << (printHistogram_ ? buildHistogram_(ivExtraHist_) : "");

		resetInterval_();
	}

	return pass;
}

std::string CrvEventFilter::buildHistogram_(const std::vector<size_t>& hist) const
{
	if(hist.empty())
		return "";
	size_t maxBin = 0;
	for(size_t v : hist)
		maxBin = std::max(maxBin, v);
	if(maxBin == 0)
		return "";

	const int          barWidth = 25;
	std::ostringstream oss;
	oss << "\n  extraPkts histogram (sum per bin):";
	for(size_t i = 0; i < hist.size(); ++i)
	{
		if(hist[i] == 0)
			continue;
		const size_t lo   = i * 5;
		const size_t hi   = lo + 5;
		const int    bars = static_cast<int>(hist[i] * barWidth / maxBin);
		oss << "\n    " << std::setw(3) << lo << "-" << std::setw(3) << hi << ": ";
		for(int b = 0; b < bars; ++b)
			oss << "\xe2\x96\x88";  // █
		oss << " (" << hist[i] << ")";
	}
	return oss.str();
}

void CrvEventFilter::resetInterval_()
{
	ivBlocks_           = 0;
	ivPackets_          = 0;
	ivExtraPackets_     = 0;
	ivMaxSum_           = 0;
	ivEventsWithBlocks_ = 0;
}

artdaq::Fragments CrvEventFilter::getFragments(art::Event& event)
{
	artdaq::Fragments    fragments;
	artdaq::FragmentPtrs containerFragments;

	auto fragmentHandles = event.getMany<std::vector<artdaq::Fragment>>();
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

				for(size_t ii = 0; ii < contf.block_count(); ++ii)
				{
					containerFragments.push_back(contf[ii]);
					fragments.push_back(*containerFragments.back());
				}
			}
		}
		else if(handle->front().type() == mu2e::FragmentType::DTCEVT)
		{
			for(const auto& f : *handle)
			{
				fragments.emplace_back(f);
			}
		}
	}

	return fragments;
}

DEFINE_ART_MODULE(ots::CrvEventFilter)
}  // namespace ots
