// ROOT-based DQM and viewer for the CRV
// Intended for use with the OTS visualizer
// Author: Sam Grant
// Date: May 2025

// C++ includes
#include <string>
#include <map>

// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"

// art/root includes
#include "art_root_io/TFileService.h"

// ROOT includes
#include <TH1.h>
#include <TRandom3.h>

// ots includes
#include "otsdaq-mu2e/ArtModules/HistoSender.hh" // HISTOGRAM SENDING
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

namespace ots
{
class CrvOtsDQM : public art::EDAnalyzer
{
  public:
    // Constructor
    explicit CrvOtsDQM(fhicl::ParameterSet const &ps);
    // Destructor
    ~CrvOtsDQM() override;

private:
    // Standard art methods 
    void analyze(art::Event const& event) override;
    void beginJob() override;
    void endJob() override;

    /// Module methods 
    void Fill(art::Event const& event);
    void Send();
 
    // fcl parameters
    int diagLevel_;
    int port_; // port to connect to
    std::string address_;
    std::string outputTag_;
    bool sendHists_;
    
    // HISTOGRAM SENDING
    std::unique_ptr<HistoSender> histoSender_;
    float sendIntervalSec_;

    // ROOT TFileService
    art::ServiceHandle<art::TFileService> tfs_;
    
    // Histograms 
    TH1F* h1_gaus_;

    // Counters
    std::size_t eventCounts_{0};

    // Misc member variables
    std::chrono::time_point<std::chrono::steady_clock> lastSendTime_;
    std::string outputPrefix_;
    TRandom3 random_;
    
};

// Constructor impl
CrvOtsDQM::CrvOtsDQM(fhicl::ParameterSet const &ps)
    : art::EDAnalyzer(ps)
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , outputTag_(ps.get<std::string>("outputTag", "CrvOtsDQM"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5))
{
    outputPrefix_ = "[CrvOtsDQM] ";
    __COUT__ << outputPrefix_ << "Initialised" << std::endl;
}

// Destructor impl
CrvOtsDQM::~CrvOtsDQM() {
    // Nothing to clean up 
}

void CrvOtsDQM::beginJob()
{
    if (diagLevel_ > 1) { 
        __COUT__ << outputPrefix_ << "Beginning job" << std::endl;
    }
    
    // Initialise histoSender 
    if (sendHists_) {
        try {
            histoSender_ = std::make_unique<HistoSender>(address_, port_);
            __COUT__ << outputPrefix_ << "Successfully connected HistoSender to " 
                     << address_ << ":" << port_ << std::endl;
        }
        catch (const std::exception& e) {
            __COUT__ << outputPrefix_ << "Failed to initialise HistoSender: " 
                     << e.what() << std::endl;
            // Disable histogram sending if connection fails
            sendHists_ = false;
        }
    }
    
    // Book histograms and register with TFS
    art::TFileDirectory dir = tfs_->mkdir(outputTag_);
    // h1_gaus_ = tfs_->make<TH1D>("h1_gaus", "Gaussian", 200, -100, 100);
    h1_gaus_ = dir.make<TH1F>("h1_gaus", "Gaussian", 200, -100, 100);

    // Seed TRandom3 
    random_.SetSeed(12345);  

    // Start last update time 
    lastSendTime_ = std::chrono::steady_clock::now();
}

void CrvOtsDQM::Fill(art::Event const& event) {
    // Fill histograms

    double randomValue = random_.Gaus(0, 25);
    h1_gaus_->Fill(randomValue);

    if (diagLevel_ > 2) {
        __COUT__ << outputPrefix_ << "Filled test histogram with value: " << randomValue << std::endl;
    }
}

void CrvOtsDQM::Send() {

    // Check flag
    if (!sendHists_) {
        return;
    }
    // Check pointer
    if (histoSender_ == nullptr) { 
        __COUT__ << outputPrefix_ << "ERROR: histoSender pointer is null" << std::endl;
        return; 
    }

    // Use the map method (three methods in HistoSender.cc)
    std::map<std::string, std::vector<TH1*>> testHists;
    testHists["testHists/h1_gaus:replace"] = {h1_gaus_};

    // Call send method 
    histoSender_->sendHistograms(testHists);

    if (diagLevel_ > 1) {
        __COUT__ << outputPrefix_ << "Sent histograms to " << address_ << ":" << port_ << std::endl;
    }
}

void CrvOtsDQM::analyze(art::Event const& event) {

    art::EventID eventID = event.id();

    if (diagLevel_ > 1) {
        __COUT__ << outputPrefix_ << "=================== " << eventID << " ===================" << std::endl;
    }

    ///////////////////// Fill /////////////////////

    Fill(event);

    ///////////////////// Send /////////////////////
    
    // Send histograms in fixed time intervals 
    auto currentTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = currentTime - lastSendTime_;

    if (elapsed.count() >= sendIntervalSec_) {
        Send();
        // Update last send time
        lastSendTime_ = currentTime;
    }

    // Update counters
    ++eventCounts_; 
}

void CrvOtsDQM::endJob() 
{
    if (diagLevel_ > 0) {
        // Print job-level statistics
        __COUT__ << outputPrefix_ << "================= End job summary =================" << std::endl;
        __COUT__ << outputPrefix_ << "Total events: " << eventCounts_ << std::endl;
        __COUT__ << outputPrefix_ << "===============================================" << std::endl;
    }

    if (diagLevel_ > 1) {
        __COUT__ << outputPrefix_ << "Ending job" << std::endl;
    }
    
    // Send final histograms & clean up
    if (sendHists_ && histoSender_ != nullptr) {
        Send();
        histoSender_.reset();
    }
    
}

DEFINE_ART_MODULE(ots::CrvOtsDQM)
}