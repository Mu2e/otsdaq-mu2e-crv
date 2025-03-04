#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art_root_io/TFileService.h"

// artdaq includes
#include "artdaq-core-mu2e/Data/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

// ROOT includes
#include "TTree.h"

#include <vector>
#include <string>
#include <iostream>

namespace ots
{

class CrvDumper : public art::EDAnalyzer
{
public:
    // Constructor
    explicit CrvDumper(fhicl::ParameterSet const& ps);
    // Destructor
    ~CrvDumper() override;

private:
    // Functions
    void beginJob() override;
    void analyze(art::Event const& e) override;
    void endJob() override;

    // fcl parameters
    art::InputTag decoderTag_;   // specify the module producing CRV decoder obects
    int diagLevel_;              // diagnostic level

    // Member variables
    float crvClockTick_;
    float rocClockTick_;

    // Tree pointer
    TTree *tree_;

    // Event-level leaves
    uint32_t runNum_;      // art::RunNumber_t
    uint32_t subRunNum_;   // art::SubRunNumber_t
    uint64_t eventNum_;    // art::EventNumber_t
    
    // SubEvent information (one entry per subevent in the event)
    std::vector<float> link0Latency_;       // Latency for each subevent
    std::vector<uint64_t> EWT_;             // Event Window Tag for each subevent
    std::vector<uint32_t> nHits_;           // Number of hits per block in each subevent

    // Hit information (flattened, with indices to track relationships)
    std::vector<int> hitSubEventIdx_;       // The subevent each hit belongs to
    std::vector<int> hitBlockIdx_;          // The block each hit belongs to
    std::vector<int> febChannel_;           // FEB channel for each hit
    std::vector<float> hitTime_;            // Time for each hit (ns)
    std::vector<int> nSamples_;             // Number of samples in each hit

