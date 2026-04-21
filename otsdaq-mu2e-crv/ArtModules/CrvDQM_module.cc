// DQM and viewer for the CRV
// Sends histograms to otsdaq visualizer and standalone THttpServer
// Sam Grant, Simon Corrodi

// C++ includes
#include <algorithm>
#include <deque>
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
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <THttpServer.h>
#include <TPaveStats.h>
#include <TRandom3.h>
#include <TSystem.h>

// OTS includes
#include "otsdaq-mu2e/ArtModules/HistoSender.hh"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

// Offline includes
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"

// Custom styling
#include "otsdaq-mu2e-crv/ArtModules/CrvDQMStyle.hh"

namespace ots
{

class CrvDQM : public art::EDAnalyzer
{
  public:
	// Constructor
	explicit CrvDQM(fhicl::ParameterSet const& ps);
	// Destructor
	~CrvDQM() override;

  private:
	// Standard art methods
	void analyze(art::Event const& event) override;
	void beginJob() override;
	void endJob() override;

	/// Module methods
	void Send();
	void startHttpServer();
	void stopHttpServer();
	void updateWebDisplay(bool force = false);

	// fcl parameters
	art::InputTag crvDigiTag_;  // producer module label
	int           diagLevel_;
	int           port_;  // port to connect to
	std::string   address_;
	std::string   outputTag_;
	bool          sendHists_;
	bool          dummyHist_;

	// Histogram binning
	int   nBinsDigisPerEvt_;
	float maxDigisPerEvt_;
	int   nBinsPeakAdc_;
	float maxPeakAdc_;

	// Block-averaging for g_digisAvgVsEwt_: one point per avgBlockSize_ events,
	// keep at most avgGraphPoints_ points in the graph (drop oldest).
	std::size_t avgBlockSize_;
	std::size_t avgGraphPoints_;

	// HISTOGRAM SENDING
	std::unique_ptr<HistoSender> histoSender_;
	float                        sendIntervalSec_;

	// ROOT TFileService
	art::ServiceHandle<art::TFileService> tfs_;

	// Histograms
	TH1F*   h1_dummy_;         // dummy
	TH1F*   h1_channels_;      // global FEB channel hits
	TH2F*   h2_channels_;      // FEB vs channel hits
	TH1F*   h1_digisPerEvt_;   // digis per event
	TH1F*   h1_peakAdc_;       // peak ADC per digi
	TGraph* g_digisVsEwt_;     // digis vs event window tag (rolling sum)
	TGraph* g_digisAvgVsEwt_;  // mean digis per event, averaged over avgBlockSize_ events

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

	// Rate counters (printed every statLogPeriodSec_ seconds at diag level 0)
	double      statLogPeriodSec_{10.0};
	std::size_t statAnalyze_{0};
	std::size_t statUpdate_{0};
	std::size_t statUpdateCalls_{0};  // all calls, including those gated out
	std::size_t statUpdateGateA_{0};  // returned because disabled / no canvas
	std::size_t statUpdateGateB_{0};  // returned because refresh period not elapsed
	std::size_t statProcEvents_{0};
	std::size_t statSend_{0};
	std::chrono::time_point<std::chrono::steady_clock> statLastLog_;

	// Rolling window of (ewt, nDigis) for g_digisVsEwt_
	static constexpr std::size_t kEwtWindow_   = 100;
	static constexpr std::size_t kGraphPoints_ = 500;     // max points kept in TGraph
	static constexpr double      kEwtXRange_   = 100000;  // x-axis shows last N EWTs
	std::deque<std::pair<uint32_t, int>> ewtWindow_;
	long long                            ewtWindowSum_{0};

	// Block-averaging accumulators for g_digisAvgVsEwt_
	long long   avgBlockSum_{0};
	std::size_t avgBlockCount_{0};
	uint32_t    avgBlockFirstEwt_{0};
	bool avgSeedsCleared_{false};  // true once the two seed points have been dropped

