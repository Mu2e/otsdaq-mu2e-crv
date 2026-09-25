// DQM and viewer for the CRV
// Sends histograms to otsdaq visualizer and standalone THttpServer
// Sam Grant, Simon Corrodi
//
// Histogram booking/filling is owned by mu2e::CRVDigiDQM (digis) and
// mu2e::CRVStatusDQM (per-link status, under status/) in Offline/DQMHelpers.
// This module keeps I/O, HistoSender, THttpServer, styling, and PDF export.

// C++ includes
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// art includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"

// art/root includes
#include "art_root_io/TFileDirectory.h"
#include "art_root_io/TFileService.h"

// ROOT includes
#include <TBox.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TDirectory.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <THttpServer.h>
#include <TPad.h>
#include <TPaveStats.h>
#include <TRandom3.h>
#include <TStyle.h>
#include <TSystem.h>

// OTS includes
#include "otsdaq-mu2e/ArtModules/HistoSender.hh"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

// Offline includes
#include "Offline/DQMHelpers/inc/CRVDigiDQM.hh"
#include "Offline/DQMHelpers/inc/CRVStatusDQM.hh"
#include "Offline/DQMHelpers/inc/DQMSegmentationConfig.hh"
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"
#include "Offline/RecoDataProducts/inc/CrvStatus.hh"

// Custom styling
#include "otsdaq-mu2e-crv/ArtModules/CrvDQMStyle.hh"

namespace
{
// What the online display shows, declared once.
struct HistPad
{
	const char* name;  // as mu2e::CRVDigiDQM books it
	int         pad;   // 1-based canvas pad
	const char* drawOpt;
	bool        logx;
	bool        logy;
	bool        autoRange;  // rescale the Y axis from the data on every refresh
	double      yFloor;     // lower edge, used for both SetMinimum and autoRange
};

constexpr HistPad kHistPads[] = {
    {"h1_digisPerEvt", 2, "HIST", true, true, true, 0.5},
    {"h1_peakAdc", 3, "HIST", false, false, false, 0.0},
    {"h1_tdc", 4, "HIST", false, false, false, 0.0},
    {"h1_channels", 5, "HIST", false, false, true, 0.5},
    {"h2_channels", 6, "COLZ", false, false, false, 0.0},
};

// The two EWT graphs keep their own pads: their axis handling is bespoke
// (a sliding X window tied to the event window tag), not table-driven.
constexpr int kPadDigisVsEwt    = 1;
constexpr int kPadDigisAvgVsEwt = 7;

constexpr int kCanvasCols = 3;
constexpr int kCanvasRows = 3;
static_assert(kCanvasCols * kCanvasRows >= kPadDigisAvgVsEwt,
              "canvas division is too small for the declared pads");

// Workaround for ROOT fatal "TPad::Range: y1 == y2 == 0" on empty histograms drawn
// in the web canvas. Put a tiny entry into bin 1 so max_bin_content > 0; it is
// overwritten as soon as real data arrives.
void seedEmptyFrame(TH1* h)
{
	if(!h)
		return;
	if(h->GetMaximum() <= h->GetMinimum())
	{
		h->SetBinContent(1, 1e-9);
		h->SetEntries(0);
	}
}

// Gray-out port-0 regions (FEB 0 should never have hits): ROC 1 globalChannelId
// 0-63 and ROC 2 1600-1663 on the channel axis, globalFebId 0 and 25 on the FEB axis.
// Segment rotations rebuild window copies, so this is idempotent and re-applied.
void addPort0Boxes(TH1* h)
{
	if(!h || h->GetListOfFunctions()->FindObject("TBox"))
		return;
	auto addBox = [h](double x1, double y1, double x2, double y2) {
		auto* box = new TBox(x1, y1, x2, y2);
		box->SetFillColor(kGray);
		box->SetFillStyle(3004);
		box->SetLineWidth(0);
		h->GetListOfFunctions()->Add(box, "");
	};
	if(h->GetDimension() == 1)
	{
		addBox(-0.5, 0.0, 63.5, 1e9);
		addBox(1599.5, 0.0, 1663.5, 1e9);
	}
	else
	{
		for(int febId : {0, 25})
			addBox(0.5, febId - 0.5, 64.5, febId + 0.5);
	}
}

}  // namespace

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
	static mu2e::CRVDigiDQM::Config   makeHelperConfig(fhicl::ParameterSet const& ps);
	static mu2e::CRVStatusDQM::Config makeStatusConfig(fhicl::ParameterSet const& ps);

	// Standard art methods
	void analyze(art::Event const& event) override;
	void beginJob() override;
	void beginRun(art::Run const& run) override;
	void beginSubRun(art::SubRun const& subRun) override;
	void endSubRun(art::SubRun const& subRun) override;
	void endJob() override;

	/// Module methods
	void Send();
	// What `publish` selected, printed once so an operator can see whether a
	// FHiCL change did what they meant. The selection is run-time now, so it
	// cannot be read off the source.
	void logPublished();
	// The job copy of a booked histogram, or nullptr if the FHiCL disabled it.
	TH1* jobCopy(const char* name);
	// Style and register with the HTTP server any status object the helper
	// booked since the last call (per-link objects appear on a link's first status).
	void registerNewStatusObjects();
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
	bool          statusGraphs_;
	std::string   canvasPdfFile_;

	// HISTOGRAM SENDING
	std::unique_ptr<HistoSender> histoSender_;
	float                        sendIntervalSec_;

	// ROOT TFileService
	art::ServiceHandle<art::TFileService> tfs_;

	// Digi DQM histograms (occupancy, ADC, TDC, CF timing, EWT graphs)
	mu2e::CRVDigiDQM dqm_;
	// Per-link status summaries and graphs (status/, status/graphs/)
	mu2e::CRVStatusDQM statusDqm_;
	std::set<TObject*> registeredStatus_;

	// Dummy histogram for HistoSender/THttpServer plumbing tests
	TH1F* h1_dummy_;

	// HTTP server & visualisation
	bool                                               enableHttpServer_;
	int                                                httpPort_;
	float                                              onlineRefreshPeriodMs_;
	std::string                                        histColor_;
	std::string                                        canvasName_;
	TCanvas*                                           webCanvas_;
	THttpServer*                                       httpServer_;
	std::chrono::time_point<std::chrono::steady_clock> lastRefreshTime_;

	// Event counter for display refresh (includes dummyHist events)
	std::size_t eventCounts_{0};

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

	// Misc member variables
	std::chrono::time_point<std::chrono::steady_clock> lastSendTime_;
	std::string                                        outputPrefix_;
	TRandom3                                           random_;
};

