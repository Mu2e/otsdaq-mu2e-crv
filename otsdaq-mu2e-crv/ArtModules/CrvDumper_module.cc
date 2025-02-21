#include "TRACE/tracemf.h"
#define TRACE_NAME "CrvDumper"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "canvas/Utilities/InputTag.h"
#include "cetlib_except/exception.h"

#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

//#include "artdaq-core-mu2e/Data/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

//#include "artdaq-core-demo/Overlays/FragmentType.hh"
//#include "artdaq-core-demo/Overlays/ToyFragment.hh"

#include <TApplication.h>
#include <TSystem.h>
#include <TAxis.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TRootCanvas.h>
#include <TStyle.h>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "tracemf.h"

namespace demo {
/**
 * \brief An example art analysis module which plots events both as histograms and event snapshots (plot of ADC value vs ADC number)
 */
class CrvDumper : public art::EDAnalyzer {
public:
	explicit CrvDumper(fhicl::ParameterSet const& p);

	~CrvDumper() override;

	void analyze(art::Event const& e) override;

	void beginJob()                   override;
	void beginRun(art::Run const&  e) override;
	void endRun  (art::Run const&  e) override;

private:
	CrvDumper(CrvDumper const&) = delete;
	CrvDumper(CrvDumper&&) = delete;
	CrvDumper& operator=(CrvDumper const&) = delete;
	CrvDumper& operator=(CrvDumper&&) = delete;

	int diagLevel_;
};

  //-----------------------------------------------------------------------------
CrvDumper::CrvDumper(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer  (ps)
    , diagLevel_(10)
 {}
  //-----------------------------------------------------------------------------
  //
void CrvDumper::beginJob() {
  std::cout << "CrvDumper beginJob " << std::endl;

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CrvDumper::~CrvDumper() {
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CrvDumper::analyze(art::Event const& event) {
    static std::size_t evt_cntr = -1;
    evt_cntr++;
    
    if (diagLevel_ > 1) {
        TLOG(TLVL_INFO) << "[CrvDumper::analyze] Starting analysis of event counter: " << evt_cntr;
    }

    std::vector<art::Handle<artdaq::Fragments> > fragmentHandles;
    fragmentHandles = event.getMany<std::vector<artdaq::Fragment> >();

    if (diagLevel_ > 1) {
        TLOG(TLVL_INFO) << "[CrvDumper::analyze] Retrieved " << fragmentHandles.size() << " fragment handles";
    }

    artdaq::FragmentPtrs containerFragments;
    artdaq::Fragments fragments;
    for (const auto& handle : fragmentHandles) {
        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << "[CrvDumper::analyze] Processing new fragment handle";
            TLOG(TLVL_INFO) << "Handle validity: " << (handle.isValid() ? "valid" : "invalid");
            if (handle.isValid()) {
                TLOG(TLVL_INFO) << "Handle size: " << handle->size();
            }
        }

        if (!handle.isValid() || handle->empty()) {
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << "Skipping invalid or empty handle";
            }
            continue;
        }

        if (handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << "Processing ContainerFragmentType";
            }
            
            for (const auto& cont : *handle) {
                artdaq::ContainerFragment contf(cont);
                if (diagLevel_ > 1) {
                    TLOG(TLVL_INFO) << "Container fragment type: " << contf.fragment_type();
                    TLOG(TLVL_INFO) << "Block count: " << contf.block_count();
                }

                if (contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
                    if (diagLevel_ > 1) {
                        TLOG(TLVL_INFO) << "Skipping non-DTCEVT fragment";
                    }
                    break;
                }

                for (size_t ii = 0; ii < contf.block_count(); ++ii) {
                    if (diagLevel_ > 1) {
                        TLOG(TLVL_INFO) << "Processing block " << ii << " of " << contf.block_count();
                    }
                    containerFragments.push_back(contf[ii]);
                    fragments.push_back(*containerFragments.back());
                }
            }
        } else {
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << "Processing non-container fragment type: " << handle->front().type();
            }

            if (handle->front().type() == mu2e::FragmentType::DTCEVT) {
                for (auto frag : *handle) {
                    if (diagLevel_ > 1) {
                        TLOG(TLVL_INFO) << "Adding DTCEVT fragment to collection";
                    }
                    fragments.emplace_back(frag);
                }
            }
        }
    }

    if (diagLevel_ > 1) {
        TLOG(TLVL_INFO) << "[CrvDumper::analyze] Found nFragments: " << fragments.size();
        TLOG(TLVL_INFO) << "Container fragments size: " << containerFragments.size();
    }

    // handle the fragments
    for (const auto& frag : fragments) {
        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << "Processing fragment with size: " << frag.size();
        }

