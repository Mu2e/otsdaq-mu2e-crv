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

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <bitset>

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

    // Utility function to print separator lines
    void printSeparator(char c, int count);
    std::string decToHex(uint64_t value, int width = 4);
    void printBits(uint32_t value, int width);
    void dumpRawData(const void* data, size_t size, size_t bytesPerLine = 16);
    
    // fcl parameters
    art::InputTag decoderTag_;   // specify the module producing CRV decoder objects
    int diagLevel_;              // diagnostic level
    int maxEventsToProcess_;     // maximum number of events to process
    bool dumpRawBytes_;          // whether to dump raw bytes
    
    // Member variables
    float crvClockTick_;
    float rocClockTick_;
    
    // Counters
    int processedEvents_;
    int totalSubEvents_;
    int totalBlocks_;
    int totalHits_;
};

// Constructor implementation
CrvDumper::CrvDumper(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , decoderTag_(ps.get<art::InputTag>("decoderTag", "genFrags"))
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , maxEventsToProcess_(ps.get<int>("maxEventsToProcess", -1))
    , dumpRawBytes_(ps.get<bool>("dumpRawBytes", false))
{
    crvClockTick_ = 12.5;  // ns
    rocClockTick_ = 50;    // ns, based on the 20 MHz clock
    processedEvents_ = 0; 
    totalSubEvents_ = 0;
    totalBlocks_ = 0;
	totalHits_ = 0;
}

// Destructor implementation
CrvDumper::~CrvDumper()
{}

// Utility function to print separator lines
void CrvDumper::printSeparator(char c, int count) {
    std::cout << std::string(count, c) << std::endl;
}

// Utility function to convert decimal to hex string
std::string CrvDumper::decToHex(uint64_t value, int width) {
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return ss.str();
}

// Utility function to print bits
void CrvDumper::printBits(uint32_t value, int width) {
    for (int i = width - 1; i >= 0; i--) {
        std::cout << ((value >> i) & 1);
        if (i % 8 == 0 && i > 0) std::cout << " ";
    }
}

// Utility function to dump raw binary data
void CrvDumper::dumpRawData(const void* data, size_t size, size_t bytesPerLine) {
    const unsigned char* byteData = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; i += bytesPerLine) {
        // Print address
        std::cout << std::setfill('0') << std::setw(8) << std::hex << i << ": ";
        
        // Print hex bytes
        for (size_t j = 0; j < bytesPerLine; j++) {
            if (i + j < size) {
                std::cout << std::setfill('0') << std::setw(2) << std::hex 
                          << static_cast<int>(byteData[i + j]) << " ";
            } else {
                std::cout << "   ";
            }
            
            if (j % 8 == 7) std::cout << " ";
        }
        
        // Print ASCII representation
        std::cout << " | ";
        for (size_t j = 0; j < bytesPerLine; j++) {
            if (i + j < size) {
                char c = byteData[i + j];
                if (c >= 32 && c <= 126) { // Printable ASCII
                    std::cout << c;
                } else {
                    std::cout << ".";
                }
            } else {
                std::cout << " ";
            }
        }
        std::cout << " |" << std::endl;
    }
    std::cout << std::dec; // Reset to decimal output
}

void CrvDumper::beginJob()
{
    printSeparator('=', 80);
    std::cout << "CrvDumper Debugging Output" << std::endl;
    std::cout << "Diagnostic Level: " << diagLevel_ << std::endl;
    std::cout << "Max Events to Process: " << (maxEventsToProcess_ < 0 ? "ALL" : std::to_string(maxEventsToProcess_)) << std::endl;
    std::cout << "Dump Raw Bytes: " << (dumpRawBytes_ ? "YES" : "NO") << std::endl;
    printSeparator('=', 80);
}

