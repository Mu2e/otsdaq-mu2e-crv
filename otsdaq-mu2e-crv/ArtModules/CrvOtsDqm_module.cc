// ROOT-based DQM and viewer for the CRV
// Intended for use with the OTS visualizer
// Author: Sam Grant
// Date: created May 2025, updated to use digis Nov 2025

// C++ includes
#include <algorithm>
#include <map>
#include <sstream>
#include <string>
// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"

// art/root includes
#include "art_root_io/TFileService.h"

// ROOT includes
#include <TCanvas.h>
#include <TColor.h>
#include <TH1.h>
#include <TH2.h>
#include <TPaveStats.h>
#include <THttpServer.h>
#include <TRandom3.h>
#include <TSystem.h>

// OTS includes
#include "otsdaq-mu2e/ArtModules/HistoSender.hh"  // HISTOGRAM SENDING
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

// Offline includes
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"

// Custom styling
#include "otsdaq-mu2e-crv/ArtModules/CrvDQMStyle.hh"

namespace ots
{

class CrvOtsDqm : public art::EDAnalyzer
{
  public:
	// Constructor
	explicit CrvOtsDqm(fhicl::ParameterSet const& ps);
	// Destructor
	~CrvOtsDqm() override;

  private:
	// Standard art methods
	void analyze(art::Event const& event) override;
	void beginJob() override;
	void endJob() override;

	/// Module methods
	// void Fill(art::Event const& event);
	void Send();
	void startHttpServer();
	void stopHttpServer();
	void updateWebDisplay(bool force = false);

	// CRV digi producer module label?

	// fcl parameters
	art::InputTag crvDigiTag_;  // producer module label
	// art::InputTag crvStatusTag_;
	int         diagLevel_;
	int         port_;  // port to connect to
	std::string address_;
	std::string outputTag_;
	bool        sendHists_;
	bool        dummyHist_;

	// Histogram binning
	int   nBinsDigisPerEvt_;
	float maxDigisPerEvt_;
	int   nBinsPeakAdc_;
	float maxPeakAdc_;

	// HISTOGRAM SENDING
	std::unique_ptr<HistoSender> histoSender_;
	float                        sendIntervalSec_;

	// ROOT TFileService
	art::ServiceHandle<art::TFileService> tfs_;

	// Histograms
	TH1F* h1_dummy_;        // dummy
	TH1F* h1_channels_;     // global FEB channel hits
	TH2F* h2_channels_;     // FEB vs channel hits
	TH1F* h1_digisPerEvt_;  // digis per event
	TH1F* h1_peakAdc_;      // peak ADC per digi

	// HTTP server & visualisation
	bool                                               enableHttpServer_;
	int                                                httpPort_;
	float                                              onlineRefreshPeriodMs_;
	std::string                                        histColor_;
	std::string                                        canvasName_;
	TCanvas*                                           webCanvas_;
	THttpServer*                                       httpServer_;
	std::chrono::time_point<std::chrono::steady_clock> lastRefreshTime_;

	// Counters
	std::size_t                          eventCounts_{0};
	std::size_t                          digiCounts_{0};
	std::set<uint8_t>                    activeFEBs_;
	std::set<uint8_t>                    activeROCs_;
	std::map<uint8_t, std::set<uint8_t>> rocFEBMap_;  // Track FEBs per ROC