	// Misc member variables
	std::chrono::time_point<std::chrono::steady_clock> lastSendTime_;
	std::string                                        outputPrefix_;
	TRandom3                                           random_;
};

// Constructor impl
CrvDQM::CrvDQM(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , crvDigiTag_(ps.get<std::string>("crvDigiTag", "crvdigi"))
    , diagLevel_(ps.get<int>("diagLevel", 3))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , outputTag_(ps.get<std::string>("outputTag", "CrvDQM"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , dummyHist_(ps.get<bool>("dummyHist", false))
    , nBinsDigisPerEvt_(ps.get<int>("nBinsDigisPerEvt", 200))
    , maxDigisPerEvt_(ps.get<float>("maxDigisPerEvt", 4000))
    , nBinsPeakAdc_(ps.get<int>("nBinsPeakAdc", 450))
    , maxPeakAdc_(ps.get<float>("maxPeakAdc", 4500))
    , avgBlockSize_(static_cast<std::size_t>(ps.get<int>("avgBlockSize", 30)))
    , avgGraphPoints_(static_cast<std::size_t>(ps.get<int>("avgGraphPoints", 1000)))
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5))
    , enableHttpServer_(ps.get<bool>("enableHttpServer", true))
    , httpPort_(ps.get<int>("httpPort", 8877))
    , onlineRefreshPeriodMs_(ps.get<float>("onlineRefreshPeriod", 500.f))
    , histColor_(ps.get<std::string>("histColor", "black"))
    , canvasName_(ps.get<std::string>("canvasName", "CrvDisplay"))
    , webCanvas_(nullptr)
    , httpServer_(nullptr)
{
	outputPrefix_ = "[CrvDQM] ";
	std::cout << outputPrefix_ << "Initialised"
	          << " (onlineRefreshPeriodMs=" << onlineRefreshPeriodMs_
	          << ", sendIntervalSec=" << sendIntervalSec_
	          << ", enableHttpServer=" << enableHttpServer_ << ")" << std::endl;
	// ROOT::EnableThreadSafety();
}

// Destructor impl
CrvDQM::~CrvDQM()
{
	// Nothing to clean up
}

void CrvDQM::beginJob()
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
		// ROC 0: 25 FEB slots
		// ROC 1: 6 FEB slots
		// 31 slots x 64 ch = 1984 ch
		h1_digisPerEvt_ = dir.make<TH1F>("h1_digisPerEvt",
		                                 "Hits / event;Hits / event;Events",
		                                 nBinsDigisPerEvt_,
		                                 0.5,
		                                 maxDigisPerEvt_ + 0.5);
		h1_digisPerEvt_->SetMinimum(0.5);
		h1_peakAdc_  = dir.make<TH1F>("h1_peakAdc",
                                     "Max sample ADC;Max sample ADC;Hits",
                                     nBinsPeakAdc_,
                                     0,
                                     maxPeakAdc_);
		h1_channels_ = dir.make<TH1F>("h1_channels",
		                              "Channel occupancy;Global channel ID;Hits",
		                              1984,
		                              -0.5,
		                              1983.5);
		h1_channels_->SetMinimum(0.5);
		h2_channels_  = dir.make<TH2F>("h2_channels",
                                      "FEB vs channel hit map;Channel;FEB",
                                      64,
                                      0.5,
                                      64.5,
                                      30,
                                      0.5,
                                      30.5);
		g_digisVsEwt_ = dir.make<TGraph>();
		g_digisVsEwt_->SetName("g_digisVsEwt");
		g_digisVsEwt_->SetTitle(
		    Form("Hits in last %zu EWTs;Event window tag;Hits (last %zu EWTs)",
		         kGraphPoints_,
		         kEwtWindow_));
		// Seed with two points so TGraphPainter has a non-degenerate Y range
		// when the canvas first draws (before any event arrives).
		g_digisVsEwt_->SetPoint(0, 0, 0);
		g_digisVsEwt_->SetPoint(1, 1, 1);

		g_digisAvgVsEwt_ = dir.make<TGraph>();
		g_digisAvgVsEwt_->SetName("g_digisAvgVsEwt");
		g_digisAvgVsEwt_->SetTitle(
		    Form("Mean hits per event (averaged over %zu events);"
		         "Event window tag;<hits / event>",
		         avgBlockSize_));
		// Two-point seed for initial frame
		g_digisAvgVsEwt_->SetPoint(0, 0, 0);
		g_digisAvgVsEwt_->SetPoint(1, 1, 1);
	}

	// Seed TRandom3
	random_.SetSeed(12345);

	// Start last update time
	lastSendTime_    = std::chrono::steady_clock::now();
	lastRefreshTime_ = lastSendTime_;
	statLastLog_     = lastSendTime_;

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
void CrvDQM::Send()
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
	++statSend_;

