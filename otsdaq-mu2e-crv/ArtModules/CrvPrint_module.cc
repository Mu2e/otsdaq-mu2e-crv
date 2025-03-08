// Sam Grant 2025
// Decode fragments and print everything 
// Independent of Offline

// C++ includes
#include <thread>
#include <iomanip>
#include <string>
#include <map>

// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"

// artdaq includes
#include "artdaq-core-mu2e/Data/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"
#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

// ROOT includes
#include "TCanvas.h"
#include "TGraph.h"
#include "TH1D.h"
#include "THttpServer.h"
#include "TSystem.h"

namespace ots
{

// Utility to convert enum values to strings for better logging
std::string subsystemToString(uint8_t subsystem) {
    switch(subsystem) {
        case 0: return "Tracker";
        case 1: return "Calorimeter";
        case 2: return "CRV";
        case 3: return "Other";
        case 4: return "STM";
        case 5: return "ExtMon";
        default: return "Unknown (" + std::to_string(subsystem) + ")";
    }
}

class CrvPrint : public art::EDAnalyzer
{
  public:
	// Constructor
	explicit CrvPrint(fhicl::ParameterSet const& ps);
	// Destructor
	~CrvPrint() override;

  private:
	// Functions
	void beginJob() override;
	void analyze(art::Event const& e) override;
	void endJob() override;
    
    // Parameters
    int diagLevel_;

