// ROOT-based DQM and viewer for the CRV
// Author: Sam Grant, Simon Corrodi 
// Data: Feb 2025

// C++ includes
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <thread>

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

// Custom includes
#include "CrvDQMStyle.hh"

namespace ots
{

// Globals, needed to exit the server thread safely
std::atomic<bool>       g_interrupted(false);
std::condition_variable g_cv;
std::mutex              g_cv_m;

// Signal handler function
void signalHandler(int signum)
{
	std::cout << "Interrupt signal (" << signum << ") received.\n";
	g_interrupted.store(true);
	g_cv.notify_all();  // Wake up any waiting threads

	// Force exit after a short delay if normal shutdown doesn't work
	std::thread([]{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		std::cerr << "\nForced exit after timeout.\n";
		_exit(1); // Emergency forced exit
	}).detach();

}

// Helper function for interruptible sleep
bool interruptibleSleep(std::chrono::seconds duration)
{
	std::unique_lock<std::mutex> lock(g_cv_m);
	return !g_cv.wait_for(lock, duration, [] { return g_interrupted.load(); });
}

class CrvDQM : public art::EDAnalyzer
{
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

	void UpdatePlots();

	// fcl parameters
	int           port_;
	int           diagLevel_;
	float         onlineRefreshPeriod_;
	bool          keepAlive_;
	int           keepAliveDuration_;  // minutes
	std::string   plotCol_;            // "red"/"blue"/"green"
	art::InputTag decoderTag_;         // specify the module producing CRV decoder obects

