// DQM and viewer for the CRV
// Sends histograms to otsdaq visualizer and standalone THttpServer
// Sam Grant, Simon Corrodi

// C++ includes
#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

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
#include "Offline/RecoDataProducts/inc/CrvStatus.hh"

// Timing
#include "otsdaq-mu2e-crv/ArtModules/CrvCFTime.hh"

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
	art::InputTag crvDigiTag_;    // producer module label
	art::InputTag crvStatusTag_;  // CrvStatus producer module label
	int           diagLevel_;
	int           port_;  // port to connect to
	std::string   address_;
	std::string   outputTag_;
	bool          sendHists_;
	bool          dummyHist_;
	bool          saveCanvasesToPdf_;
	bool          showSameFpgaTimingInCanvas_;
	std::string   canvasPdfFile_;

	// Histogram binning
	int   nBinsDigisPerEvt_;
	float maxDigisPerEvt_;
	int   nBinsPeakAdc_;
	float maxPeakAdc_;
	int   nBinsTdc_;
	float maxTdc_;

	// Timing parameters
	double cfFraction_;       // constant-fraction threshold (default 0.20)
	float  dtBinSize_;        // bin width in ns for dt histograms
	float  dtRange_;          // +/- range in ns for dt histograms
	int    minAmplitude_;     // minimum amplitude (peak - baseline) to accept a hit
	int    nBinsDt_;          // number of bins for dt histograms (computed from range/binSize)

	// Block-averaging for g_digisAvgVsEwt_: one point per avgBlockSize_ events,
	// keep at most avgGraphPoints_ points in the graph (drop oldest).
	std::size_t avgBlockSize_;
	std::size_t avgGraphPoints_;
	std::size_t channelsWindowEwts_;

	// HISTOGRAM SENDING
	std::unique_ptr<HistoSender> histoSender_;
	float                        sendIntervalSec_;

	// ROOT TFileService
	art::ServiceHandle<art::TFileService> tfs_;

	// Histograms
	TH1F*   h1_dummy_;         // dummy
	TH1F*   h1_channels_;      // global FEB channel hits
	TH1F*   h1_channelsLastEwt_;  // channel hits with EWT in [current-window, current]
	TH2F*   h2_channels_;      // FEB vs channel hits
	TH1F*   h1_digisPerEvt_;   // digis per event
	TH1F*   h1_peakAdc_;       // peak ADC per digi
	TH1F*   h1_tdc_;           // digi start timestamp
	TGraph* g_digisVsEwt_;     // digis vs event window tag (rolling sum)
	TGraph* g_digisAvgVsEwt_;  // mean digis per event, averaged over avgBlockSize_ events

	// MicroBunchStatus vs EWT per ROC link (only updated on change)
	std::map<uint8_t, TGraph*>  g_ubStatusVsEwt_;     // linkID -> TGraph
	std::map<uint8_t, uint32_t> lastMicroBunchStatus_; // linkID -> last seen value

	// Timing histograms (dt between pairs), created on-demand
	// Key: pair of global FEB IDs (low, high)
	std::map<std::pair<uint8_t, uint8_t>, TH1F*> h1_dtFebPairs_;
	// Key: (globalFebId, fpgaPair) where fpgaPair encodes (fpgaA, fpgaB) as fpgaA*4+fpgaB
	std::map<std::pair<uint8_t, uint8_t>, TH1F*> h1_dtFpgaPairs_;

	// Timestamp (startTDC) distributions, created on-demand
	std::map<uint8_t, TH1F*>                     h1_tdcPerFeb_;   // globalFebId -> hist
	std::map<std::pair<uint8_t, uint8_t>, TH1F*> h1_tdcPerFpga_;  // (globalFebId, fpga) -> hist

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
	static constexpr std::size_t kEwtWindow_   = 1000;
	static constexpr std::size_t kGraphPoints_ = 10000;    // max points kept in TGraph
	static constexpr double      kEwtXRange_   = 1000000;  // x-axis shows last N EWTs
	std::deque<std::pair<uint32_t, int>> ewtWindow_;
	long long                            ewtWindowSum_{0};
	std::deque<std::pair<uint32_t, std::vector<uint16_t>>> recentChannelHitsByEwt_;

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
    , crvStatusTag_(ps.get<std::string>("crvStatusTag", "crvdigi"))
    , diagLevel_(ps.get<int>("diagLevel", 3))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , outputTag_(ps.get<std::string>("outputTag", "CrvDQM"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , dummyHist_(ps.get<bool>("dummyHist", false))
	, saveCanvasesToPdf_(ps.get<bool>("saveCanvasesToPdf", false))
	, showSameFpgaTimingInCanvas_(ps.get<bool>("showSameFpgaTimingInCanvas", true))
	, canvasPdfFile_(ps.get<std::string>("canvasPdfFile", "CrvDQM.pdf"))
    , nBinsDigisPerEvt_(ps.get<int>("nBinsDigisPerEvt", 200))
    , maxDigisPerEvt_(ps.get<float>("maxDigisPerEvt", 4000))
    , nBinsPeakAdc_(ps.get<int>("nBinsPeakAdc", 450))
    , maxPeakAdc_(ps.get<float>("maxPeakAdc", 4500))
    , nBinsTdc_(ps.get<int>("nBinsTdc", 400))
    , maxTdc_(ps.get<float>("maxTdc", 40000))
    , cfFraction_(ps.get<double>("cfFraction", 0.20))
    , dtBinSize_(ps.get<float>("dtBinSize", 0.5))
    , dtRange_(ps.get<float>("dtRange", 100.0))
    , minAmplitude_(ps.get<int>("minAmplitude", 10))
    , avgBlockSize_(static_cast<std::size_t>(ps.get<int>("avgBlockSize", 30)))
    , avgGraphPoints_(static_cast<std::size_t>(ps.get<int>("avgGraphPoints", 1000)))
	    , channelsWindowEwts_(
		    static_cast<std::size_t>(ps.get<int>("channelsWindowEwts", 50000)))
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
	nBinsDt_      = static_cast<int>(2.0 * dtRange_ / dtBinSize_);
	if(channelsWindowEwts_ == 0)
		channelsWindowEwts_ = 1;
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
		// ROC 1: 8 FEB slots
		// 31 slots x 64 ch = 2112 ch
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
		h1_tdc_  = dir.make<TH1F>("h1_tdc",
                                     "Start timestamp of digi in units of 12.5ns;Start timestamp of digi;Digis",
                                     nBinsTdc_,
                                     0,
                                     maxTdc_);
		h1_channels_ = dir.make<TH1F>("h1_channels",
		                              "Channel occupancy;Global channel ID;Hits",
		                              2112,
		                              -0.5,
		                              2111.5);
		h1_channels_->SetMinimum(0.5);
		h1_channelsLastEwt_ =
		    dir.make<TH1F>("h1_channelsLastEwt",
		                   Form("Channel occupancy (EWT span %zu);Global channel ID;Hits",
		                        channelsWindowEwts_),
		                   2112,
		                   -0.5,
		                   2111.5);
		h1_channelsLastEwt_->SetMinimum(0.5);
		h2_channels_  = dir.make<TH2F>("h2_channels",
                                      "FEB vs channel hit map;Channel;FEB",
                                      64,
                                      0.5,
                                      64.5,
                                      32,
                                      0.5,
                                      32.5);
		g_digisVsEwt_ = dir.make<TGraph>();
		g_digisVsEwt_->SetName("g_digisVsEwt");
		g_digisVsEwt_->SetTitle(
		    Form("Hits in last %zu EWTs (%zu points);"
		         "Event window tag;Hits",
		         kEwtWindow_,
		         kGraphPoints_));
		CrvDQMStyle::FormatGraph(g_digisVsEwt_, histColor_);
		g_digisVsEwt_->SetMarkerColor(g_digisVsEwt_->GetLineColor());
		g_digisVsEwt_->SetDrawOption("AP");
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
		CrvDQMStyle::FormatGraph(g_digisAvgVsEwt_, histColor_);
		g_digisAvgVsEwt_->SetMarkerColor(g_digisAvgVsEwt_->GetLineColor());
		g_digisAvgVsEwt_->SetDrawOption("AP");
		// Two-point seed for initial frame
		g_digisAvgVsEwt_->SetPoint(0, 0, 0);
		g_digisAvgVsEwt_->SetPoint(1, 1, 1);

		// MicroBunchStatus graphs are created on-demand per ROC link
		// in analyze() when a new link ID is first encountered.
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
		hists["crv/h1_gaus:replace"] = {h1_dummy_};
	}
	else
	{
		hists["crv/h1_channels:replace"]    = {h1_channels_};
		hists["crv/h1_channelsLastEwt:replace"] = {h1_channelsLastEwt_};
		hists["crv/h2_channels:replace"]    = {h2_channels_};
		hists["crv/h1_digisPerEvt:replace"] = {h1_digisPerEvt_};
		hists["crv/h1_peakAdc:replace"]     = {h1_peakAdc_};
		hists["crv/h1_tdc:replace"]         = {h1_tdc_};

		for(const auto& [key, h] : h1_dtFebPairs_)
		{
			if(h == nullptr)
				continue;
			hists["crv/timing_feb:replace"].push_back(h);
		}

		for(const auto& [key, h] : h1_dtFpgaPairs_)
		{
			if(h == nullptr)
				continue;
			hists["crv/timing_fpga:replace"].push_back(h);
		}
	}

	// Call send method
	histoSender_->sendHistograms(hists);
	++statSend_;

	// Send graphs
	if(!dummyHist_)
	{
		std::map<std::string, std::vector<TGraph*>> graphs;
		graphs["crv/graphs:replace"] = {g_digisVsEwt_, g_digisAvgVsEwt_};
		for(auto& [linkID, g] : g_ubStatusVsEwt_)
		{
			if(g != nullptr)
				graphs["crv/graphs:replace"].push_back(g);
		}
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

		// Pad 4: TDC
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatHist(h1_tdc_, histColor_);
		seedFrame(h1_tdc_);
		h1_tdc_->Draw("HIST");

		// Pad 5: global channel occupancy
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
			st->SetTextSize(0.040);
			st->SetOptStat(111110);
		}

		// Pad 6: channel vs FEB hit map
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

		// Pad 7: block-averaged hits per event (points only, no connecting line)
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
		httpServer_->Register("/", h1_tdc_);
		httpServer_->Register("/", h1_channels_);
		httpServer_->Register("/", h1_channelsLastEwt_);
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
		if(h1_channelsLastEwt_)
		{
			double maxContent =
			    h1_channelsLastEwt_->GetBinContent(h1_channelsLastEwt_->GetMaximumBin());
			h1_channelsLastEwt_->GetYaxis()->SetRangeUser(
			    0.5, std::max(1.0, 1.15 * maxContent));
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
		CrvDQMStyle::FormatHist(h1_tdc_, histColor_);
		CrvDQMStyle::FormatHist(h1_channels_, histColor_);
		CrvDQMStyle::FormatHist(h1_channelsLastEwt_, histColor_);
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
		{
			autoRangeGraphY(g_digisVsEwt_);
			// autoRangeGraphY may trigger TGraph::GetHistogram() to recreate the
			// frame, which defaults X limits to the data range and breaks the
			// sliding window set in analyze(). Re-apply the sliding window here.
			double currentEwt = static_cast<double>(ewtWindow_.back().first);
			double xLo        = std::max(0.0, currentEwt - kEwtXRange_);
			double xHi        = currentEwt;
			if(xHi <= xLo)
				xHi = xLo + 1.0;
			if(TH1F* frame = g_digisVsEwt_->GetHistogram())
				frame->GetXaxis()->SetLimits(xLo, xHi);
		}
		autoRangeGraphY(g_digisAvgVsEwt_);
		// Same fix for the averaged graph: span the points it currently holds.
		if(g_digisAvgVsEwt_->GetN() > 0)
		{
			double* ax   = g_digisAvgVsEwt_->GetX();
			int     nAvg = g_digisAvgVsEwt_->GetN();
			double  aLo  = *std::min_element(ax, ax + nAvg);
			double  aHi  = *std::max_element(ax, ax + nAvg);
			if(aHi <= aLo)
				aHi = aLo + 1.0;
			if(TH1F* frame = g_digisAvgVsEwt_->GetHistogram())
				frame->GetXaxis()->SetLimits(aLo, aHi);
		}
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
		std::vector<uint16_t> eventChannelHits;

		if(crvDigisHandle.isValid() && !crvDigisHandle->empty())
		{
			const mu2e::CrvDigiCollection& crvDigis = *crvDigisHandle;
			nDigis                                  = crvDigis.size();

			// Hit times grouped as [globalFebId][fpga] = vector of {time_ns, channel}
			struct FpgaHit
			{
				double  time_ns;
				uint8_t channel;
			};
			std::map<uint8_t, std::map<uint8_t, std::vector<FpgaHit>>> hitTimes;

			// Loop over digis
			for(const auto& digi : crvDigis)
			{
				uint8_t roc        = digi.GetROC();
				uint8_t feb        = digi.GetFEB();
				uint8_t febChannel = digi.GetFEBchannel();

				if(roc == 4)
					roc = 2;

				int globalFebId     = ((roc - 1) * 25) + feb;
				int globalChannelId = globalFebId * 64 + febChannel;

				h1_channels_->Fill(globalChannelId);
				eventChannelHits.push_back(static_cast<uint16_t>(globalChannelId));
				h2_channels_->Fill(febChannel + 1, globalFebId);

				const auto& adcs = digi.GetADCs();
				if(!adcs.empty())
				{
					int16_t maxSample = *std::max_element(adcs.begin(), adcs.end());
					h1_peakAdc_->Fill(maxSample);
				}

				uint16_t tdc = digi.GetStartTDC();
				h1_tdc_->Fill(tdc);

				// CF timing
				crv::CFResult cf = crv::cfTime(adcs, cfFraction_, minAmplitude_);
				if(cf.valid)
				{
					double absTime_ns =
					    cf.time_ns + digi.GetStartTDC() * crv::kDigitizationPeriodNs;
					uint8_t fpga = febChannel / 16;
					hitTimes[static_cast<uint8_t>(globalFebId)][fpga]
					    .push_back({absTime_ns, febChannel});
				}

				activeROCs_.insert(roc);
				activeFEBs_.insert(globalFebId);
				rocFEBMap_[roc].insert(feb);
			}

			///////////////////// Timing pairs /////////////////////

			// FEB-to-FEB: first hit from each FEB
			{
				std::vector<std::pair<uint8_t, double>> febFirstHit;
				for(auto& [febId, fpgaMap] : hitTimes)
				{
					double earliest = std::numeric_limits<double>::max();
					for(auto& [fpga, hits] : fpgaMap)
						for(auto& hit : hits)
							if(hit.time_ns < earliest)
								earliest = hit.time_ns;
					febFirstHit.push_back({febId, earliest});
				}

				for(std::size_t i = 0; i < febFirstHit.size(); ++i)
				{
					for(std::size_t j = i + 1; j < febFirstHit.size(); ++j)
					{
						uint8_t lo = febFirstHit[i].first;
						uint8_t hi = febFirstHit[j].first;
						double  dt = febFirstHit[j].second - febFirstHit[i].second;
						auto    key = std::make_pair(lo, hi);

						if(h1_dtFebPairs_.find(key) == h1_dtFebPairs_.end())
						{
							art::TFileDirectory tdir =
							    tfs_->mkdir(outputTag_).mkdir("timing_feb");
							std::string name = Form("dt_feb%02d_feb%02d", lo, hi);
							std::string title =
							    Form("#Deltat FEB %02d - FEB %02d;#Deltat [ns];Entries",
							         lo, hi);
							h1_dtFebPairs_[key] = tdir.make<TH1F>(
							    name.c_str(), title.c_str(), nBinsDt_, -dtRange_, dtRange_);
						}
						h1_dtFebPairs_[key]->Fill(dt);
					}
				}
			}

			// FPGA pairs within each FEB (all 10 combinations: 0-0,0-1,...,3-3)
			for(auto& [febId, fpgaMap] : hitTimes)
			{
				for(auto itA = fpgaMap.begin(); itA != fpgaMap.end(); ++itA)
				{
					for(auto itB = itA; itB != fpgaMap.end(); ++itB)
					{
						uint8_t fpgaA = itA->first;
						uint8_t fpgaB = itB->first;
						const auto& hitsA = itA->second;
						const auto& hitsB = itB->second;

						uint8_t pairCode = fpgaA * 4 + fpgaB;
						auto    key = std::make_pair(febId, pairCode);

						if(h1_dtFpgaPairs_.find(key) == h1_dtFpgaPairs_.end())
						{
							art::TFileDirectory tdir =
							    tfs_->mkdir(outputTag_).mkdir("timing_fpga");
							std::string name =
							    Form("dt_feb%02d_fpga%d_fpga%d", febId, fpgaA, fpgaB);
							std::string title = Form(
							    "#Deltat FEB %02d FPGA %d - FPGA %d;#Deltat [ns];Entries",
							    febId, fpgaA, fpgaB);
							h1_dtFpgaPairs_[key] = tdir.make<TH1F>(
							    name.c_str(), title.c_str(), nBinsDt_, -dtRange_, dtRange_);
						}

						TH1F* h = h1_dtFpgaPairs_[key];

						if(fpgaA == fpgaB)
						{
							// Same FPGA: pairs of different channels only
							for(std::size_t ia = 0; ia < hitsA.size(); ++ia)
								for(std::size_t ib = ia + 1; ib < hitsA.size(); ++ib)
									if(hitsA[ia].channel != hitsA[ib].channel)
										h->Fill(hitsA[ib].time_ns - hitsA[ia].time_ns);
						}
						else
						{
							// Cross-FPGA: all pairs
							for(const auto& hA : hitsA)
								for(const auto& hB : hitsB)
									h->Fill(hB.time_ns - hA.time_ns);
						}
					}
				}
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

		// Rolling channel occupancy over an EWT span.
		// Keep one compact channel-hit list per EWT and update incrementally by
		// adding the newest EWT and subtracting entries that fall below
		// (currentEwt - channelsWindowEwts_).
		const uint32_t currentEwt = eventID.event();
		recentChannelHitsByEwt_.emplace_back(currentEwt, std::move(eventChannelHits));
		for(const auto channelId : recentChannelHitsByEwt_.back().second)
		{
			if(channelId < 2112)
				h1_channelsLastEwt_->AddBinContent(static_cast<Int_t>(channelId) + 1, 1.0);
		}

		const uint32_t minKeepEwt =
		    (currentEwt > channelsWindowEwts_)
		        ? static_cast<uint32_t>(currentEwt - channelsWindowEwts_)
		        : 0u;
		while(!recentChannelHitsByEwt_.empty() &&
		      recentChannelHitsByEwt_.front().first < minKeepEwt)
		{
			for(const auto channelId : recentChannelHitsByEwt_.front().second)
			{
				if(channelId < 2112)
					h1_channelsLastEwt_->AddBinContent(static_cast<Int_t>(channelId) + 1,
					                                  -1.0);
			}
			recentChannelHitsByEwt_.pop_front();
		}

		///////////////////// Process CrvStatus (MicroBunchStatus) /////////////////////
		art::Handle<mu2e::CrvStatusCollection> crvStatusHandle;
		event.getByLabel(crvStatusTag_, crvStatusHandle);

		if(crvStatusHandle.isValid() && !crvStatusHandle->empty())
		{
			for(const auto& status : *crvStatusHandle)
			{
				uint8_t linkID = status.GetLinkID();
				// GetROCHeader() is non-const in CrvStatus.hh; const_cast needed
				auto& rocHeaders =
				    const_cast<mu2e::CrvStatus&>(status).GetROCHeader();
				if(rocHeaders.empty())
					continue;

				uint32_t ubStatus = rocHeaders[0].GetMicroBunchStatus();
				uint64_t ewt     = status.GetEventWindowTag();

				// Create graph on first encounter of this link
				if(g_ubStatusVsEwt_.find(linkID) == g_ubStatusVsEwt_.end())
				{
					art::TFileDirectory dir = tfs_->mkdir(outputTag_);
					std::string name  = Form("g_ubStatusVsEwt_link%d", linkID);
					std::string title = Form("MicroBunchStatus vs EWT (link %d);"
					                         "Event window tag;MicroBunchStatus",
					                         linkID);
					TGraph* g = dir.make<TGraph>();
					g->SetName(name.c_str());
					g->SetTitle(title.c_str());
					CrvDQMStyle::FormatGraph(g, histColor_);
					g->SetMarkerColor(g->GetLineColor());
					g->SetDrawOption("AP");
					g->SetPoint(0, static_cast<double>(ewt),
					            static_cast<double>(ubStatus));
					g_ubStatusVsEwt_[linkID]     = g;
					lastMicroBunchStatus_[linkID] = ubStatus;

					if(enableHttpServer_ && httpServer_)
						httpServer_->Register("/", g);

					if(diagLevel_ > 0)
					{
						std::cout << outputPrefix_
						          << "Created MicroBunchStatus graph for link "
						          << (int)linkID << std::endl;
					}
					continue;
				}

				// Only add a point when the status changes
				if(ubStatus != lastMicroBunchStatus_[linkID])
				{
					TGraph* g = g_ubStatusVsEwt_[linkID];
					g->SetPoint(g->GetN(), static_cast<double>(ewt),
					            static_cast<double>(ubStatus));
					lastMicroBunchStatus_[linkID] = ubStatus;

					if(diagLevel_ > 1)
					{
						std::cout << outputPrefix_
						          << "MicroBunchStatus changed on link "
						          << (int)linkID << ": 0x" << std::hex
						          << ubStatus << std::dec
						          << " at EWT " << ewt << std::endl;
					}
				}
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
		          << "gSystem->ProcessEvents=" << (statProcEvents_ / dt) << " Hz, "
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
			for(auto& [linkID, g] : g_ubStatusVsEwt_)
			{
				std::cout << outputPrefix_ << "MicroBunchStatus link " << (int)linkID
				          << ": " << g->GetN() << " status changes recorded"
				          << std::endl;
			}
		}
		std::cout << outputPrefix_
		          << "===============================================" << std::endl;
	}

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Ending job" << std::endl;
	}

	// Write TGraphs to the output ROOT file (TGraph doesn't auto-associate
	// with a TDirectory like TH1 does, so make<TGraph>() alone won't persist them)
	if(!dummyHist_)
	{
		art::TFileDirectory dir = tfs_->mkdir(outputTag_);
		dir.makeAndRegister<TGraph>(
		    g_digisVsEwt_->GetName(), g_digisVsEwt_->GetTitle(),
		    g_digisVsEwt_->GetN(), g_digisVsEwt_->GetX(), g_digisVsEwt_->GetY());
		dir.makeAndRegister<TGraph>(
		    g_digisAvgVsEwt_->GetName(), g_digisAvgVsEwt_->GetTitle(),
		    g_digisAvgVsEwt_->GetN(), g_digisAvgVsEwt_->GetX(),
		    g_digisAvgVsEwt_->GetY());
		for(auto& [linkID, g] : g_ubStatusVsEwt_)
			dir.makeAndRegister<TGraph>(
			    g->GetName(), g->GetTitle(), g->GetN(), g->GetX(), g->GetY());
	}

	// Send final histograms & clean up
	if(sendHists_ && histoSender_ != nullptr)
	{
		Send();
		histoSender_.reset();
	}

	std::vector<TCanvas*> canvasesForPdf;
	if(enableHttpServer_ && webCanvas_ != nullptr)
	{
		canvasesForPdf.push_back(webCanvas_);
	}

	if(enableHttpServer_)
	{
		updateWebDisplay(true);
	}

	// Create summary canvases: one per FEB showing FPGA-pair dt histograms
	if(!dummyHist_)
	{
		std::set<uint8_t> febsWithTiming;
		for(auto& [key, h] : h1_dtFpgaPairs_)
			febsWithTiming.insert(key.first);

		art::TFileDirectory canvasDir = tfs_->mkdir(outputTag_).mkdir("timing_feb_canvases");

		for(uint8_t febId : febsWithTiming)
		{
			std::string cName = Form("c_timing_feb%02d", febId);
			std::string cTitle = Form("FPGA timing FEB %02d", febId);
			TCanvas* c =
			    canvasDir.make<TCanvas>(cName.c_str(), cTitle.c_str(), 1200, 1200);
			TDirectory* saveDir = gDirectory;
			c->Divide(4, 4);

			for(uint8_t fpgaA = 0; fpgaA < 4; ++fpgaA)
			{
				for(uint8_t fpgaB = fpgaA; fpgaB < 4; ++fpgaB)
				{
					if(!showSameFpgaTimingInCanvas_ && fpgaA == fpgaB)
						continue;
					int pad = fpgaA * 4 + fpgaB + 1;
					uint8_t pairCode = fpgaA * 4 + fpgaB;
					auto key = std::make_pair(febId, pairCode);
					auto it = h1_dtFpgaPairs_.find(key);
					if(it != h1_dtFpgaPairs_.end())
					{
						c->cd(pad);
						it->second->Draw("HIST");
					}
				}
			}
			c->Update();
			saveDir->cd();
			c->Write();
			canvasesForPdf.push_back(c);
		}
	}

	if(saveCanvasesToPdf_)
	{
		if(canvasesForPdf.empty())
		{
			std::cout << outputPrefix_
			          << "No canvases available for PDF export (requested file: "
			          << canvasPdfFile_ << ")" << std::endl;
		}
		else
		{
			canvasesForPdf.front()->Print((canvasPdfFile_ + "[").c_str());
			for(TCanvas* c : canvasesForPdf)
			{
				if(c == nullptr)
					continue;
				c->Modified();
				c->Update();
				c->Print(canvasPdfFile_.c_str());
			}
			canvasesForPdf.back()->Print((canvasPdfFile_ + "]").c_str());
			std::cout << outputPrefix_ << "Saved " << canvasesForPdf.size()
			          << " canvases to PDF: " << canvasPdfFile_ << std::endl;
		}
	}

	if(enableHttpServer_)
	{
		stopHttpServer();
	}
}

DEFINE_ART_MODULE(ots::CrvDQM)
}  // namespace ots