	// Send graphs
	if(!dummyHist_)
	{
		std::map<std::string, std::vector<TGraph*>> graphs;
		graphs["graphs/g_digisVsEwt:replace"]    = {g_digisVsEwt_};
		graphs["graphs/g_digisAvgVsEwt:replace"] = {g_digisAvgVsEwt_};
		histoSender_->sendGraphs(graphs);
	}

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Sent histograms to " << address_ << ":" << port_
		          << std::endl;
	}
}

void CrvDQM::startHttpServer()
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
		webCanvas_->Divide(3, 2);
	}

	int padIdx = 1;
	// Workaround for ROOT fatal "TPad::Range: y1 == y2 == 0" on empty
	// histograms. Put a tiny entry into bin 1 so max_bin_content > 0.
	// This is overwritten as soon as real data arrives.
	auto seedFrame = [](TH1* h) {
		if(!h)
			return;
		if(h->GetMaximum() <= h->GetMinimum())
		{
			h->SetBinContent(1, 1e-9);
			h->SetEntries(0);
		}
	};

	if(dummyHist_)
	{
		webCanvas_->cd(padIdx);
		CrvDQMStyle::FormatHist(h1_dummy_, histColor_);
		seedFrame(h1_dummy_);
		h1_dummy_->Draw("HIST");
	}
	else
	{
		// Pad 1: digis vs event window tag (rolling).
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatGraph(g_digisVsEwt_, histColor_);
		if(TH1F* frame = g_digisVsEwt_->GetHistogram())
		{
			frame->GetXaxis()->SetLimits(0.0, 1.0);
			frame->SetMinimum(0.0);
			frame->SetMaximum(1.0);
		}
		g_digisVsEwt_->Draw("AP");

		// Pad 2: digis per event
		webCanvas_->cd(padIdx++);
		gPad->SetLogx();
		gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_digisPerEvt_, histColor_);
		h1_digisPerEvt_->SetMinimum(0.5);
		seedFrame(h1_digisPerEvt_);
		h1_digisPerEvt_->Draw("HIST");

		// Pad 3: peak ADC
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatHist(h1_peakAdc_, histColor_);
		seedFrame(h1_peakAdc_);
		h1_peakAdc_->Draw("HIST");

		// Pad 4: global channel occupancy
		webCanvas_->cd(padIdx++);
		// gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_channels_, histColor_);
		h1_channels_->SetMinimum(0.5);
		seedFrame(h1_channels_);
		h1_channels_->Draw("HIST");
		gPad->Update();
		// Force stat box styling. Workaround for large-bin histogram
		TPaveStats* st = dynamic_cast<TPaveStats*>(h1_channels_->FindObject("stats"));
		if(st)
		{
			st->SetBorderSize(0);
			st->SetFillStyle(0);
			st->SetTextFont(42);
			st->SetTextSize(0.032);
			st->SetOptStat(111110);
		}

		// Pad 5: channel vs FEB hit map
		webCanvas_->cd(padIdx++);
		// gPad->SetLogz();
		gPad->SetRightMargin(0.14);
		if(h2_channels_)
		{
			CrvDQMStyle::FormatHist2D(h2_channels_);
			h2_channels_->GetZaxis()->SetTitle("Hits");
			seedFrame(h2_channels_);
			gStyle->SetPalette(kInvertedDarkBodyRadiator);
			h2_channels_->Draw("COLZ");
		}

		// Pad 6: block-averaged hits per event (points only, no connecting line)
		webCanvas_->cd(padIdx);
		CrvDQMStyle::FormatGraph(g_digisAvgVsEwt_, histColor_);
		if(TH1F* frame = g_digisAvgVsEwt_->GetHistogram())
		{
			frame->GetXaxis()->SetLimits(0.0, 1.0);
			frame->SetMinimum(0.0);
			frame->SetMaximum(1.0);
		}
		g_digisAvgVsEwt_->Draw("AP");
	}

	// Register canvas and histograms with server
	httpServer_->Register("/", webCanvas_);
	if(!dummyHist_)
	{
		httpServer_->Register("/", h1_digisPerEvt_);
		httpServer_->Register("/", h1_peakAdc_);
		httpServer_->Register("/", h1_channels_);
		httpServer_->Register("/", h2_channels_);
		httpServer_->Register("/", g_digisVsEwt_);
		httpServer_->Register("/", g_digisAvgVsEwt_);
	}

	// Publish refresh period so the HTML page can read it
	httpServer_->CreateItem("/config/refreshMs", Form("%.0f", onlineRefreshPeriodMs_));

	// Setup custom page
	std::string webPage = std::string(getenv("OTS_SOURCE")) +
	                      "/otsdaq-mu2e-crv/UserWebGUI/html/CrvDQM.html";
	httpServer_->SetDefaultPage(webPage);

	lastRefreshTime_ = std::chrono::steady_clock::now();

	std::cout << outputPrefix_ << "HTTP server running on http://localhost:" << httpPort_
	          << "/" << std::endl;
}