        mu2e::DTCEventFragment bb(frag);
        auto data = bb.getData();
        auto event = &data;

        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << "Event tag: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                           << event->GetEventWindowTag().GetEventWindowTag(true);
            TLOG(TLVL_INFO) << "Fragment type: " << frag.type();
            TLOG(TLVL_INFO) << "Fragment sequence ID: " << frag.sequenceID();
            TLOG(TLVL_INFO) << "Fragment timestamp: " << frag.timestamp();
        }

        DTCLib::DTC_EventHeader* eventHeader = event->GetHeader();
        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << "Event Header JSON: " << eventHeader->toJson();
            TLOG(TLVL_INFO) << "Subevents count: " << event->GetSubEventCount();
        }
    /* 

	static std::size_t evt_cntr = -1;
	evt_cntr++;
    
	// New code for CRV VST Demo
    std::vector<art::Handle<artdaq::Fragments> > fragmentHandles;
    fragmentHandles = event.getMany<std::vector<artdaq::Fragment> >();

    artdaq::FragmentPtrs containerFragments;
    artdaq::Fragments fragments;
	for (const auto& handle : fragmentHandles) {
        if (!handle.isValid() || handle->empty()) {
	        continue;
        }
        if (handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
            for (const auto& cont : *handle) {
                artdaq::ContainerFragment contf(cont);
	            if (contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
                    break;
                }
                for (size_t ii = 0; ii < contf.block_count(); ++ii) {
                    containerFragments.push_back(contf[ii]);
                    fragments.push_back(*containerFragments.back());
                }
            }
        } else {
            if (handle->front().type() == mu2e::FragmentType::DTCEVT) {
                for (auto frag : *handle) {
                    fragments.emplace_back(frag);
                }
            }
        }
    }
    if (diagLevel_ > 1) {
        TLOG(TLVL_INFO) << "[CrvVstDemoViewer::analyze] Found nFragments  " << fragments.size();
    }

    // handle the fragments
    for (const auto& frag : fragments) {
        mu2e::DTCEventFragment bb(frag);
        auto data = bb.getData();
        auto event = &data;
        if (diagLevel_ > 1)
            TLOG(TLVL_INFO) << "Event tag:\t" << "0x" << std::hex << std::setw(4) << std::setfill('0') << event->GetEventWindowTag().GetEventWindowTag(true);
        DTCLib::DTC_EventHeader* eventHeader = event->GetHeader();
        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << eventHeader->toJson() << std::endl
            << "Subevents count: " << event->GetSubEventCount() << std::endl;
        }
        
        bool hist_updated = false;
        for (unsigned int i = 0; i < event->GetSubEventCount(); ++i) { // In future, use GetSubsystemData to only get CRV subevents
            DTCLib::DTC_SubEvent& subevent = *(event->GetSubEvent(i));
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << "Subevent [" << i << "]:" << std::endl;
                TLOG(TLVL_INFO) << subevent.GetHeader()->toJson() << std::endl;
                TLOG(TLVL_INFO) << "Number of Data Block: " << subevent.GetDataBlockCount() << std::endl;
            }
                
            for (size_t bl = 0; bl < subevent.GetDataBlockCount(); ++bl) {
                auto block = subevent.GetDataBlock(bl);
                auto blockheader = block->GetHeader();
                if (diagLevel_ > 1) {
                    TLOG(TLVL_INFO) << blockheader->toJSON() << std::endl;
                    for (int ii = 0; ii < blockheader->GetPacketCount(); ++ii) {
                        TLOG(TLVL_INFO) << DTCLib::DTC_DataPacket(((uint8_t*)block->blockPointer) + ((ii + 1) * 16)).toJSON() << std::endl;
                    }
                }
                // check if we want to decode this data block
                // make sure we only process CRV data
                if(blockheader->GetSubsystem() == 0x2) {
                    if(blockheader->isValid()) {
                        // DQM for version 0x0 data
                        if( blockheader->GetVersion() == 0x0 ) {
                            auto crvData = mu2e::CRVDataDecoder(subevent); // reference
                            //const auto crvStatus = crvData.GetCRVROCStatusPacket(bl);
                            auto hits = crvData.GetCRVHits(bl);
                            for (auto& hit : hits) {
                                //if (newCanvas_) {
                                //    bookCanvas_();
                                //}
                                histograms_["channels"]->Fill(hit.first.febChannel); // one hist per FPGA?
                                histograms_["timestamps"]->Fill(hit.first.HitTime);
                                hist_updated = true;
                                //std::copy(toyPtr->dataBeginADCs(), toyPtr->dataBeginADCs() + total_adc_values, graphs_[fid]->GetY()); 
                                for(auto& adc : hit.second) {
                                    histograms_["adc"]->Fill(adc.ADC);
                                }
                            }
                            histograms_["nhits"]->Fill(hits.size());
                        }
                    } else {
                        // TODO increase a non-valid counter?
                    }
                }
            }
        }
        if(hist_updated) {
            auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> elapsed = currentTime - lastUpdate_;
            if(elapsed.count() >= onlineRefreshPeriod_) {
                for(size_t i = 1; i <= histograms_.size(); i++) {
                    _hCanvas->cd(i);
                    _hCanvas->Pad()->Modified();
                }
                _hCanvas->cd(0);
                _hCanvas->Update();
                gSystem->ProcessEvents(); 
                lastUpdate_ = currentTime;
            }
        }*/
    }
}

//-----------------------------------------------------------------------------
void CrvDumper::beginRun(art::Run const& e) {
}

//-----------------------------------------------------------------------------
void CrvDumper::endRun(art::Run const& e) {
}

DEFINE_ART_MODULE(CrvDumper)  // NOLINT(performance-unnecessary-value-param)
}  // namespace demo
