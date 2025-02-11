// ROOT-based DQM and viewer for the CRV 
// Authors: Sam Grant and Simon Corrodi 
// Data: Feb 2025

// C++ includes
#include <thread>

// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"

// artdaq includes
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq-core-mu2e/Data/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

// ROOT includes
#include "TCanvas.h"
#include "TH1D.h"
#include "THttpServer.h"
#include "TSystem.h"

// Custom includes
#include "CrvDQMStyle.hh"

namespace demo { // what is the appropriate namespace?

class CrvDQM : public art::EDAnalyzer {
public:
    // Constructor
    explicit CrvDQM(fhicl::ParameterSet const& ps);
    // Destructor
    ~CrvDQM() override;

private:
    
    // Functions
    void beginJob() override;
    void analyze(art::Event const& e) override;
    void endJob() override;

    // fcl parameters
    int port_;
    int diagLevel_;
    float onlineRefreshPeriod_; 
    bool keepAlive_; 
    int keepAliveDuration_; // minutes
    std::string histColor_; // "red"/"blue"/"green"

    // Member variables
    TCanvas* canvas_;
    std::unordered_map<std::string, TH1D*> hists_;
    THttpServer* server_;
    std::chrono::time_point<std::chrono::steady_clock> lastUpdate_;
    std::size_t eventCounter_;
    std::thread keepAliveThread_;  

};

// Constructor implementation
CrvDQM::CrvDQM(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , port_(ps.get<int>("port", 8877))
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , onlineRefreshPeriod_(ps.get<float> ("onlineRefreshPeriod", 500)) // ms
    , keepAlive_(ps.get<int> ("keepAlive", true))
    , keepAliveDuration_(ps.get<int> ("keepAliveDuration", 5)) // minutes
    , histColor_(ps.get<std::string> ("histColor", "blue")) // minutes
{
    // Initialise non-fcl member variables
    eventCounter_ = 0;
}

// Destructor implementation
CrvDQM::~CrvDQM() {
    // Make sure the server thread is stopped
    if(keepAliveThread_.joinable()) {
        keepAliveThread_.join();
    }
    // Then clean up ROOT objects
    if (server_) {
        delete server_; 
        server_ = nullptr;
    }
    if (canvas_) {
        delete canvas_;
        canvas_ = nullptr;
    }
    for (auto& hist : hists_) {
        if (hist.second) delete hist.second;
        hist.second = nullptr;
    }
    hists_.clear();
}

void CrvDQM::beginJob() {
    // Create HTTP server
    server_ = new THttpServer(Form("http:%d", port_));

    // Set global plot style
    CrvDQMStyle::SetStyle();    
    
    // Create canvas
    std::string canvasName = "WebDisplay";
    canvas_ = new TCanvas(canvasName.c_str(), "CRV web display"); 
    canvas_->Divide(2,2); // divide into 4x4 grid
    
    // Create histograms
    hists_["channels"] = new TH1D("Channels", ";Channel;Counts", 64, -0.5, 63.5);
    hists_["timestamps"] = new TH1D("Timestamps", ";Timestamp;Counts", 256, 0, 255); // 0-0xff 
    hists_["nhits"] = new TH1D("Hits", ";Hits / block;Counts", 61, 0, 60);
    hists_["adc"] = new TH1D("ADC",";ADC;Counts", 201, -100, 100); 

    // Format and draw
    int canvasIdx = 1;
    for (auto& hist : hists_) {
        // Get pad 
        canvas_->cd(canvasIdx);
        ++canvasIdx;
        // Consistent formatting
        CrvDQMStyle::FormatHist(hist.second, histColor_);
        // Draw
        hist.second->Draw();
    }

    // Register with server
    server_->Register("/", canvas_);

    // Set item defaults
    server_->SetItemField("/","_monitoring", Form("%f", onlineRefreshPeriod_)); // Update period 
    server_->SetItemField("/","_sidebar", "0");
    server_->SetItemField("/","_drawitem", canvasName.c_str()); // Set DQM canvas as default item
    server_->SetItemField("/","_http_cache", "0");  // Disable HTTP caching

    // Last update variable 
    lastUpdate_ = std::chrono::steady_clock::now();

    // Print URL
    printf("Server running on http://localhost:%d/\n", port_);

    // Start an independent thread for server
    keepAliveThread_ = std::thread([this]() {
        while(keepAlive_) {
            gSystem->ProcessEvents(); // Process events
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small sleep
        }
    });

}

void CrvDQM::analyze(art::Event const& e) {
	// Get fragments 
    std::vector<art::Handle<artdaq::Fragments> > fragmentHandles;
    fragmentHandles = e.getMany<std::vector<artdaq::Fragment> >();
    artdaq::FragmentPtrs containerFragments;
    artdaq::Fragments fragments;

    // Iterate through fragment handles
	for (const auto& handle : fragmentHandles) {
        // Catch invalid or empty handles
        if (!handle.isValid() || handle->empty()) {
	        continue;
        }
        // Check if the first object is Container Fragment
        // ContainerFragment
        // ├── Fragment 1 (DTCEVT)
        // └── Fragment n (DTCEVT)
        if (handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
            // Iterate through containers
            for (const auto& cont : *handle) {
                artdaq::ContainerFragment contf(cont);
                // Break if this is single fragment rather than a container
	            if (contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
                    break;
                }
                // Iterate through fragments in container and fill fragments vector
                for (size_t i = 0; i < contf.block_count(); ++i) {
                    containerFragments.push_back(contf[i]);
                    fragments.push_back(*containerFragments.back());
                }
            }
        } else { // If the first object in the handle a single fragment
            if (handle->front().type() == mu2e::FragmentType::DTCEVT) {
                // Iterate through fragments and fill fragments vector
                for (auto frag : *handle) {
                    fragments.emplace_back(frag);
                }
            }
        }
    }
    if (diagLevel_ > 1) { 
        TLOG(TLVL_INFO) << "[CrvDQM::analyze] Found nFragments" << fragments.size();
    }

    // Handle the fragments
    for (const auto& frag : fragments) {
        try {
            mu2e::DTCEventFragment bb(frag); 
            auto data = bb.getData(); 
            auto event = &data; 
            if (diagLevel_ > 1) { 
                TLOG(TLVL_INFO) << "Event tag:\t" << "0x" << std::hex << std::setw(4) << std::setfill('0') << event->GetEventWindowTag().GetEventWindowTag(true);
            }
            DTCLib::DTC_EventHeader* eventHeader = event->GetHeader();
            
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << eventHeader->toJson() << std::endl
                << "Subevents count: " << event->GetSubEventCount() << std::endl;
            }
            
            bool plotsUpdated = false;
            for (unsigned int i = 0; i < event->GetSubEventCount(); ++i) { // In future, use GetSubsystemData to only get CRV subevents
                DTCLib::DTC_SubEvent& subevent = *(event->GetSubEvent(i));
                if (diagLevel_ > 1) {
                    TLOG(TLVL_INFO) << "Subevent [" << i << "]:" << std::endl;
                    TLOG(TLVL_INFO) << subevent.GetHeader()->toJson() << std::endl;
                    TLOG(TLVL_INFO) << "Number of Data Block: " << subevent.GetDataBlockCount() << std::endl;
                }
                
                for (size_t bl = 0; bl < subevent.GetDataBlockCount(); ++bl) {
                    ++eventCounter_;
                    auto block = subevent.GetDataBlock(bl);
                    auto blockheader = block->GetHeader();
                    if (diagLevel_ > 1) {
                        TLOG(TLVL_INFO) << blockheader->toJSON() << std::endl;
                        for (int ii = 0; ii < blockheader->GetPacketCount(); ++ii) {
                            TLOG(TLVL_INFO) << DTCLib::DTC_DataPacket(((uint8_t*)block->blockPointer) + ((ii + 1) * 16)).toJSON() << std::endl;
                        }
                    }
                    // Check if we want to decode this data block
                    // Make sure we only process CRV data
                    if(blockheader->GetSubsystem() == 0x2) { 
                        if(blockheader->isValid()) {
                            // DQM for version 0x0 data
                            if( blockheader->GetVersion() == 0x0 ) {
                                auto crvData = mu2e::CRVDataDecoder(subevent); // reference
                                //const auto crvStatus = crvData.GetCRVROCStatusPacket(bl);
                                auto hits = crvData.GetCRVHits(bl);
                                for (auto& hit : hits) {
                                    // Fill histograms
                                    hists_["channels"]->Fill(hit.first.febChannel);
                                    hists_["timestamps"]->Fill(hit.first.HitTime);
                                    for (auto& adc : hit.second) {
                                        hists_["adc"]->Fill(adc.ADC);
                                    }
                                }
                                hists_["nhits"]->Fill(hits.size());
                                plotsUpdated = true;
                            }
                        } else {
                            // Iterate invalid count?
                            // ++invalidEventCounter_;
                            continue;
                        }
                    }
                }
            }

            if(plotsUpdated) {
                auto currentTime = std::chrono::steady_clock::now();
                std::chrono::duration<double, std::milli> elapsed = currentTime - lastUpdate_;
                if(elapsed.count() >= onlineRefreshPeriod_) {
                    // Auto scale y-axis
                    for (auto& hist : hists_) { 
                        double maxContent =  hist.second->GetBinContent(hist.second->GetMaximumBin()); 
                        hist.second->GetYaxis()->SetRangeUser(0, 1.15*maxContent);
                    }
                    // Update the canvas
                    canvas_->Modified();
                    canvas_->Update();
                    gSystem->ProcessEvents(); // Update display
                    lastUpdate_ = currentTime; // Update the time
                }
            }

        }
        catch (const std::exception& e) {
            if (diagLevel_ > 0) {
                TLOG(TLVL_WARNING) << "Error processing fragment: " << e.what();
            }
            continue;
        }
    }
}

// End job printouts
void CrvDQM::endJob() {
    // What am we defining as an "event"? Need to think about it.
    printf("========================================\n");
    printf("Processed Events  : %zu\n", eventCounter_);
    // printf("Invalid Events    : %zu\n", invalidEventCounter_);
    // printf("Valid Events      : %zu\n", eventCounter_ - invalidEventCounter_);
    // printf("Error Rate        : %.2f%%\n", 
    //      (eventCounter_ > 0) ? (100.0 * invalidEventCounter_ / eventCounter_) : 0.0);
    if (keepAlive_) { 
        printf("Keeping server alive for %i minutes", keepAliveDuration_);
        printf("\n========================================\n");
        std::this_thread::sleep_for(std::chrono::minutes(keepAliveDuration_));
    } else { 
        printf("Killing server");
        printf("\n========================================\n");
    }
    // Server server thread is then stopped by the destructor
}

DEFINE_ART_MODULE(CrvDQM)
}