    std::string outputPrefix_;
	size_t fragmentCounts_{0};
    size_t subEventCounts_{0};
    size_t blockCounts_{0};
    size_t eventWindowTagCounts_{0};
    size_t packetCounts_{0};
    size_t crvBlockCounts_{0};
    size_t crvHitCounts_{0};

};

// Constructor implementation
CrvPrint::CrvPrint(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps),
      diagLevel_(ps.get<int>("diagLevel", 1))
{
    outputPrefix_ = "[CrvPrint] ";
}

// Destructor implementation
CrvPrint::~CrvPrint()
{}

void CrvPrint::beginJob()
{
    std::cout << outputPrefix_ <<"Beginning job with debug level (diagLevel_) " << diagLevel_ << std::endl;
}

void CrvPrint::analyze(art::Event const& e)
{
	// Get fragments
	std::vector<art::Handle<artdaq::Fragments> > fragmentHandles;
	fragmentHandles = e.getMany<std::vector<artdaq::Fragment> >();
	artdaq::FragmentPtrs containerFragments;
	artdaq::Fragments fragments;

    if(diagLevel_ > 0) {
        std::cout << outputPrefix_ << "=================== " << e.id() << " ===================" << std::endl;
        std::cout << outputPrefix_ << "Number of fragment handles: " << fragmentHandles.size() << std::endl;
    }

	// Iterate through fragment handles
	for(const auto& handle : fragmentHandles)
	{
		// Catch invalid or empty handles
		if(!handle.isValid() || handle->empty())
		{
            if(diagLevel_ > 0) 
			{
                std::cout << outputPrefix_ << "Found invalid or empty handle" << std::endl;
            }
			continue;
		}
        
		if(handle->front().type() == artdaq::Fragment::ContainerFragmentType)
		{
			// Iterate through containers
            if(diagLevel_ > 1) {
                std::cout << outputPrefix_ << "Container fragments (ContainerFragmentType) in handle: " << handle->size() << std::endl;
            }
			for(const auto& cont : *handle)
			{
				artdaq::ContainerFragment contf(cont);
                
                if(diagLevel_ > 1) {
                    std::cout << outputPrefix_ << "Container fragment type: " << contf.fragment_type() << std::endl;
                    std::cout << outputPrefix_ << "Container block count: " << contf.block_count() << std::endl;
                }
                
				// Break if this is single fragment rather than a container
				if(contf.fragment_type() != mu2e::FragmentType::DTCEVT)
				{
                    if(diagLevel_ > 1) {
                        std::cout << outputPrefix_ << "Container fragment type is not DTCEVT" << std::endl;
                    }
					break;
				}
				// Iterate through fragments in container and fill fragments vector
				for(size_t i = 0; i < contf.block_count(); ++i)
				{
					containerFragments.push_back(contf[i]);
					fragments.push_back(*containerFragments.back());
				}
			}
		}
		else if(handle->front().type() == mu2e::FragmentType::DTCEVT)
		{  // If the first object in the handle a single fragment
			if(diagLevel_ > 1) {
				std::cout << outputPrefix_ << "DTC Event (DTCEVT) fragments in handle: " << handle->size() << std::endl;
			}
			// Iterate through fragments and fill fragments vector
			for(auto frag : *handle)
			{
				fragments.emplace_back(frag);
			}
		}
		else 
		{
			std::cerr << outputPrefix_ << "Handle type '" << handle->front().type() << "' not recognised"<<std::endl; 

		}
	}

    fragmentCounts_ += fragments.size();

    // Summary counters for current event
    size_t currentEventSubEvents = 0;
    size_t currentEventBlocks = 0;
    size_t currentEventPackets = 0;
    size_t currentEventCrvBlocks = 0;
    size_t currentEventCrvHits = 0;
	
	// Handle the fragments
	for(const auto& frag : fragments)
	{
		try
		{
			mu2e::DTCEventFragment bb(frag);
			auto data = bb.getData();
			auto event = &data;

			auto EWT = event->GetEventWindowTag().GetEventWindowTag(true);

			if(diagLevel_ > 1)
			{
				std::cout << outputPrefix_ << "Event Window Tag: " << EWT << std::dec << std::endl;
			}
            ++eventWindowTagCounts_;
            
			// Event header
			DTCLib::DTC_EventHeader* eventHeader = event->GetHeader();
            size_t subEventsCount = event->GetSubEventCount();
            currentEventSubEvents += subEventsCount;

			if(diagLevel_ > 1)
			{
				std::cout << outputPrefix_ << "Subevents count: " << subEventsCount << std::endl;
                std::cout << outputPrefix_ << eventHeader->toJson() << std::endl;
            }

			for(unsigned int iSubEvent = 0; iSubEvent < subEventsCount; ++iSubEvent)
			{  
				// Subevent
				DTCLib::DTC_SubEvent& subevent = *(event->GetSubEvent(iSubEvent));

				// Subevent header
				const DTCLib::DTC_SubEventHeader* subeventHeader = subevent.GetHeader();
                size_t blockCount = subevent.GetDataBlockCount();

				if(diagLevel_ > 1)
				{
					std::cout << outputPrefix_ << "---> Subevent [" << iSubEvent << "]:" << std::endl;
					std::cout << outputPrefix_ << "Number of Data Blocks: " << blockCount << std::endl;
                    std::cout << outputPrefix_ << subeventHeader->toJson() << std::endl;
                }

                currentEventBlocks += blockCount;

				for(size_t iBlock = 0; iBlock < blockCount; ++iBlock)
				{
					auto block = subevent.GetDataBlock(iBlock);
					auto blockheader = block->GetHeader();
                    auto subsystem = blockheader->GetSubsystem();
                    size_t packetCount = blockheader->GetPacketCount();
                    currentEventPackets += packetCount;
                    
					if(diagLevel_ > 1)
					{
						std::cout << outputPrefix_ << "---> Block [" << iBlock << "]:" << std::endl;
						std::cout << outputPrefix_ << "Packet Count: " << blockheader->GetPacketCount() << std::endl;
                        std::cout << outputPrefix_ << blockheader->toJSON() << std::endl;
                        std::cout << outputPrefix_ << "Block details:" << std::endl
                                    << outputPrefix_ << "  Subsystem: " << subsystemToString(subsystem) << std::endl
                                    << outputPrefix_ << "  Valid: " << (blockheader->isValid() ? "Yes" : "No") << std::endl
                                    << outputPrefix_ << "  Version: 0x" << std::hex << (int)blockheader->GetVersion() << std::dec << std::endl
                                    << outputPrefix_ << "  DTC ID: " << (int)blockheader->GetID() << std::endl
                                    << outputPrefix_ << "  Byte Count: " << block->byteSize << std::endl
                                    << outputPrefix_ << "  Event Window Tag: " << blockheader->GetEventWindowTag().GetEventWindowTag(true) << std::endl; // " (0x" 
                        
                        if (diagLevel_ > 2)
                        {
                            for(int iPacket = 0; iPacket < blockheader->GetPacketCount(); ++iPacket)
                            {
                                std::cout << outputPrefix_ << "---> Packet [" << iPacket << "]: " << 
                                    DTCLib::DTC_DataPacket(((uint8_t*)block->blockPointer) + ((iPacket + 1) * 16)).toJSON() << std::endl;
                            }
                        }
					}

					// Make sure we only process CRV data
					if(blockheader->GetSubsystem() == DTCLib::DTC_Subsystem_CRV)
					{
                        ++currentEventCrvBlocks; 

                        if(diagLevel_ > 0) 
                        {
                            std::cout << outputPrefix_ << "*** CRV BLOCK FOUND ***" << std::endl;
                            
                            // Additional info for debugging
                            std::cout << outputPrefix_ << "CRV Block Status: " << (int)blockheader->GetStatus() << std::endl;
                        }
                        
						if(blockheader->isValid())
						{
							// Inherited this from someone, maybe Pasha
							if(blockheader->GetVersion() == 0x0)
							{
								// Create the CRV data decoder object for this subevent
								mu2e::CRVDataDecoder crvData(subevent); 
                                
								// Get ROC status packet
								auto rocStatus = crvData.GetCRVROCStatusPacket(iBlock);
								if(rocStatus != nullptr && diagLevel_ > 0)
								{
									std::cout << outputPrefix_ << "CRV ROC Status Packet:" << std::endl
												<< outputPrefix_ << "  Controller ID: " << (int)rocStatus->ControllerID << std::endl
												<< outputPrefix_ << "  Active FEB Flags: " << rocStatus->GetActiveFEBFlags().to_string() << std::endl
												<< outputPrefix_ << "  Trigger Count: " << rocStatus->TriggerCount << std::endl
												<< outputPrefix_ << "  Event Window Tag: " << rocStatus->GetEventWindowTag() << std::endl;
								}
                                
                                // Process CRV hits using the data decoder
                                std::vector<mu2e::CRVDataDecoder::CRVHit> hits = crvData.GetCRVHits(iBlock);
            
                                currentEventCrvHits += hits.size();
                                
                                if(diagLevel_ > 0)
                                {
                                    std::cout << outputPrefix_ << "Found " << hits.size() << " CRV hits" << std::endl;
                                }
                                    
                                // Output detailed hit information when requested
                                if(diagLevel_ > 1 && !hits.empty())
                                {
                                    std::cout << "First Hit Details:" << std::endl;
                                    auto& firstHit = hits.front();
                                    std::cout << outputPrefix_ << "  FEB Channel: " << firstHit.first.febChannel << std::endl
                                                << outputPrefix_ << "  Port Number: " << firstHit.first.portNumber << std::endl
                                                << outputPrefix_ << "  Controller Number: " << firstHit.first.controllerNumber << std::endl
                                                << outputPrefix_ << "  Hit Time: " << firstHit.first.HitTime << std::endl
                                                << outputPrefix_ << "  Num Samples: " << firstHit.first.NumSamples << std::endl;
                                    
                                    if(!firstHit.second.empty())
                                    {
                                        std::cout << outputPrefix_ << "  First Sample ADC: " << firstHit.second.front().ADC << std::endl;
                                    }
                                }
                                
                                // Deeper waveform details for even higher diagnostic levels
                                if(diagLevel_ > 2 && !hits.empty())
                                {
                                    auto& firstHit = hits.front();
                                    std::cout << outputPrefix_ << "Waveform for first hit:" << std::endl;
                                    for(size_t i = 0; i < firstHit.second.size(); i++)
                                    {
                                        if(i % 8 == 0) std::cout << "  ";
                                        std::cout << std::setw(5) << firstHit.second[i].ADC;
                                        if(i % 8 == 7 || i == firstHit.second.size() - 1) std::cout << std::endl;
                                    }
                                }                    
                            }
                            else
                            {
                                if(diagLevel_ > 0)
                                {
                                    std::cout << outputPrefix_ << "CRV block with unsupported version: 0x" 
                                              << std::hex << (int)blockheader->GetVersion() << std::dec << std::endl;
                                }
                            }
						}
                        else if(diagLevel_ > 0)
                        {
                            std::cout << outputPrefix_ << "Invalid CRV block header!" << std::endl;
                        }
					} 
				} 
			}
		}
		catch(const std::exception& e)
		{
			if(diagLevel_ > 0)
			{
				std::cerr << outputPrefix_ << "Error processing fragment: " << e.what() << std::endl;
			}
			continue;
		}
		
	}

    // Print summary for this event
    if(diagLevel_ > 0)
    {
        std::cout << outputPrefix_ << "Event Summary: " 
                << currentEventSubEvents << " SubEvents; "
                << currentEventBlocks << " Blocks; "
                << currentEventPackets << " Packets; "
                << currentEventCrvBlocks << " CRV Blocks; "
                << currentEventCrvHits << " CRV Hits."
                << std::endl;
    }

    subEventCounts_ += currentEventSubEvents;
    blockCounts_ += currentEventBlocks;
    packetCounts_ += currentEventPackets;
    crvBlockCounts_ += currentEventCrvBlocks;
    crvHitCounts_ += currentEventCrvHits;

}

void CrvPrint::endJob()
{
    // Print job-level statistics
    std::cout << outputPrefix_ << "================= Job Summary =================" << std::endl;
    std::cout << outputPrefix_ << "Total Fragments: " << fragmentCounts_ << std::endl;
    std::cout << outputPrefix_ << "Total SubEvents: " << subEventCounts_ << std::endl;
    std::cout << outputPrefix_ << "Total Blocks: "    << blockCounts_    << std::endl;
    std::cout << outputPrefix_ << "Total CRV Blocks: " << crvBlockCounts_ << std::endl;
    std::cout << outputPrefix_ << "Total CRV Hits: " << crvHitCounts_ << std::endl;
    std::cout << outputPrefix_ << "===============================================" << std::endl;
}

DEFINE_ART_MODULE(CrvPrint)
}  // namespace ots