	// Misc member variables
	std::chrono::time_point<std::chrono::steady_clock> lastSendTime_;
	std::string                                        outputPrefix_;
	TRandom3                                           random_;
};

// Constructor impl
CrvOtsDqm::CrvOtsDqm(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , crvDigiTag_(ps.get<std::string>("crvDigiTag", "crvdigi"))
    // , crvStatusTag_(ps.get<std::string>("crvStatusTag", "crvdigi"))
    , diagLevel_(ps.get<int>("diagLevel", 3))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , outputTag_(ps.get<std::string>("outputTag", "CrvOtsDqm"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , dummyHist_(ps.get<bool>("dummyHist", false))
    , nBinsDigisPerEvt_(ps.get<int>("nBinsDigisPerEvt", 200))
    , maxDigisPerEvt_(ps.get<float>("maxDigisPerEvt", 4000))
    , nBinsPeakAdc_(ps.get<int>("nBinsPeakAdc", 450))
    , maxPeakAdc_(ps.get<float>("maxPeakAdc", 4500))
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5))
    , enableHttpServer_(ps.get<bool>("enableHttpServer", true))
    , httpPort_(ps.get<int>("httpPort", 8877))
    , onlineRefreshPeriodMs_(ps.get<float>("onlineRefreshPeriod", 500.f))
    , histColor_(ps.get<std::string>("histColor", "black"))
    , canvasName_(ps.get<std::string>("canvasName", "CrvOtsDqmDisplay"))
    , webCanvas_(nullptr)
    , httpServer_(nullptr)
{
	outputPrefix_ = "[CrvOtsDqm] ";
	std::cout << outputPrefix_ << "Initialised" << std::endl;
}

// Destructor impl
CrvOtsDqm::~CrvOtsDqm()
{
	// Nothing to clean up
}

void CrvOtsDqm::beginJob()
{
	// Apply styling before booking histograms so they inherit the style
	CrvDQMStyle::SetStyle();

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Beginning job" << std::endl;
	}

	// Initialise histoSender
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
			// Disable histogram sending if connection fails
			sendHists_ = false;
		}
	}

	// Book histograms and register with TFS
	art::TFileDirectory dir = tfs_->mkdir(outputTag_);
	if(dummyHist_)
	{
		h1_dummy_ = dir.make<TH1F>("h1_dummy", "Dummy Gaussian", 200, -100, 100);
	}
	else
	{
		// ROC 1: 25 FEB slots (0-24), ROC 2: 6 FEB slots (25-30) = 31 slots x 64 ch = 1984
		// Port 0 per ROC reserved for misconfigured FEBs
		h1_digisPerEvt_ = dir.make<TH1F>(
		    "h1_digisPerEvt", "Hits / event;Hits / event;Events",
		    nBinsDigisPerEvt_, 0.5, maxDigisPerEvt_ + 0.5);
		h1_digisPerEvt_->SetMinimum(0.5);
		h1_peakAdc_ = dir.make<TH1F>(
		    "h1_peakAdc", "Max sample ADC;Max sample ADC;Hits",
		    nBinsPeakAdc_, 0, maxPeakAdc_);
		// ROC 1: 25 FEB slots (0-24), ROC 2: 6 FEB slots (25-30) = 31 slots x 64 ch = 1984
		// Port 0 per ROC reserved for misconfigured FEBs
		h1_channels_ = dir.make<TH1F>("h1_channels",
		                              "Channel occupancy;Global channel ID;Hits",
		                              1984,
		                              -0.5,
		                              1983.5);
		h1_channels_->SetMinimum(0.5);
		h2_channels_ = dir.make<TH2F>(
		    "h2_channels", "FEB vs channel hit map;Channel;FEB", 64, 0.5, 64.5, 30, 0.5, 30.5);
	}

	// Seed TRandom3
	random_.SetSeed(12345);

	// Start last update time
	lastSendTime_    = std::chrono::steady_clock::now();
	lastRefreshTime_ = lastSendTime_;

	if(enableHttpServer_)
	{
		try
		{
			startHttpServer();
		}
		catch(const std::exception& e)
		{
			std::cout << outputPrefix_ << "Failed to start HTTP server: " << e.what()
			          << std::endl;
			enableHttpServer_ = false;
		}
	}

	updateWebDisplay();
}
void CrvOtsDqm::Send()
{
	// Check flag
	if(!sendHists_)
	{
		return;
	}

	// Check pointer
	if(histoSender_ == nullptr)
	{
		std::cout << outputPrefix_ << "ERROR: histoSender pointer is null" << std::endl;
		return;
	}

	// Use the map method (three methods in HistoSender.cc)
	std::map<std::string, std::vector<TH1*>> hists;
	if(dummyHist_)
	{
		hists["hists/h1_gaus:replace"] = {h1_dummy_};
	}
	else
	{
		hists["hists/h1_channels:replace"]    = {h1_channels_};
		hists["hists/h2_channels:replace"]    = {h2_channels_};
		hists["hists/h1_digisPerEvt:replace"] = {h1_digisPerEvt_};
		hists["hists/h1_peakAdc:replace"]     = {h1_peakAdc_};
	}

	// Call send method
	histoSender_->sendHistograms(hists);

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Sent histograms to " << address_ << ":" << port_
		          << std::endl;
	}
}