void CrvDumper::analyze(art::Event const& e)
{
    // Check if we've reached our maximum events
    if (maxEventsToProcess_ >= 0 && processedEvents_ >= maxEventsToProcess_) {
        return;
    }
    
    processedEvents_++;
    
    printSeparator('=', 80);
    std::cout << "Event: " << e.event() << ", Run: " << e.run() << ", SubRun: " << e.subRun() << std::endl;
    printSeparator('=', 80);

    try {
        // Get CRV data decoders from the event 
        auto decodersHandle = e.getValidHandle<std::vector<mu2e::CRVDataDecoder>>(decoderTag_);
        size_t nSubEvents = decodersHandle->size();
        totalSubEvents_ += nSubEvents;
        
        std::cout << "Number of SubEvents: " << nSubEvents << std::endl;

        // Process the decoder objects, each contains data from one subevent
        for(size_t iSubEvent = 0; iSubEvent < nSubEvents; ++iSubEvent) {
            printSeparator('-', 80);
            std::cout << "SubEvent " << iSubEvent << std::endl;
            printSeparator('-', 80);
            
            const mu2e::CRVDataDecoder& decoder((*decodersHandle)[iSubEvent]);
            
            // Print block count
            size_t blockCount = decoder.block_count();
            totalBlocks_ += blockCount;
            
            if(blockCount != 6) { 
                std::cout << "Fewer than 6 block in this subevent, skipping..." << std::endl;
                continue;
            } 
            else { 
                std::cout << blockCount << " blocks in this subevent" << std::endl;
            }

        
            // Check if we got a valid ROC status packet
			// auto crvRocStatus = decoder.GetCRVROCStatusPacket(bl);
    		// std::cout << "ROC Status Packet: " << (crvRocStatus ? "Present" : "Absent") << std::endl;
            // 
            // Process each block in this subevent
            for(size_t bl = 0; bl < blockCount; ++bl) {
                std::cout << "\n[BLOCK " << bl << "]" << std::endl;
                printSeparator('-', 50);
                
                // Get block at this index
                auto block = decoder.dataAtBlockIndex(bl);
                if(!block) {
                    std::cout << "  Empty block, skipping..." << std::endl;
                    continue;
                }

                // Get block header
                auto blockHeader = block->GetHeader();

                if(blockHeader->GetSubsystem() != 0x2) { 
                    std::cout << "Block Header Subsystem ID != 2: skipping..." << std::endl;
                    continue;
                } 
                else { 
                    std::cout << "Block Header Subsystem ID: " << blockHeader->GetSubsystem() << std::endl;
                }
                
                std::cout << "Block Header Valid: " << (blockHeader->isValid() ? "YES" : "NO") << std::endl;
                if(!blockHeader->isValid()) {
                    continue;
                }
                
				// Data packet header (block header) info
				std::cout << "Block Header:" << std::endl;
				std::cout << "  DTC ID: " << static_cast<int>(blockHeader->GetID()) << std::endl;
				std::cout << "  Subsystem: " << static_cast<int>(blockHeader->GetSubsystem()) << std::endl;
				std::cout << "  PacketCount: " << blockHeader->GetPacketCount() << std::endl;
				std::cout << "  Version: " << static_cast<int>(blockHeader->GetVersion()) << std::endl;
				std::cout << "  Status: " << static_cast<int>(blockHeader->GetStatus()) << std::endl;
				std::cout << "  EVB Mode: " << static_cast<int>(blockHeader->GetEVBMode()) << std::endl;
				std::cout << "  ByteCount: " << blockHeader->GetByteCount() << " bytes" << std::endl;
						
				// Event Window Tag
				auto ewt = blockHeader->GetEventWindowTag();
				std::cout << "  Event Window Tag: 0x" << ewt.GetEventWindowTag(true) << std::endl;
                
                // Get block data size
                std::cout << "  Block Size: " << block->byteSize << " bytes" << std::endl;
                
                // Get CRV ROC Status packet for this block
                auto crvRocStatus = decoder.GetCRVROCStatusPacket(bl);

				// Check if it's valid
				std::cout << "ROC Status Packet: " << (crvRocStatus ? "Present" : "Absent") << std::endl;
				
                if(crvRocStatus) {
                    std::cout << "\nROC Status Header:" << std::endl;
                    std::cout << "  Packet Type: 0x" << static_cast<int>(crvRocStatus->PacketType) << std::dec << std::endl;
                    std::cout << "  Controller ID: " << static_cast<int>(crvRocStatus->ControllerID) << std::endl;
                    std::cout << "  Controller Event Word Count: " << crvRocStatus->ControllerEventWordCount << std::endl;
                    std::cout << "  Event Window Tag: " << crvRocStatus->GetEventWindowTag() << std::endl;
                    
                    // Active FEBs bitfield
                    std::bitset<24> activeFEBs = crvRocStatus->GetActiveFEBFlags();
                    std::cout << "  Active FEBs: " << decToHex(activeFEBs.to_ulong(), 6) << std::endl;
                    std::cout << "    Bits: " << activeFEBs << std::endl;
                    
                    if (diagLevel_ > 1) {
                        std::cout << "    Active FEB Numbers: ";
                        for (int i = 0; i < 24; i++) {
                            if (activeFEBs[i]) {
                                std::cout << i+1 << " ";
                            }
                        }
                        std::cout << std::endl;
                    }
                    
                    // Trigger Count
                    std::cout << "  Trigger Count: " << crvRocStatus->TriggerCount << std::endl;
                    
                    // MicroBunch Status
                    std::cout << "  MicroBunch Status: " << decToHex(crvRocStatus->MicroBunchStatus) << std::endl;
                    
                    // EventWindowTag1 and EventWindowTag0
                    std::cout << "  EventWindowTag1: " << decToHex(crvRocStatus->EventWindowTag1) << std::endl;
                    std::cout << "  EventWindowTag0: " << decToHex(crvRocStatus->EventWindowTag0) << std::endl;
                    
                    if (dumpRawBytes_ && diagLevel_ > 1) {
                        std::cout << "\n  Raw ROC Status Packet Bytes:" << std::endl;
                        dumpRawData(crvRocStatus.get(), sizeof(mu2e::CRVDataDecoder::CRVROCStatusPacket));
                    }
                }
                else {
                    std::cout << "\n  No ROC Status packet found for this block" << std::endl;
                }

				try {
                	std::vector<mu2e::CRVDataDecoder::CRVHit> hits = decoder.GetCRVHits(bl);
                
                    totalHits_ += hits.size();

                    std::cout << "\nHits: " << hits.size() << std::endl;
                    
                    if (diagLevel_ > 1 && !hits.empty()) {
                        printSeparator('-', 40);
                        
                        for (size_t h = 0; h < hits.size(); ++h) {
                            if (h < 20 || diagLevel_ > 2) { // Limit the number of hits displayed unless diagLevel > 2
                                const auto& hit = hits[h];
                                
                                std::cout << "Hit " << h << ":" << std::endl;
                                std::cout << "  FEB Channel: " << hit.first.febChannel << std::endl;
                                std::cout << "  Port Number: " << hit.first.portNumber << std::endl;
                                std::cout << "  Controller Number: " << hit.first.controllerNumber << std::endl;
                                std::cout << "  Hit Time: " << hit.first.HitTime << " ticks (" 
                                        << hit.first.HitTime * crvClockTick_ << " ns)" << std::endl;
                                std::cout << "  Samples: " << hit.first.NumSamples << std::endl;
                                
                                // Calculate SiPM and fiber
                                int sipm = hit.first.febChannel % 4;
                                int fiber = hit.first.febChannel / 4;
                                std::cout << "  Decoded: Fiber " << fiber << ", SiPM " << sipm << std::endl;
                                
                                // Print waveform samples if high enough diagnostic level
                                if (diagLevel_ > 2) {
                                    const auto& waveform = hit.second;
                                    
                                    if (!waveform.empty()) {
                                        std::cout << "  Waveform samples:" << std::endl;
                                        
                                        for (size_t s = 0; s < waveform.size(); ++s) {
                                            if (s % 8 == 0) {
                                                std::cout << "    ";
                                            }
                                            std::cout << std::setw(4) << waveform[s].ADC;
                                            
                                            if (s % 8 == 7 || s == waveform.size() - 1) {
                                                std::cout << std::endl;
                                            } else {
                                                std::cout << ", ";
                                            }
                                        }
                                        
                                        // Calculate statistics
                                        int minADC = 4095, maxADC = 0, sumADC = 0;
                                        for (const auto& sample : waveform) {
											minADC = std::min(minADC, static_cast<int>(sample.ADC));
											maxADC = std::max(maxADC, static_cast<int>(sample.ADC));
                                            sumADC += sample.ADC;
                                        }
                                        double avgADC = static_cast<double>(sumADC) / waveform.size();
                                        
                                        std::cout << "  Waveform stats: Min=" << minADC 
                                                << ", Max=" << maxADC 
                                                << ", Avg=" << std::fixed << std::setprecision(1) << avgADC 
                                                << std::endl;
                                    }
                                }
                                std::cout << std::endl;
                            }
                        }
                        
                        if (hits.size() > 20 && diagLevel_ <= 2) {
                            std::cout << "... and " << (hits.size() - 20) << " more hits (increase diagLevel to see all)" << std::endl;
                        }
                    }
                } 
				catch(const std::exception& e) 
				{
                    std::cout << "\nError retrieving hits for this block" << std::endl;
                }
                
                // Dump raw block data if requested at highest diagnostic level
                if (dumpRawBytes_ && diagLevel_ > 2) {
                    std::cout << "\nRaw Block Data:" << std::endl;
                    dumpRawData(block->GetRawBufferPointer(), std::min(block->byteSize, size_t(256)));
                    if (block->byteSize > 256) {
                        std::cout << "... [truncated, " << (block->byteSize - 256) << " more bytes]" << std::endl;
                    }
                }
            }
        }
    }
    catch(const std::exception& e) {
        std::cerr << "ERROR processing event: " << e.what() << std::endl;
    }
}

// End job printouts
void CrvDumper::endJob()
{
    printSeparator('=', 80);
    std::cout << "CrvDumper Summary" << std::endl;
    printSeparator('-', 40);
    std::cout << "Total events processed: " << processedEvents_ << std::endl;
    std::cout << "Total SubEvents:        " << totalSubEvents_ << std::endl;
    std::cout << "Total Blocks:           " << totalBlocks_ << std::endl;
    std::cout << "Total Hits:             " << totalHits_ << std::endl;
    printSeparator('=', 80);
}

DEFINE_ART_MODULE(CrvDumper)

}  // namespace ots