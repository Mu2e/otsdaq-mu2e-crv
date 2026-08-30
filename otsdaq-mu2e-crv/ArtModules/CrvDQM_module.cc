// DQM and viewer for the CRV
// Sends histograms to otsdaq visualizer and standalone THttpServer
// Sam Grant, Simon Corrodi
//
// Digi histogram booking/filling is owned by mu2e::CRVDigiDQM
// (Offline/DQMHelpers). This module keeps I/O, HistoSender, THttpServer,
// styling, and PDF export.

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

// art/root includes
#include "art_root_io/TFileDirectory.h"
#include "art_root_io/TFileService.h"

// ROOT includes
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
#include "Offline/RecoDataProducts/inc/CrvDigi.hh"
#include "Offline/RecoDataProducts/inc/CrvStatus.hh"

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
	static mu2e::CRVDigiDQM::Config makeHelperConfig(fhicl::ParameterSet const& ps);

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

	// HISTOGRAM SENDING
	std::unique_ptr<HistoSender> histoSender_;
	float                        sendIntervalSec_;

	// ROOT TFileService
	art::ServiceHandle<art::TFileService> tfs_;

	// Digi DQM histograms (occupancy, ADC, TDC, CF timing, EWT graphs)
	mu2e::CRVDigiDQM dqm_;

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
	c.nBinsDigisPerEvt   = ps.get<int>("nBinsDigisPerEvt", 200);
	c.maxDigisPerEvt     = ps.get<float>("maxDigisPerEvt", 4000);
	c.nBinsPeakAdc       = ps.get<int>("nBinsPeakAdc", 450);
	c.maxPeakAdc         = ps.get<float>("maxPeakAdc", 4500);
	c.nBinsTdc           = ps.get<int>("nBinsTdc", 400);
	c.maxTdc             = ps.get<float>("maxTdc", 40000);
	c.cfFraction         = ps.get<double>("cfFraction", 0.20);
	c.dtBinSize          = ps.get<float>("dtBinSize", 0.5);
	c.dtRange            = ps.get<float>("dtRange", 100.0);
	c.dtVsFebBinSize     = ps.get<float>("dtVsFebBinSize", 2.0);
	c.dtVsFebRange       = ps.get<float>("dtVsFebRange", 500.0);
	c.minAmplitude       = ps.get<int>("minAmplitude", 10);
	c.avgBlockSize       = static_cast<std::size_t>(ps.get<int>("avgBlockSize", 30));
	c.avgGraphPoints     = static_cast<std::size_t>(ps.get<int>("avgGraphPoints", 1000));
	c.channelsWindowEwts =
	    static_cast<std::size_t>(ps.get<int>("channelsWindowEwts", 50000));
	c.fillInclusive      = false;
	c.fillCrvIdRates     = ps.get<bool>("fillCrvIdRates", true);
	c.kppReadout         = ps.get<bool>("kppReadout", true);
	return c;
}

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
    , sendIntervalSec_(ps.get<float>("sendIntervalSec", 0.5))
    , dqm_(makeHelperConfig(ps))
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
		hists["crv/h1_channels:replace"]        = {dqm_.h1_channels()};
		hists["crv/h1_channelsLastEwt:replace"] = {dqm_.h1_channelsLastEwt()};
		hists["crv/h2_channels:replace"]        = {dqm_.h2_channels()};
		hists["crv/h1_digisPerEvt:replace"]     = {dqm_.h1_digisPerEvt()};
		hists["crv/h1_peakAdc:replace"]         = {dqm_.h1_peakAdc()};
		hists["crv/h1_tdc:replace"]             = {dqm_.h1_tdc()};

		if(TH2F* h = dqm_.dtVsFeb())
		{
			hists["crv/timing_feb:replace"].push_back(h);
		}
		if(TH1F* h = dqm_.dtOutOfRangePerFeb())
		{
			hists["crv/timing_feb:replace"].push_back(h);
		}
		if(TH1F* h = dqm_.dtOutOfRangePerFebLastEwt())
		{
			hists["crv/timing_feb:replace"].push_back(h);
		}

		for(const auto& [key, h] : dqm_.dtFpgaPairs())
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
		graphs["crv/graphs:replace"] = {dqm_.g_digisVsEwt(), dqm_.g_digisAvgVsEwt()};
		for(auto& [linkID, g] : dqm_.ubStatusVsEwt())
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
		TH1F*   h1_digisPerEvt     = dqm_.h1_digisPerEvt();
		TH1F*   h1_peakAdc         = dqm_.h1_peakAdc();
		TH1F*   h1_tdc             = dqm_.h1_tdc();
		TH1F*   h1_channels        = dqm_.h1_channels();
		TH2F*   h2_channels        = dqm_.h2_channels();
		TGraph* g_digisVsEwt       = dqm_.g_digisVsEwt();
		TGraph* g_digisAvgVsEwt    = dqm_.g_digisAvgVsEwt();

		// Pad 1: digis vs event window tag (rolling).
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatGraph(g_digisVsEwt, histColor_);
		if(TH1F* frame = g_digisVsEwt->GetHistogram())
		{
			frame->GetXaxis()->SetLimits(0.0, 1.0);
			frame->SetMinimum(0.0);
			frame->SetMaximum(1.0);
		}
		g_digisVsEwt->Draw("AP");

		// Pad 2: digis per event
		webCanvas_->cd(padIdx++);
		gPad->SetLogx();
		gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_digisPerEvt, histColor_);
		h1_digisPerEvt->SetMinimum(0.5);
		seedFrame(h1_digisPerEvt);
		h1_digisPerEvt->Draw("HIST");

		// Pad 3: peak ADC
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatHist(h1_peakAdc, histColor_);
		seedFrame(h1_peakAdc);
		h1_peakAdc->Draw("HIST");

		// Pad 4: TDC
		webCanvas_->cd(padIdx++);
		CrvDQMStyle::FormatHist(h1_tdc, histColor_);
		seedFrame(h1_tdc);
		h1_tdc->Draw("HIST");

		// Pad 5: global channel occupancy
		webCanvas_->cd(padIdx++);
		// gPad->SetLogy();
		CrvDQMStyle::FormatHist(h1_channels, histColor_);
		h1_channels->SetMinimum(0.5);
		seedFrame(h1_channels);
		h1_channels->Draw("HIST");
		gPad->Update();
		// Force stat box styling. Workaround for large-bin histogram
		TPaveStats* st = dynamic_cast<TPaveStats*>(h1_channels->FindObject("stats"));
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
		if(h2_channels)
		{
			CrvDQMStyle::FormatHist2D(h2_channels);
			h2_channels->GetZaxis()->SetTitle("Hits");
			seedFrame(h2_channels);
			gStyle->SetPalette(kInvertedDarkBodyRadiator);
			h2_channels->Draw("COLZ");
		}

		// Pad 7: block-averaged hits per event (points only, no connecting line)
		webCanvas_->cd(padIdx);
		CrvDQMStyle::FormatGraph(g_digisAvgVsEwt, histColor_);
		if(TH1F* frame = g_digisAvgVsEwt->GetHistogram())
		{
			frame->GetXaxis()->SetLimits(0.0, 1.0);
			frame->SetMinimum(0.0);
			frame->SetMaximum(1.0);
		}
		g_digisAvgVsEwt->Draw("AP");
	}

	// Register canvas and histograms with server
	httpServer_->Register("/", webCanvas_);
	if(!dummyHist_)
	{
		httpServer_->Register("/", dqm_.h1_digisPerEvt());
		httpServer_->Register("/", dqm_.h1_peakAdc());
		httpServer_->Register("/", dqm_.h1_tdc());
		httpServer_->Register("/", dqm_.h1_channels());
		httpServer_->Register("/", dqm_.h1_channelsLastEwt());
		httpServer_->Register("/", dqm_.h2_channels());
		httpServer_->Register("/", dqm_.g_digisVsEwt());
		httpServer_->Register("/", dqm_.g_digisAvgVsEwt());
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
		TH1F* h1_digisPerEvt     = dqm_.h1_digisPerEvt();
		TH1F* h1_channels        = dqm_.h1_channels();
		TH1F* h1_channelsLastEwt = dqm_.h1_channelsLastEwt();
		if(h1_digisPerEvt)
		{
			double maxContent =
			    h1_digisPerEvt->GetBinContent(h1_digisPerEvt->GetMaximumBin());
			h1_digisPerEvt->GetYaxis()->SetRangeUser(0.5,
			                                         std::max(1.0, 1.15 * maxContent));
		}
		if(h1_channels)
		{
			double maxContent = h1_channels->GetBinContent(h1_channels->GetMaximumBin());
			h1_channels->GetYaxis()->SetRangeUser(0.5, std::max(1.0, 1.15 * maxContent));
		}
		if(h1_channelsLastEwt)
		{
			double maxContent =
			    h1_channelsLastEwt->GetBinContent(h1_channelsLastEwt->GetMaximumBin());
			h1_channelsLastEwt->GetYaxis()->SetRangeUser(
			    0.5, std::max(1.0, 1.15 * maxContent));
		}
	}

	// Re-apply palette right before update: global TColor state is fragile
	gStyle->SetPalette(kInvertedDarkBodyRadiator);

	// Re-apply per-object formatting that ROOT loses when internal
	// structures are recreated (e.g. TGraph histogram after SetPoint/RemovePoint)
	if(!dummyHist_)
	{
		TH1F*   h1_digisPerEvt     = dqm_.h1_digisPerEvt();
		TH1F*   h1_peakAdc         = dqm_.h1_peakAdc();
		TH1F*   h1_tdc             = dqm_.h1_tdc();
		TH1F*   h1_channels        = dqm_.h1_channels();
		TH1F*   h1_channelsLastEwt = dqm_.h1_channelsLastEwt();
		TH2F*   h2_channels        = dqm_.h2_channels();
		TGraph* g_digisVsEwt       = dqm_.g_digisVsEwt();
		TGraph* g_digisAvgVsEwt    = dqm_.g_digisAvgVsEwt();

		CrvDQMStyle::FormatHist(h1_digisPerEvt, histColor_);
		CrvDQMStyle::FormatHist(h1_peakAdc, histColor_);
		CrvDQMStyle::FormatHist(h1_tdc, histColor_);
		CrvDQMStyle::FormatHist(h1_channels, histColor_);
		CrvDQMStyle::FormatHist(h1_channelsLastEwt, histColor_);
		CrvDQMStyle::FormatHist2D(h2_channels);
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
			double xLo = std::max(0.0, currentEwt - mu2e::CRVDigiDQM::kEwtXRange);
			double xHi = currentEwt;
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

		const mu2e::CrvDigiCollection emptyDigis;
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
		const mu2e::CrvStatusCollection emptyStatus;
		const mu2e::CrvStatusCollection& crvStatus =
		    (crvStatusHandle.isValid() && crvStatusHandle.product() != nullptr)
		        ? *crvStatusHandle
		        : emptyStatus;

		dqm_.Fill(crvDigis, crvStatus);
	}

	///////////////////// Send /////////////////////

	// Send histograms in fixed time intervals
	auto                             currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double> elapsed = currentTime - lastSendTime_;

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
		statAnalyze_       = 0;
		statUpdate_        = 0;
		statUpdateCalls_   = 0;
		statUpdateGateA_   = 0;
		statUpdateGateB_   = 0;
		statProcEvents_    = 0;
		statSend_          = 0;
		statLastLog_       = currentTime;
	}
}

void CrvDQM::endJob()
{
	if(diagLevel_ > 0)
	{
		// Print job-level statistics
		std::cout << outputPrefix_
		          << "================= End job summary =================" << std::endl;
		std::cout << outputPrefix_ << "Total events: "
		          << (dummyHist_ ? eventCounts_ : dqm_.nEvents()) << std::endl;
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
			for(auto& [linkID, g] : dqm_.ubStatusVsEwt())
			{
				std::cout << outputPrefix_ << "MicroBunchStatus link " << (int)linkID
				          << ": " << g->GetN() << " status changes recorded" << std::endl;
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