void CrvOtsDqm::startHttpServer()
{
	// Create HTTP server
	httpServer_ = new THttpServer(Form("http:%d", httpPort_));

	// Create canvas
	webCanvas_ = new TCanvas(canvasName_.c_str(), "CRV DQM");
	if(dummyHist_)
	{
		webCanvas_->Divide(1, 1);
	}
	else
	{
		webCanvas_->Divide(2, 2);
	}

	int padIdx = 1;
	if(dummyHist_)
	{
		webCanvas_->cd(padIdx);
		CrvDQMStyle::FormatHist(h1_dummy_, histColor_);
		h1_dummy_->Draw("HIST");
	}
	else
	{
		// Pad 1: digis per event
		webCanvas_->cd(padIdx++);
		gPad->SetLogx();
		gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_digisPerEvt_, histColor_);
		h1_digisPerEvt_->SetMinimum(0.5);
		h1_digisPerEvt_->Draw("HIST");

		// Pad 2: peak ADC
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatHist(h1_peakAdc_, histColor_);
		h1_peakAdc_->Draw("HIST");

		// Pad 3: global channel occupancy
		webCanvas_->cd(padIdx++);
		gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_channels_, histColor_);
		h1_channels_->SetMinimum(0.5);
		h1_channels_->Draw("HIST");
		gPad->Update();
		// Force stat box styling — workaround for large-bin histogram
		TPaveStats* st = dynamic_cast<TPaveStats*>(h1_channels_->FindObject("stats"));
		if(st)
		{
			st->SetBorderSize(0);
			st->SetFillStyle(0);
			st->SetTextFont(42);
			st->SetTextSize(0.032);
			st->SetOptStat(111110);
		}

		// Pad 4: channel vs FEB hit map
		webCanvas_->cd(padIdx);
		gPad->SetLogz();
		gPad->SetRightMargin(0.14);
		if(h2_channels_)
		{
			CrvDQMStyle::FormatHist2D(h2_channels_);
			h2_channels_->GetZaxis()->SetTitle("Hits");
			gStyle->SetPalette(kInvertedDarkBodyRadiator);
			h2_channels_->Draw("COLZ");
		}
	}

	// Register canvas and histograms with server
	httpServer_->Register("/", webCanvas_);
	if(!dummyHist_)
	{
		httpServer_->Register("/", h1_digisPerEvt_);
		httpServer_->Register("/", h1_peakAdc_);
		httpServer_->Register("/", h1_channels_);
		httpServer_->Register("/", h2_channels_);
	}

	// Publish refresh period so the HTML page can read it
	httpServer_->CreateItem("/config/refreshMs", Form("%.0f", onlineRefreshPeriodMs_));

	// Setup custom page
	std::string webPage = std::string(getenv("OTS_SOURCE")) +
	                      "/otsdaq-mu2e-crv/UserWebGUI/html/CrvDqm.html";
	httpServer_->SetDefaultPage(webPage);

	lastRefreshTime_ = std::chrono::steady_clock::now();

	std::cout << outputPrefix_ << "HTTP server running on http://localhost:" << httpPort_
	          << "/" << std::endl;
}

void CrvOtsDqm::stopHttpServer()
{
	if(httpServer_ != nullptr)
	{
		delete httpServer_;
		httpServer_ = nullptr;
	}

	if(webCanvas_ != nullptr)
	{
		delete webCanvas_;
		webCanvas_ = nullptr;
	}
}

void CrvOtsDqm::updateWebDisplay(bool force)
{
	if(!enableHttpServer_ || webCanvas_ == nullptr)
	{
		return;
	}

	auto                                      now     = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> elapsed = now - lastRefreshTime_;

	if(!force && elapsed.count() < onlineRefreshPeriodMs_)
	{
		return;
	}

	if(dummyHist_ && h1_dummy_)
	{
		double maxContent = h1_dummy_->GetBinContent(h1_dummy_->GetMaximumBin());
		h1_dummy_->GetYaxis()->SetRangeUser(0.0, std::max(1.0, 1.15 * maxContent));
	}
	else
	{
		if(h1_digisPerEvt_)
		{
			double maxContent = h1_digisPerEvt_->GetBinContent(h1_digisPerEvt_->GetMaximumBin());
			h1_digisPerEvt_->GetYaxis()->SetRangeUser(0.5, std::max(1.0, 1.15 * maxContent));
		}
		if(h1_channels_)
		{
			double maxContent = h1_channels_->GetBinContent(h1_channels_->GetMaximumBin());
			h1_channels_->GetYaxis()->SetRangeUser(0.5, std::max(1.0, 1.15 * maxContent));
		}
	}

	// Re-apply palette right before update — global TColor state is fragile
	gStyle->SetPalette(kInvertedDarkBodyRadiator);

	if(eventCounts_ > 0)
	{
		for(int i = 1; i <= webCanvas_->GetListOfPrimitives()->GetSize(); ++i)
		{
			webCanvas_->cd(i);
			gPad->Modified();
		}
	}
	webCanvas_->cd();
	webCanvas_->Modified();
	webCanvas_->Update();

	gSystem->ProcessEvents();
	lastRefreshTime_ = now;
}