mu2e::CRVDigiDQM::Config CrvDQM::makeHelperConfig(fhicl::ParameterSet const& ps)
{
	mu2e::CRVDigiDQM::Config c;
	c.nBinsDigisPerEvt = ps.get<int>("nBinsDigisPerEvt", 200);
	c.maxDigisPerEvt   = ps.get<float>("maxDigisPerEvt", 4000);
	c.nBinsPeakAdc     = ps.get<int>("nBinsPeakAdc", 450);
	c.maxPeakAdc       = ps.get<float>("maxPeakAdc", 4500);
	c.nBinsTdc         = ps.get<int>("nBinsTdc", 400);
	c.maxTdc           = ps.get<float>("maxTdc", 40000);
	c.cfFraction       = ps.get<double>("cfFraction", 0.20);
	c.dtBinSize        = ps.get<float>("dtBinSize", 0.5);
	c.dtRange          = ps.get<float>("dtRange", 100.0);
	c.dtVsFebBinSize   = ps.get<float>("dtVsFebBinSize", 2.0);
	c.dtVsFebRange     = ps.get<float>("dtVsFebRange", 500.0);
	c.minAmplitude     = ps.get<int>("minAmplitude", 10);
	c.avgBlockSize     = static_cast<std::size_t>(ps.get<int>("avgBlockSize", 30));
	c.avgGraphPoints   = static_cast<std::size_t>(ps.get<int>("avgGraphPoints", 1000));
	c.channelsWindowEwts =
	    static_cast<std::size_t>(ps.get<int>("channelsWindowEwts", 50000));
	c.fillInclusive  = false;
	c.fillCrvIdRates = ps.get<bool>("fillCrvIdRates", true);
	c.kppReadout     = ps.get<bool>("kppReadout", true);
	c.fillLivePlots  = ps.get<bool>("fillLivePlots", true);
	// Which histograms get per-subrun / last-N-events copies. Parsed by the
	// same code the offline analyzers use, so the grammar cannot drift.
	c.segmentation =
	    mu2e::parseSegmentation(ps.get<fhicl::ParameterSet>("segmentation", {}));
	return c;
}