void CrvDQM::stopHttpServer()
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

void CrvDQM::updateWebDisplay(bool force)
{
	++statUpdateCalls_;

	if(!enableHttpServer_ || webCanvas_ == nullptr)
	{
		++statUpdateGateA_;
		return;
	}

	auto                                      now     = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> elapsed = now - lastRefreshTime_;

	if(!force && elapsed.count() < onlineRefreshPeriodMs_)
	{
		++statUpdateGateB_;
		return;
	}

	++statUpdate_;

	if(dummyHist_ && h1_dummy_)
	{
		double maxContent = h1_dummy_->GetBinContent(h1_dummy_->GetMaximumBin());
		h1_dummy_->GetYaxis()->SetRangeUser(0.0, std::max(1.0, 1.15 * maxContent));
	}
	else
	{
		if(h1_digisPerEvt_)
		{
			double maxContent =
			    h1_digisPerEvt_->GetBinContent(h1_digisPerEvt_->GetMaximumBin());
			h1_digisPerEvt_->GetYaxis()->SetRangeUser(0.5,
			                                          std::max(1.0, 1.15 * maxContent));
		}
		if(h1_channels_)
		{
			double maxContent =
			    h1_channels_->GetBinContent(h1_channels_->GetMaximumBin());
			h1_channels_->GetYaxis()->SetRangeUser(0.5, std::max(1.0, 1.15 * maxContent));
		}
	}

	// Re-apply palette right before update: global TColor state is fragile
	gStyle->SetPalette(kInvertedDarkBodyRadiator);

	// Re-apply per-object formatting that ROOT loses when internal
	// structures are recreated (e.g. TGraph histogram after SetPoint/RemovePoint)
	if(!dummyHist_)
	{
		CrvDQMStyle::FormatHist(h1_digisPerEvt_, histColor_);
		CrvDQMStyle::FormatHist(h1_peakAdc_, histColor_);
		CrvDQMStyle::FormatHist(h1_channels_, histColor_);
		CrvDQMStyle::FormatHist2D(h2_channels_);
		CrvDQMStyle::FormatGraph(g_digisVsEwt_, histColor_);

		// Auto-range both hits-graphs' Y axes from current data.
		auto autoRangeGraphY = [](TGraph* g) {
			if(!g || g->GetN() <= 0)
				return;
			double* y   = g->GetY();
			int     n   = g->GetN();
			double  yLo = *std::min_element(y, y + n);
			double  yHi = *std::max_element(y, y + n);
			if(yHi <= yLo)
				yHi = yLo + 1.0;
			double margin = 0.1 * (yHi - yLo);
			g->SetMinimum(std::max(0.0, yLo - margin));
			g->SetMaximum(yHi + margin);
			if(TH1F* frame = g->GetHistogram())
			{
				frame->SetMinimum(std::max(0.0, yLo - margin));
				frame->SetMaximum(yHi + margin);
			}
		};
		if(!ewtWindow_.empty())
			autoRangeGraphY(g_digisVsEwt_);
		autoRangeGraphY(g_digisAvgVsEwt_);
	}

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
	++statProcEvents_;
	lastRefreshTime_ = now;
}