	// Member variables
	TCanvas*                                           canvas_;
	std::unordered_map<std::string, TH1D*>             hists_;
	std::unordered_map<std::string, TGraph*>           graphs_;
	THttpServer*                                       server_;
	std::chrono::time_point<std::chrono::steady_clock> lastUpdate_;
	std::thread                                        keepAliveThread_;
	std::size_t                                        eventCounter_;
	std::size_t                                        subEventCounter_;
	std::size_t                                        blockCounter_;
	std::size_t                                        packetCounter_;
	float                                              crvClockTick_;
	float                                              rocClockTick_;
	std::mutex                                         rootMutex_; // Protect ROOT operations
};

// Constructor implementation
CrvDQM::CrvDQM(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer(ps)
    , port_(ps.get<int>("port", 8877))
    , diagLevel_(ps.get<int>("diagLevel", 1))
    , onlineRefreshPeriod_(ps.get<float>("onlineRefreshPeriod", 500))  // ms
    , keepAlive_(ps.get<int>("keepAlive", true))
    , keepAliveDuration_(ps.get<int>("keepAliveDuration", 5))  // minutes
    , plotCol_(ps.get<std::string>("plotCol", "blue"))
    , decoderTag_(ps.get<art::InputTag>("decoderTag", "genFrags"))
{
	// Initialise non-fcl member variables
	eventCounter_    = 0;
	subEventCounter_ = 0;
	blockCounter_    = 0;
	packetCounter_   = 0;
	crvClockTick_    = 12.5;  // ns
	rocClockTick_    = 50;    // ns, based on the 20 MHz clock

	// Register signal handler for clean shutdown
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
}

// Destructor implementation
CrvDQM::~CrvDQM()
{
	// Signal interruption to ensure any loops break
	g_interrupted.store(true);
	g_cv.notify_all();

	// Make sure the server thread is stopped
	if(keepAliveThread_.joinable())
	{
		keepAliveThread_.join();
	}
	// Then clean up ROOT objects
	if(server_)
	{
		delete server_;
		server_ = nullptr;
	}
	if(canvas_)
	{
		delete canvas_;
		canvas_ = nullptr;
	}
	for(auto& hist : hists_)
	{
		if(hist.second)
			delete hist.second;
		hist.second = nullptr;
	}
	hists_.clear();
	for(auto& graph : graphs_)
	{
		if(graph.second)
			delete graph.second;
		graph.second = nullptr;
	}
	graphs_.clear();
}

void CrvDQM::beginJob()
{
	// Create HTTP server
	server_ = new THttpServer(Form("http:%d", port_));

	// Set global plot style
	CrvDQMStyle::SetStyle();

	// Create canvas
	std::string canvasName = "WebDisplay";
	canvas_ = new TCanvas(canvasName.c_str(), "CRV web display");
	canvas_->Divide(2, 3);  // 2 columns 3 rows

	// Create histograms
	hists_["numSamples"] = new TH1D("numSamples", ";Samples / block;Entries", 8, 0.5, 8.5);
	hists_["ADC"] = new TH1D("ADC", ";ADC;Entries", 201, -100, 100);  // Raw ADC waveform per hit
	hists_["febChannel"] = new TH1D("febChannel", ";FEB channel;Entries", 64, -0.5, 63.5);  // 64 channels at the moment (single FEB)
	hists_["nHits"] = new TH1D("nHits", ";CRV hits / block;Entries", 61, 0, 60);
	hists_["hitTime"] = new TH1D("hitTime", ";Hit time [ns];Entries", 256, 0, 255 * crvClockTick_);

	// Graphs
	graphs_["latency"] = new TGraph();
	graphs_["latency"]->SetTitle(";Subevent;ROC latency [ns]");

	// Format and draw
	int canvasIdx = 1;
	for(auto& hist : hists_)
	{
		// Get pad
		canvas_->cd(canvasIdx);
		++canvasIdx;
		// Histogram formatting
		CrvDQMStyle::FormatHist(hist.second, plotCol_);
		// Draw
		hist.second->Draw("HIST");
	}

	for(auto& graph : graphs_)
	{
		// Get pad
		canvas_->cd(canvasIdx);
		++canvasIdx;
		// Graph formatting
		CrvDQMStyle::FormatGraph(graph.second, plotCol_);
		// Draw
		graph.second->Draw("APL");
	}

	// Register with server
	server_->Register("/", canvas_);

	// Set item defaults
	server_->SetItemField("/", "_monitoring", Form("%f", onlineRefreshPeriod_));  // Update period
	server_->SetItemField("/", "_browser", "off"); // Turn off sidebar
	server_->SetItemField("/", "_drawitem", canvasName.c_str()); // Set DQM canvas as default item
	server_->SetItemField("/", "_http_cache", "0");  // Disable HTTP caching?

	// Last update variable
	lastUpdate_ = std::chrono::steady_clock::now();

	// Print URL
	printf("Server running on http://localhost:%d/\n", port_);

	// Start an independent thread for server with interruptible sleep
	keepAliveThread_ = std::thread([this]() {
		while(!g_interrupted.load())
		{
			{
				std::lock_guard<std::mutex> lock(rootMutex_);
				gSystem->ProcessEvents();  // Process events with mutex protection
			}

			// Check interrupt flag again
			if(g_interrupted.load()) break;

			// Sleep for a short time, but be able to wake up if interrupted
			std::unique_lock<std::mutex> lock(g_cv_m);
			g_cv.wait_for(lock, std::chrono::milliseconds(50), [] {
				return g_interrupted.load();
			});

			// Check again 
			if(g_interrupted.load()) break;
		}

		std::cout << "Keep-alive thread exiting..." << std::endl;
	});
}

void CrvDQM::UpdatePlots()
{
	// Get time
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> elapsed = currentTime - lastUpdate_;

	if(elapsed.count() >= onlineRefreshPeriod_)
	{
		// // Auto scale histogram y-axis
		for(auto& hist : hists_)
		{
			double maxContent = hist.second->GetBinContent(hist.second->GetMaximumBin());
			hist.second->GetYaxis()->SetRangeUser(0, 1.15 * maxContent);
		}

		for(auto& graph : graphs_)
		{
			// Get the number of points in the graph
			int nPoints = graph.second->GetN();
			// Get the x and y values of the graph
			double* xValues = graph.second->GetX();
			double* yValues = graph.second->GetY();

			// Find the min and max x and y values
			double xMin = *std::min_element(xValues, xValues + nPoints);
			double xMax = *std::max_element(xValues, xValues + nPoints);
			double yMin = *std::min_element(yValues, yValues + nPoints);
			double yMax = *std::max_element(yValues, yValues + nPoints);

			// Optionally add a margin to the y-axis for better visualization
			double yMargin = 0.1 * (yMax - yMin);

			// Set the range for both axes
			graph.second->GetXaxis()->SetRangeUser(xMin, xMax);
			graph.second->GetYaxis()->SetRangeUser(yMin - yMargin, yMax + yMargin);
		}

		// Update the canvas
		canvas_->Modified();
		canvas_->Update();
		gSystem->ProcessEvents();   // Update display
		lastUpdate_ = currentTime;  // Update the time
	}
}

void CrvDQM::analyze(art::Event const& e)
{
	// Iterate event counts
	++eventCounter_;

	// Print run info
	if(diagLevel_ > 1)
	{
		TLOG(TLVL_INFO) << std::dec << "Run/Subrun/Event: " 
						<< e.run() << "/"
		                << e.subRun() << "/" 
						<< e.event() << std::endl;
	}

	// Try getting CRV data
	try
	{
		// Get CRV data decoders from the event, decoderTag is used to specify a CRV
		// producer
		auto decodersHandle = e.getValidHandle<std::vector<mu2e::CRVDataDecoder>>(decoderTag_);
		size_t nSubEvents = decodersHandle->size();

		// Process the decoder objects, each contains data from one subevent (see
		// DTCLib::DTC_SubEvent)
		for(size_t iSubEvent = 0; iSubEvent < nSubEvents; ++iSubEvent)
		{
			const mu2e::CRVDataDecoder& decoder((*decodersHandle)[iSubEvent]);
			decoder.setup_event();

			// Access the SubEventHeader through the internal event_ member
			// Why so weird? Can we add a GetSubEventHeader() method to the
			// CRVDataDecoder?
			auto subEventHeader = decoder.event_.GetHeader();

			// ROC latency
			auto link0_latency = subEventHeader->link0_drp_rx_latency * rocClockTick_;

			// Get EWT from first block
			auto EWT0 = decoder.dataAtBlockIndex(0)->GetHeader()->GetEventWindowTag().GetEventWindowTag(true);

			// Fill ROC latency graph
			graphs_["latency"]->SetPoint(subEventCounter_, EWT0, link0_latency);

			// Iterate subevent counts
			++subEventCounter_;

			// For each subevent, process the blocks (see DTCLib::DTC_DataBlock)
			for(size_t bl = 0; bl < decoder.block_count(); ++bl)
			{
				// Iterate block counts
				++blockCounter_;

				// Get block at this index
				auto block = decoder.dataAtBlockIndex(bl);
				if(!block)
					continue;  // Skip empty blocks

				// Get block header (DTCLib::DTC_DataHeaderPacket)
				auto blockHeader = block->GetHeader();  //

				if(diagLevel_ > 1)
				{  // Print the block header
					TLOG(TLVL_INFO) << blockHeader->toJSON() << std::endl;
				}

				if(!blockHeader->isValid())
				{
					if(diagLevel_ > 1)
						TLOG(TLVL_INFO) << "Block header is invalid..." << std::endl;
					continue;  // skip this block
				}

				// Get CRV hits / packets
				auto hits = decoder.GetCRVHits(bl);

				// Fill nHits histogram
				hists_["nHits"]->Fill(hits.size());

				// Process CRV hits
				for(auto& hit : hits)
				{
					// Iterate packet counter
					++packetCounter_;

					// Fill hit info level histograms
					hists_["febChannel"]->Fill(hit.first.febChannel);
					hists_["hitTime"]->Fill(hit.first.HitTime * crvClockTick_);  // ns
					hists_["numSamples"]->Fill(hit.first.NumSamples);

					// Process waveforms
					for(auto& waveforms : hit.second)
					{
						hists_["ADC"]->Fill(waveforms.ADC);
					}
				}

			}  // blocks

		}  // subevents
	}
	catch(const std::exception& e)  // Catch errors on subevent level
	{
		if(diagLevel_ > 0)
		{
			TLOG(TLVL_WARNING) << "Error processing subevent!" << e.what();
		}
	}

	// Update plots
	CrvDQM::UpdatePlots();

}  // analyze

// End job printouts
void CrvDQM::endJob()
{
	printf("\n**************************************************\n");
	printf("Processed:");
	printf("\n---> %zu events", eventCounter_);
	printf("\n---> %zu subevents", subEventCounter_);
	printf("\n---> %zu blocks", blockCounter_);
	printf("\n---> %zu packets", packetCounter_);

	if(keepAlive_ && !g_interrupted.load())
	{
		printf("\nKeeping server alive for %i minutes (or until Ctrl+C)",
		       keepAliveDuration_);
		printf("\n**************************************************\n");

		// Use the interruptible sleep function
		for(int i = 0; i < keepAliveDuration_ && !g_interrupted.load(); i++)
		{
			if(!interruptibleSleep(std::chrono::seconds(60)))
			{
				// Sleep was interrupted
				break;
			}
		}

		if(g_interrupted.load())
		{
			printf("\nReceived interrupt signal, shutting down server...\n");
		}
	}
	else
	{
		if(g_interrupted.load())
		{
			printf("\nReceived interrupt signal, shutting down server...\n");
		}
		else
		{
			printf("\nKilling server\n");
		}
		printf("**************************************************\n");
	}

	// Signal keepAliveThread to exit
	g_interrupted.store(true);
	g_cv.notify_all();
}

DEFINE_ART_MODULE(CrvDQM)

}  // namespace ots