mu2e::CRVStatusDQM::Config CrvDQM::makeStatusConfig(fhicl::ParameterSet const& ps)
{
	mu2e::CRVStatusDQM::Config c;
	c.fillLinkPlots  = true;
	c.fillLinkGraphs = ps.get<bool>("statusGraphs", true);
	c.maxLinkGraphPoints =
	    static_cast<std::size_t>(ps.get<int>("maxLatencyGraphPoints", 10000));
	// Publishing of the status/ histograms, same grammar as `segmentation`.
	c.segmentation =
	    mu2e::parseSegmentation(ps.get<fhicl::ParameterSet>("statusSegmentation", {}));
	return c;
}

// Constructor impl
CrvDQM::CrvDQM(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , crvDigiTag_(ps.get<std::string>("crvDigiTag", "CrvDigi"))
    , crvStatusTag_(ps.get<std::string>("crvStatusTag", crvDigiTag_.label()))
    , diagLevel_(ps.get<int>("diagLevel", 3))
    , port_(ps.get<int>("port", 6000))
    , address_(ps.get<std::string>("address", "localhost"))
    , outputTag_(ps.get<std::string>("outputTag", "CrvDQM"))
    , sendHists_(ps.get<bool>("sendHists", true))
    , dummyHist_(ps.get<bool>("dummyHist", false))
    , saveCanvasesToPdf_(ps.get<bool>("saveCanvasesToPdf", false))
    , showSameFpgaTimingInCanvas_(ps.get<bool>("showSameFpgaTimingInCanvas", true))
    , statusGraphs_(ps.get<bool>("statusGraphs", true))
    , canvasPdfFile_(ps.get<std::string>("canvasPdfFile", "CrvDQM.pdf"))
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5))
    , dqm_(makeHelperConfig(ps))
    , statusDqm_(makeStatusConfig(ps))
    , h1_dummy_(nullptr)
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

void CrvDQM::beginSubRun(art::SubRun const& subRun)
{
	if(dummyHist_)
		return;
	const int run    = static_cast<int>(subRun.run());
	const int subrun = static_cast<int>(subRun.subRun());
	dqm_.BeginSubRun(run, subrun);
	statusDqm_.BeginSubRun(run, subrun);
}

void CrvDQM::endSubRun(art::SubRun const& subRun)
{
	if(dummyHist_)
		return;
	dqm_.EndSubRun();
	statusDqm_.EndSubRun(static_cast<int>(subRun.run()),
	                     static_cast<int>(subRun.subRun()));
}