void CrvDQM::analyze(art::Event const& event)
{
	++statAnalyze_;

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

			// Block-average: accumulate avgBlockSize_ events, then plot one point
			// at the mid-EWT of the block
			if(avgBlockCount_ == 0)
				avgBlockFirstEwt_ = eventID.event();
			avgBlockSum_ += nDigis;
			++avgBlockCount_;
			if(avgBlockCount_ >= avgBlockSize_)
			{
				double midEwt = 0.5 * (static_cast<double>(avgBlockFirstEwt_) +
				                       static_cast<double>(eventID.event()));
				double mean   = static_cast<double>(avgBlockSum_) /
				              static_cast<double>(avgBlockCount_);

				// Drop the two seed points on the first real fill
				if(!avgSeedsCleared_)
				{
					g_digisAvgVsEwt_->Set(0);
					avgSeedsCleared_ = true;
				}
				g_digisAvgVsEwt_->SetPoint(g_digisAvgVsEwt_->GetN(), midEwt, mean);

				while(static_cast<std::size_t>(g_digisAvgVsEwt_->GetN()) >
				      avgGraphPoints_)
				{
					g_digisAvgVsEwt_->RemovePoint(0);
				}

				avgBlockSum_   = 0;
				avgBlockCount_ = 0;
			}

			// Rolling sum of digis over the last kEwtWindow_ EWTs
			ewtWindow_.emplace_back(eventID.event(), nDigis);
			ewtWindowSum_ += nDigis;
			while(ewtWindow_.size() > kEwtWindow_)
			{
				ewtWindowSum_ -= ewtWindow_.front().second;
				ewtWindow_.pop_front();
			}
			// Drop the two seed points on the first real fill
			if(ewtWindow_.size() == 1 && g_digisVsEwt_->GetN() == 2)
			{
				g_digisVsEwt_->Set(0);
			}
			g_digisVsEwt_->SetPoint(
			    g_digisVsEwt_->GetN(), eventID.event(), ewtWindowSum_);

			// Keep only the last kGraphPoints_ points in the TGraph
			while(static_cast<std::size_t>(g_digisVsEwt_->GetN()) > kGraphPoints_)
			{
				g_digisVsEwt_->RemovePoint(0);
			}

			// Scale x-axis to show only the last kEwtXRange_ EWTs.
			// Use SetLimits on the backing histogram rather than SetRangeUser:
			// SetRangeUser requires the new window to lie within the existing
			// axis limits
			double currentEwt = static_cast<double>(eventID.event());
			double xLo        = std::max(0.0, currentEwt - kEwtXRange_);
			double xHi        = currentEwt;
			if(xHi <= xLo)
				xHi = xLo + 1.0;
			if(TH1F* frame = g_digisVsEwt_->GetHistogram())
			{
				frame->GetXaxis()->SetLimits(xLo, xHi);
			}
			// Auto-range the averaged-hits graph X axis over the points it
			// currently holds (up to avgGraphPoints_). Seeds are already
			// dropped before the first real point is added, so check > 0.
			if(g_digisAvgVsEwt_->GetN() > 0)
			{
				double* ax   = g_digisAvgVsEwt_->GetX();
				int     nAvg = g_digisAvgVsEwt_->GetN();
				double  aLo  = *std::min_element(ax, ax + nAvg);
				double  aHi  = *std::max_element(ax, ax + nAvg);
				if(aHi <= aLo)
					aHi = aLo + 1.0;
				if(TH1F* frame = g_digisAvgVsEwt_->GetHistogram())
				{
					frame->GetXaxis()->SetLimits(aLo, aHi);
				}
			}

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

	// Periodic rate stats
	std::chrono::duration<double> statElapsed = currentTime - statLastLog_;
	if(statElapsed.count() >= statLogPeriodSec_)
	{
		double dt = statElapsed.count();
		std::cout << outputPrefix_ << "Rates (last " << dt << " s): "
		          << "analyze=" << (statAnalyze_ / dt) << " Hz, "
		          << "updateWebDisplay=" << (statUpdate_ / dt) << " Hz"
		          << " (calls=" << statUpdateCalls_ << ", gateA=" << statUpdateGateA_
		          << ", gateB=" << statUpdateGateB_ << "), "
		          << "ProcessEvents=" << (statProcEvents_ / dt) << " Hz, "
		          << "sendHistograms=" << (statSend_ / dt) << " Hz" << std::endl;
		statAnalyze_     = 0;
		statUpdate_      = 0;
		statUpdateCalls_ = 0;
		statUpdateGateA_ = 0;
		statUpdateGateB_ = 0;
		statProcEvents_  = 0;
		statSend_        = 0;
		statLastLog_     = currentTime;
	}
}

void CrvDQM::endJob()
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

DEFINE_ART_MODULE(ots::CrvDQM)
}  // namespace ots