    // Waveform information (partially flattened)
    std::vector<int> waveformHitIdx_;       // Which hit this waveform belongs to
    std::vector<std::vector<int>> ADC_;     // ADC values for each hit's waveform
};

// Constructor implementation
CrvDumper::CrvDumper(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , decoderTag_(ps.get<art::InputTag>("decoderTag", "genFrags"))
    , diagLevel_(ps.get<int>("diagLevel", 0))
{
    crvClockTick_    = 12.5;  // ns
    rocClockTick_    = 50;    // ns, based on the 20 MHz clock
}

// Destructor implementation
CrvDumper::~CrvDumper()
{}

void CrvDumper::beginJob()
{
    // Initialise tree 
    art::ServiceHandle<art::TFileService> tfs;

    tree_ = tfs->make<TTree>("crvDump", "CRV dump");

    // Event-level branches
    tree_->Branch("runNum", &runNum_); 
    tree_->Branch("subRunNum", &subRunNum_); 
    tree_->Branch("eventNum", &eventNum_); 
    
    // SubEvent-level branches
    tree_->Branch("link0Latency", &link0Latency_);
    tree_->Branch("EWT", &EWT_);
    tree_->Branch("nHits", &nHits_); // per block
    
    // Hit-level branches
    tree_->Branch("hitSubEventIdx", &hitSubEventIdx_);
    tree_->Branch("hitBlockIdx", &hitBlockIdx_);
    tree_->Branch("febChannel", &febChannel_);
    tree_->Branch("hitTime", &hitTime_);
    tree_->Branch("nSamples", &nSamples_);
    
    // Waveform-level branches
    tree_->Branch("waveformHitIdx", &waveformHitIdx_);
    tree_->Branch("ADC", &ADC_);
}

void CrvDumper::analyze(art::Event const& e)
{
    // Clear vectors for new event
    link0Latency_.clear();
    EWT_.clear();
    nHits_.clear();
    
    hitSubEventIdx_.clear();
    hitBlockIdx_.clear();
    febChannel_.clear();
    hitTime_.clear();
    nSamples_.clear();
    
    waveformHitIdx_.clear();
    ADC_.clear();
    
    // Set event info
    eventNum_ = e.event();
    runNum_ = e.run();
    subRunNum_ = e.subRun();

    // Try getting CRV data
    try
    {
        // Get CRV data decoders from the event 
        auto decodersHandle = e.getValidHandle<std::vector<mu2e::CRVDataDecoder>>(decoderTag_);
        size_t nSubEvents = decodersHandle->size();

        // Process the decoder objects, each contains data from one subevent
        for(size_t iSubEvent = 0; iSubEvent < nSubEvents; ++iSubEvent)
        {
            const mu2e::CRVDataDecoder& decoder((*decodersHandle)[iSubEvent]);
            decoder.setup_event();

            // Access the SubEventHeader
            auto subEventHeader = decoder.event_.GetHeader();

            // Record subevent info
            link0Latency_.push_back(subEventHeader->link0_drp_rx_latency * rocClockTick_);
            
            // Get EWT from first block in this subevent
            // Should we actually check every block? 
            uint64_t ewt = 0;
            if (decoder.block_count() > 0) {
                auto block = decoder.dataAtBlockIndex(0);
                if (block && block->GetHeader()) {
                    ewt = block->GetHeader()->GetEventWindowTag().GetEventWindowTag(true);
                }
            }
            EWT_.push_back(ewt);
            
            // For each subevent, process the blocks
            for(size_t bl = 0; bl < decoder.block_count(); ++bl)
            {
                // Get block at this index
                auto block = decoder.dataAtBlockIndex(bl);
                if(!block) continue; // Skip empty blocks

                // Get block header
                auto blockHeader = block->GetHeader();

                if(diagLevel_ > 1) {
                    std::cout << blockHeader->toJSON() << std::endl;
                }

                if(!blockHeader->isValid()) {
                    if(diagLevel_ > 1)
                        std::cout << "Block header is invalid..." << std::endl;
                    continue;  // skip this block
                }

                // // Get CRV ROC header for this block
                // std::unique_ptr<mu2e::CRVDataDecoder::CRVROCStatusPacket> crvRocHeader = decoder.GetCRVROCStatusPacket(bl);
                // if (crvRocHeader==nullptr)
                // { 
                //     continue;
                // } else {
                //     std::cout << "**** ROC Status Header ****" << std::endl;
                //     std::cout << "ROCID (ROC Status): "<< (uint16_t)crvRocHeader->ControllerID << std::endl;
                // }

                // Try to get hits 
                try 
                { 
                    // Get CRV hits for this block
                    auto hits = decoder.GetCRVHits(bl);
                    
                    // Store hits in this subevent
                    nHits_.push_back(hits.size());
                    
                    // Process each hit in this block
                    for(auto &hit : hits)
                    {
                        // Store hit information with references to subevent and block
                        hitSubEventIdx_.push_back(iSubEvent);
                        hitBlockIdx_.push_back(bl);
                        febChannel_.push_back(hit.first.febChannel);
                        hitTime_.push_back(hit.first.HitTime * crvClockTick_);  // Convert to ns
                        nSamples_.push_back(hit.first.NumSamples);
                        
                        // Current hit index in the full event
                        int currentHitIdx = hitSubEventIdx_.size() - 1;
                        
                        // Store waveform reference and samples
                        waveformHitIdx_.push_back(currentHitIdx);
                        
                        // Create vector for this waveform's samples
                        std::vector<int> samples;
                        samples.reserve(hit.first.NumSamples); // Reserve indices
                        
                        // Process waveform samples
                        for(auto& sample : hit.second)
                        {
                            samples.push_back(sample.ADC);
                        }
                        
                        // Add this waveform to the collection
                        ADC_.push_back(samples);
                    }
                }
                catch(const std::exception& e) 
                {
                    if(diagLevel_ > 0) {
                        std::cout << "Error processing block: " << e.what() << std::endl;
                    }
                }

                
            }
        }
    }
    catch(const std::exception& e)
    {
        if(diagLevel_ > 0) {
            std::cout << "Error processing event: " << e.what() << std::endl;
        }
    }
    
    // Fill tree after processing the entire event
    tree_->Fill();
}

// End job printouts
void CrvDumper::endJob()
{
    std::cout << "\n**************************************************" << std::endl;
    std::cout << "CrvDumper job completed." << std::endl;
    std::cout << "Total events processed: " << tree_->GetEntries() << std::endl;
    std::cout << "**************************************************" << std::endl;
}

DEFINE_ART_MODULE(CrvDumper)

}  // namespace ots