void CrvDQM::beginRun(art::Run const& run)
{
	// The DQM art process can outlive a run, and everything here is streamed in replace
	// mode without ever being cleared, so a new run would otherwise inherit the previous
	// run's contents. Objects stay booked (and registered with the HTTP server); only
	// their contents and the rolling windows behind them are cleared.
	std::cout << outputPrefix_ << "Run " << run.run()
	          << ": resetting histograms, graphs, and rolling windows" << std::endl;

	if(dummyHist_)
	{
		if(h1_dummy_)
			h1_dummy_->Reset("ICES");
		return;
	}

	dqm_.ResetForNewRun();
	statusDqm_.ResetForNewRun();

	for(const char* name : {"h1_channels", "h2_channels"})
		for(TH1* h : dqm_.segments().copies(name))
			addPort0Boxes(h);

	// The web canvas draws these directly; keep them drawable while empty.
	if(enableHttpServer_ && webCanvas_)
	{
		for(const auto& spec : kHistPads)
			seedEmptyFrame(jobCopy(spec.name));
	}
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

	art::TFileDirectory dir = tfs_->mkdir(outputTag_);
	if(dummyHist_)
	{
		h1_dummy_ = dir.make<TH1F>("h1_dummy", "Dummy Gaussian", 200, -100, 100);
	}
	else
	{
		dqm_.Book(dir);
		for(const char* name : {"h1_channels", "h2_channels"})
			for(TH1* h : dqm_.segments().copies(name))
				addPort0Boxes(h);
		statusDqm_.Book(tfs_->mkdir(outputTag_ + "/status"));
		CrvDQMStyle::FormatGraph(dqm_.g_digisVsEwt(), histColor_);
		dqm_.g_digisVsEwt()->SetMarkerColor(dqm_.g_digisVsEwt()->GetLineColor());
		dqm_.g_digisVsEwt()->SetDrawOption("AP");
		CrvDQMStyle::FormatGraph(dqm_.g_digisAvgVsEwt(), histColor_);
		dqm_.g_digisAvgVsEwt()->SetMarkerColor(dqm_.g_digisAvgVsEwt()->GetLineColor());
		dqm_.g_digisAvgVsEwt()->SetDrawOption("AP");
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

	// The 2D status summaries exist from booking; per-link objects follow per link.
	registerNewStatusObjects();

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

	// The live segment copies are labelled with the range they hold; refresh
	// that before shipping, so a title read on the GUI is never behind.
	dqm_.segments().RefreshLabels();
	statusDqm_.segments().RefreshLabels();

	// Use the map method (three methods in HistoSender.cc)
	std::map<std::string, std::vector<TH1*>> hists;
	if(dummyHist_)
	{
		hists["crv/h1_gaus:replace"] = {h1_dummy_};
	}
	else
	{
		for(const auto* segments : {&dqm_.segments(), &statusDqm_.segments()})
		{
			for(const auto& [group, copies] : segments->publishedCopies())
			{
				auto& out = hists["crv/" + group + ":replace"];
				out.insert(out.end(), copies.begin(), copies.end());
			}
		}
	}

	// Call send method
	histoSender_->sendHistograms(hists);
	++statSend_;

	// Send graphs
	if(!dummyHist_)
	{
		std::map<std::string, std::vector<TGraph*>> graphs;
		graphs["crv/graphs:replace"] = {dqm_.g_digisVsEwt(), dqm_.g_digisAvgVsEwt()};
		if(statusGraphs_)
		{
			for(TGraph* g : statusDqm_.linkGraphs())
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

void CrvDQM::logPublished()
{
	auto published = dqm_.segments().publishedCopies();
	for(const auto& [group, copies] : statusDqm_.segments().publishedCopies())
	{
		auto& out = published[group];
		out.insert(out.end(), copies.begin(), copies.end());
	}
	std::cout << outputPrefix_ << "publishing " << published.size()
	          << " HistoSender group(s):" << std::endl;
	for(const auto& [group, copies] : published)
	{
		std::cout << outputPrefix_ << "  crv/" << group << ":replace  (" << copies.size()
		          << ")";
		// Naming every member of a 300-histogram group helps nobody.
		if(copies.size() <= 8)
		{
			for(TH1* h : copies)
				std::cout << " " << h->GetName();
		}
		std::cout << std::endl;
	}
}

TH1* CrvDQM::jobCopy(const char* name)
{
	const auto copies = dqm_.segments().copies(name);
	return copies.empty() ? nullptr : copies.front();
}

void CrvDQM::registerNewStatusObjects()
{
	if(dummyHist_)
		return;
	for(TH1* h : statusDqm_.segments().allCopies())
	{
		if(!registeredStatus_.insert(h).second)
			continue;
		if(auto* h2 = dynamic_cast<TH2*>(h))
		{
			CrvDQMStyle::FormatHist2D(h2);
			h2->SetOption("COLZ");
		}
		else
		{
			CrvDQMStyle::FormatHist(h, histColor_);
			if(std::string(h->GetName()).rfind("h1_latency", 0) == 0)
			{
				h->SetStatOverflows(TH1::kConsider);
				h->SetStats(1);
			}
		}
		if(enableHttpServer_ && httpServer_)
			httpServer_->Register("/", h);
	}
	for(TGraph* g : statusDqm_.linkGraphs())
	{
		if(!registeredStatus_.insert(g).second)
			continue;
		CrvDQMStyle::FormatGraph(g, histColor_);
		g->SetMarkerColor(g->GetLineColor());
		g->SetDrawOption("AP");
		if(enableHttpServer_ && httpServer_)
			httpServer_->Register("/", g);
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
		webCanvas_->Divide(kCanvasCols, kCanvasRows);
	}

	int padIdx = 1;
	// Keep empty histograms drawable (see seedEmptyFrame); also re-applied in beginRun.
	auto seedFrame = &seedEmptyFrame;

	if(dummyHist_)
	{
		webCanvas_->cd(padIdx);
		CrvDQMStyle::FormatHist(h1_dummy_, histColor_);
		seedFrame(h1_dummy_);
		h1_dummy_->Draw("HIST");
	}
	else
	{
		TGraph* g_digisVsEwt    = dqm_.g_digisVsEwt();
		TGraph* g_digisAvgVsEwt = dqm_.g_digisAvgVsEwt();

		// The two EWT graphs: bespoke axis handling, so they stay explicit.
		auto drawGraph = [&](TGraph* g, int pad) {
			webCanvas_->cd(pad);
			CrvDQMStyle::FormatGraph(g, histColor_);
			if(TH1F* frame = g->GetHistogram())
			{
				frame->GetXaxis()->SetLimits(0.0, 1.0);
				frame->SetMinimum(0.0);
				frame->SetMaximum(1.0);
			}
			g->Draw("AP");
		};
		drawGraph(g_digisVsEwt, kPadDigisVsEwt);
		drawGraph(g_digisAvgVsEwt, kPadDigisAvgVsEwt);

		// Everything else comes off the table. The job copy is what the canvas
		// draws; the segment copies are registered and restyled below but are
		// not given pads of their own.
		for(const auto& spec : kHistPads)
		{
			TH1* h = jobCopy(spec.name);
			if(h == nullptr)
				continue;
			webCanvas_->cd(spec.pad);
			if(spec.logx)
				gPad->SetLogx();
			if(spec.logy)
				gPad->SetLogy();
			if(auto* h2 = dynamic_cast<TH2*>(h))
			{
				// Colour maps need room for the palette and a Z title.
				gPad->SetRightMargin(0.14);
				CrvDQMStyle::FormatHist2D(h2);
				h2->GetZaxis()->SetTitle("Hits");
				gStyle->SetPalette(kInvertedDarkBodyRadiator);
			}
			else
			{
				CrvDQMStyle::FormatHist(h, histColor_);
				if(spec.yFloor > 0.0)
					h->SetMinimum(spec.yFloor);
			}
			seedFrame(h);
			h->Draw(spec.drawOpt);

			// One genuine special: the occupancy axis is wide enough that ROOT
			// drops the stat box styling, so re-apply it once the pad is drawn.
			if(std::string(spec.name) == "h1_channels")
			{
				gPad->Update();
				if(auto* st = dynamic_cast<TPaveStats*>(h->FindObject("stats")))
				{
					st->SetBorderSize(0);
					st->SetFillStyle(0);
					st->SetTextFont(42);
					st->SetTextSize(0.040);
					st->SetOptStat(111110);
				}
			}
		}
	}

	// Register canvas and histograms with server
	httpServer_->Register("/", webCanvas_);
	if(!dummyHist_)
	{
		// Every copy of every displayed histogram, so a configured window or
		// subrun copy is reachable on the server without a change here.
		for(const auto& spec : kHistPads)
		{
			for(TH1* h : dqm_.segments().copies(spec.name))
			{
				httpServer_->Register("/", h);
			}
		}
		httpServer_->Register("/", dqm_.g_digisVsEwt());
		httpServer_->Register("/", dqm_.g_digisAvgVsEwt());
	}

	// Publish refresh period so the HTML page can read it
	httpServer_->CreateItem("/config/refreshMs", Form("%.0f", onlineRefreshPeriodMs_));

	// Setup custom page. OTS_SOURCE is only set inside the otsdaq environment;
	// a file-mode dry run has it unset, and std::string(nullptr) is undefined
	// behaviour, so fall back to the built-in page rather than crashing.
	if(const char* otsSource = getenv("OTS_SOURCE"))
	{
		httpServer_->SetDefaultPage(std::string(otsSource) +
		                            "/otsdaq-mu2e-crv/UserWebGUI/html/CrvDQM.html");
	}
	else
	{
		std::cout << outputPrefix_
		          << "OTS_SOURCE is not set; serving the default THttpServer page"
		          << std::endl;
	}

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

	// The live segment copies carry the range they hold in their titles, and
	// the canvas draws those titles; bring them up to date before redrawing.
	dqm_.segments().RefreshLabels();
	statusDqm_.segments().RefreshLabels();

	++statUpdate_;

	if(dummyHist_ && h1_dummy_)
	{
		double maxContent = h1_dummy_->GetBinContent(h1_dummy_->GetMaximumBin());
		h1_dummy_->GetYaxis()->SetRangeUser(0.0, std::max(1.0, 1.15 * maxContent));
	}
	else
	{
		// Every copy of every histogram the table marks autoRange -- so the
		// rolling window copy is rescaled with its parent, whatever it is
		// called and however many older spans are configured.
		for(const auto& spec : kHistPads)
		{
			if(!spec.autoRange)
				continue;
			for(TH1* h : dqm_.segments().copies(spec.name))
			{
				const double maxContent = h->GetBinContent(h->GetMaximumBin());
				h->GetYaxis()->SetRangeUser(spec.yFloor,
				                            std::max(1.0, 1.15 * maxContent));
			}
		}
	}

	// Re-apply palette right before update: global TColor state is fragile
	gStyle->SetPalette(kInvertedDarkBodyRadiator);

	// Re-apply per-object formatting that ROOT loses when internal
	// structures are recreated (e.g. TGraph histogram after SetPoint/RemovePoint)
	if(!dummyHist_)
	{
		TGraph* g_digisVsEwt    = dqm_.g_digisVsEwt();
		TGraph* g_digisAvgVsEwt = dqm_.g_digisAvgVsEwt();

		for(const auto& spec : kHistPads)
		{
			for(TH1* h : dqm_.segments().copies(spec.name))
			{
				if(auto* h2 = dynamic_cast<TH2*>(h))
					CrvDQMStyle::FormatHist2D(h2);
				else
					CrvDQMStyle::FormatHist(h, histColor_);
				if(spec.name == std::string("h1_channels") ||
				   spec.name == std::string("h2_channels"))
					addPort0Boxes(h);
			}
		}
		CrvDQMStyle::FormatGraph(g_digisVsEwt, histColor_);

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
		if(dqm_.hasEwtWindow())
		{
			autoRangeGraphY(g_digisVsEwt);
			// autoRangeGraphY may trigger TGraph::GetHistogram() to recreate the
			// frame, which defaults X limits to the data range and breaks the
			// sliding window set in Fill(). Re-apply the sliding window here.
			double currentEwt = static_cast<double>(dqm_.lastEwt());
			double xLo        = std::max(0.0, currentEwt - mu2e::CRVDigiDQM::kEwtXRange);
			double xHi        = currentEwt;
			if(xHi <= xLo)
				xHi = xLo + 1.0;
			if(TH1F* frame = g_digisVsEwt->GetHistogram())
				frame->GetXaxis()->SetLimits(xLo, xHi);
		}
		autoRangeGraphY(g_digisAvgVsEwt);
		// Same fix for the averaged graph: span the points it currently holds.
		if(g_digisAvgVsEwt->GetN() > 0)
		{
			double* ax   = g_digisAvgVsEwt->GetX();
			int     nAvg = g_digisAvgVsEwt->GetN();
			double  aLo  = *std::min_element(ax, ax + nAvg);
			double  aHi  = *std::max_element(ax, ax + nAvg);
			if(aHi <= aLo)
				aHi = aLo + 1.0;
			if(TH1F* frame = g_digisAvgVsEwt->GetHistogram())
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
		art::Handle<mu2e::CrvDigiCollection> crvDigisHandle;
		event.getByLabel(crvDigiTag_, crvDigisHandle);

		const mu2e::CrvDigiCollection  emptyDigis;
		const mu2e::CrvDigiCollection& crvDigis =
		    (crvDigisHandle.isValid() && crvDigisHandle.product() != nullptr)
		        ? *crvDigisHandle
		        : emptyDigis;

		if(!crvDigisHandle.isValid() || crvDigis.empty())
		{
			if(diagLevel_ > 1)
			{
				std::cout << outputPrefix_ << "Warning! No CRV digis found" << std::endl;
			}
		}
		else if(diagLevel_ > 1)
		{
			std::cout << outputPrefix_ << "Found " << crvDigis.size() << std::endl;
		}

		art::Handle<mu2e::CrvStatusCollection> crvStatusHandle;
		event.getByLabel(crvStatusTag_, crvStatusHandle);
		const mu2e::CrvStatusCollection  emptyStatus;
		const mu2e::CrvStatusCollection& crvStatus =
		    (crvStatusHandle.isValid() && crvStatusHandle.product() != nullptr)
		        ? *crvStatusHandle
		        : emptyStatus;

		dqm_.Fill(crvDigis, crvStatus);
		statusDqm_.Fill(crvStatus);
		registerNewStatusObjects();
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
		std::cout << outputPrefix_
		          << "Total events: " << (dummyHist_ ? eventCounts_ : dqm_.nEvents())
		          << std::endl;
		if(!dummyHist_)
		{
			std::cout << outputPrefix_ << "Total digis: " << dqm_.nDigis() << std::endl;
			std::cout << outputPrefix_ << "Active FEBs: " << dqm_.activeFEBs().size()
			          << std::endl;
			// Print FEBs per ROC
			for(auto& [roc, febs] : dqm_.rocFEBMap())
			{
				std::cout << outputPrefix_ << "ROC " << (int)roc << " has " << febs.size()
				          << " FEBs: ";
				for(auto feb : febs)
				{
					std::cout << (int)feb << " ";
				}
				std::cout << std::endl;
			}
			for(TGraph* g : statusDqm_.linkGraphs())
			{
				std::cout << outputPrefix_ << g->GetName() << ": " << g->GetN()
				          << " points recorded" << std::endl;
			}
		}
		std::cout << outputPrefix_
		          << "===============================================" << std::endl;
	}

	if(diagLevel_ > 1)
	{
		std::cout << outputPrefix_ << "Ending job" << std::endl;
	}

	if(!dummyHist_)
	{
		dqm_.WriteGraphs();
		statusDqm_.WriteGraphs();
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
		std::set<int> febsWithTiming;
		for(auto& [key, h] : dqm_.dtFpgaPairs())
			febsWithTiming.insert(key.first);

		art::TFileDirectory canvasDir =
		    tfs_->mkdir(outputTag_).mkdir("timing_feb_canvases");

		for(int febId : febsWithTiming)
		{
			std::string cName  = Form("c_timing_feb%02d", febId);
			std::string cTitle = Form("FPGA timing FEB %02d", febId);
			TCanvas*    c =
			    canvasDir.make<TCanvas>(cName.c_str(), cTitle.c_str(), 1200, 1200);
			TDirectory* saveDir = gDirectory;
			c->Divide(4, 4);

			for(uint8_t fpgaA = 0; fpgaA < 4; ++fpgaA)
			{
				for(uint8_t fpgaB = fpgaA; fpgaB < 4; ++fpgaB)
				{
					if(!showSameFpgaTimingInCanvas_ && fpgaA == fpgaB)
						continue;
					int     pad      = fpgaA * 4 + fpgaB + 1;
					uint8_t pairCode = fpgaA * 4 + fpgaB;
					auto    key      = std::make_pair(febId, pairCode);
					auto    it       = dqm_.dtFpgaPairs().find(key);
					if(it != dqm_.dtFpgaPairs().end())
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

	if(diagLevel_ > 0)
	{
		logPublished();
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