void CrvOtsDqm::analyze(art::Event const& event)
{
	art::EventID eventID = event.id();

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "=================== " << eventID
		          << " ===================" << std::endl;
	}

	if(dummyHist_)
	{
		// Fill dummy histogram only
		double randomValue = random_.Gaus(0, 25);
		h1_dummy_->Fill(randomValue);

		if(diagLevel_ > 2)
		{
			std::cout << outputPrefix_
			          << "Filled dummy histogram with value: " << randomValue
			          << std::endl;
		}
	}
	else
	{
		///////////////////// Process digis /////////////////////
		art::Handle<mu2e::CrvDigiCollection> crvDigisHandle;
		event.getByLabel(crvDigiTag_, crvDigisHandle);

		int nDigis = 0;

		if(crvDigisHandle.isValid() && !crvDigisHandle->empty())
		{
			const mu2e::CrvDigiCollection& crvDigis = *crvDigisHandle;
			nDigis                                  = crvDigis.size();

			// Loop over digis
			for(const auto& digi : crvDigis)
			{
				// Get channel identifier (barIndex)
				// int channelId = digi.GetScintillatorBarIndex().asInt();
				uint8_t roc        = digi.GetROC();         // 1-2 (extracted)
				uint8_t feb        = digi.GetFEB();         // 1-28 (extracted)
				uint8_t febChannel = digi.GetFEBchannel();  // 0-63

				// === Global channel IDs ===
				// 25 FEB slots per ROC (0-24), FEB 0 reserved for misconfigured boards
				// ROC 1: slots 0-24, ROC 2: slots 25-49
				int globalFebId     = ((roc - 1) * 25) + feb;
				int globalChannelId = globalFebId * 64 + febChannel;

				// Fill channel histograms
				h1_channels_->Fill(globalChannelId);
				h2_channels_->Fill(febChannel + 1, globalFebId);

				// Max sample ADC from waveform
				const auto& adcs = digi.GetADCs();
				if(!adcs.empty())
				{
					int16_t maxSample = *std::max_element(adcs.begin(), adcs.end());
					h1_peakAdc_->Fill(maxSample);
				}

				// Track active ROCs and FEBs
				activeROCs_.insert(roc);
				activeFEBs_.insert(globalFebId);
				rocFEBMap_[roc].insert(feb);
			}

			digiCounts_ += nDigis;
			h1_digisPerEvt_->Fill(nDigis);

			if(diagLevel_ > 1)
			{
				std::cout << outputPrefix_ << "Found " << nDigis << std::endl;
				//           << nNZSDigis << " NZS, "
				//           << nOddTimestamp << " odd timestamps)" << std::endl;
				// std::cout << "  Active channels: " << nActiveChannels << std::endl;
			}
		}
		else
		{
			if(diagLevel_ > 1)
			{
				std::cout << outputPrefix_ << "Warning! No CRV digis found" << std::endl;
			}
		}
	}

	///////////////////// Send /////////////////////

	// Send histograms in fixed time intervals
	auto                          currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed     = currentTime - lastSendTime_;

	if(elapsed.count() >= sendIntervalSec_)
	{
		Send();
		// Update last send time
		lastSendTime_ = currentTime;
	}

	updateWebDisplay();

	// Update event counter
	++eventCounts_;
}

void CrvOtsDqm::endJob()
{
	if(diagLevel_ > 0)
	{
		// Print job-level statistics
		std::cout << outputPrefix_
		          << "================= End job summary =================" << std::endl;
		std::cout << outputPrefix_ << "Total events: " << eventCounts_ << std::endl;
		if(!dummyHist_)
		{
			std::cout << outputPrefix_ << "Total digis: " << digiCounts_ << std::endl;
			std::cout << outputPrefix_ << "Active FEBs: " << activeFEBs_.size()
			          << std::endl;
			// Print FEBs per ROC
			for(auto& [roc, febs] : rocFEBMap_)
			{
				std::cout << outputPrefix_ << "ROC " << (int)roc << " has " << febs.size()
				          << " FEBs: ";
				for(auto feb : febs)
				{
					std::cout << (int)feb << " ";
				}
				std::cout << std::endl;
			}
		}
		std::cout << outputPrefix_
		          << "===============================================" << std::endl;
	}

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Ending job" << std::endl;
	}

	// Send final histograms & clean up
	if(sendHists_ && histoSender_ != nullptr)
	{
		Send();
		histoSender_.reset();
	}

	if(enableHttpServer_)
	{
		updateWebDisplay(true);
		stopHttpServer();
	}
}

DEFINE_ART_MODULE(ots::CrvOtsDqm)
}  // namespace ots
