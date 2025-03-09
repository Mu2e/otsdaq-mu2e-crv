// ROOT-based DQM and viewer for the CRV
// Author: Sam Grant, Simon Corrodi
// Date: Feb 2025

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

namespace ots {

// Globals for shutdown
std::atomic<bool> g_interrupted(false);
std::atomic<bool> g_sigint_received(false);
std::atomic<int> g_termination_attempts(0);
std::condition_variable g_cv;
std::mutex g_cv_m;

// Signal handler for shutdown
void SignalHandler(int signal) {
	// SIGINT: Ctrl+C; SIGTERM: termination signal
	if (signal == SIGINT || signal == SIGTERM) {
		g_termination_attempts++;
		std::cout << "\n *** Received termination signal #" << g_termination_attempts << " ***" << std::endl;

		// First attempt
		if (g_termination_attempts == 1) {
			// Broadcast termination
			g_interrupted.store(true);
			g_cv.notify_all();
			g_sigint_received.store(true);
		}
		// Second attempt: force exit after delay
		else if (g_termination_attempts == 2) {
			std::cout << "---> Forcing exit in 5 seconds..." << std::endl;
			std::thread([] {
				std::this_thread::sleep_for(std::chrono::seconds(5));
				if (g_termination_attempts >= 2) {
					std::cerr << "---> Process did not terminate gracefully. Exiting without cleanup via _exit(1)..." << std::endl;
					_exit(1); // Force exit with error
				}
			}).detach();
		}
		// Third attempt: force immediate exit with error
		else {
			std::cerr << "---> Immediate termination requested. Exiting now." << std::endl;
			_exit(1);
		}
	}
}

// Utility to convert enum values to strings for better logging
std::string subsystemToString(uint8_t subsystem) {
	switch (subsystem) {
	case 0: return "Tracker";
	case 1: return "Calorimeter";
	case 2: return "CRV";
	case 3: return "Other";
	case 4: return "STM";
	case 5: return "ExtMon";
	default: return "Unknown (" + std::to_string(subsystem) + ")";
	}
}

class CrvDQM : public art::EDAnalyzer {
  public:
	// Constructor
	explicit CrvDQM(fhicl::ParameterSet const &ps);
	// Destructor
	~CrvDQM() override;

  private:
	// art functions
	void beginJob() override;
	void analyze(art::Event const &e) override;
	void endJob() override;

	// Other functions
	void UpdatePlots();
	// void CreateMainPage();
	// void SetupDisplayPages();

	// fcl parameters
	int port_;
	int diagLevel_;
	float onlineRefreshPeriod_;
	bool keepAlive_;
	int keepAliveDuration_; // minutes
	std::string plotCol_; // "red"/"blue"/"green"
	art::InputTag decoderTag_; // specify the module producing CRV decoder obects
	int spillLength_;

	// TCanvas *canvas_;
	// Member variables
    std::unordered_map<std::string, TCanvas*> canvases_;  // Multiple canvases
	std::unordered_map<std::string, TH1D *> hists_;
	std::unordered_map<std::string, TGraph *> graphs_;
	THttpServer *server_;
	std::chrono::time_point<std::chrono::steady_clock> lastUpdate_;
	std::thread keepAliveThread_;
	std::thread exitThread_;
	float crvClockTick_;
	float rocClockTick_;
	bool shuttingDown_;
	std::string webPath_;
	
	// Counters
	std::string outputPrefix_;
	std::size_t eventCounts_{0};
	std::size_t subEventCounts_{0};
	std::size_t blockCounts_{0};
	std::size_t eventWindowTagCounts_{0};
	std::size_t packetCounts_{0};
	std::size_t crvBlockCounts_{0};
	std::size_t crvHitCounts_{0};
};

// Constructor implementation
CrvDQM::CrvDQM(fhicl::ParameterSet const &ps)
    : art::EDAnalyzer(ps)
	, port_(ps.get<int>("port", 8877))
	, diagLevel_(ps.get<int>("diagLevel", 0))
	, onlineRefreshPeriod_(ps.get<float>("onlineRefreshPeriod", 500)) // ms
    , keepAlive_(ps.get<int>("keepAlive", true))
	, keepAliveDuration_(ps.get<int>("keepAliveDuration", 5)) // minutes
    , plotCol_(ps.get<std::string>("plotCol", "red"))
	, decoderTag_(ps.get<art::InputTag>("decoderTag", "genFrags")) 
	, spillLength_(ps.get<int>("spillLength", 0xff))
	, webPath_(ps.get<std::string>("webPath", "/home/mu2ecrv/daq/ots_v3_03_01/srcs/otsdaq-mu2e-crv/otsdaq-mu2e-crv/Web/"))
{
	// Initialise non-fcl member variables
	crvClockTick_ = 12.5; // ns
	rocClockTick_ = 50; // ns, based on the 20 MHz clock (a guess)
	shuttingDown_ = false;
	outputPrefix_ = "[CrvDQM] ";
}

// Destructor implementation
CrvDQM::~CrvDQM() {
	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "Destructor called" << std::endl;
	}

	shuttingDown_ = true;

	// Signal interruption
	g_interrupted.store(true);
	g_cv.notify_all();

	// Join keepAliveThread if using
	if (keepAliveThread_.joinable()) {

		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "---> Joining keepAliveThread..." << std::endl;
		}

		// Create a joining thread so I can use timeouts
		std::thread joiner([this]() {
			keepAliveThread_.join();
			if (diagLevel_ > 1) {
				std::cout << outputPrefix_ << "---> keepAliveThread joined successfully" << std::endl;
			}
		});

		// Detacher joiner thread
		joiner.detach();

		// Wait a moment
		std::this_thread::sleep_for(std::chrono::seconds(3));

		// If thread is still joinable, detach it
		if (keepAliveThread_.joinable()) {
			std::cerr << outputPrefix_ << "---> keepAliveThread did not exit in time, detaching" << std::endl;
			keepAliveThread_.detach();
		}
	}

	// Then clean up ROOT objects
	if (server_) {
		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "---> Disabling server updates..." << std::endl;
		}
		try {
			server_->SetItemField("/", "_monitoring", "0");
			gSystem->ProcessEvents(); // Ensure the change propagates
			// Give it a moment
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		} catch (const std::exception &e) {
			std::cerr << outputPrefix_ << "Error disabling server updates: " << e.what() << std::endl;
		}

		// Now delete the server
		try {
			if (diagLevel_ > 1) {
				std::cout << outputPrefix_ << "---> Destroying HTTP server..." << std::endl;
			}
			delete server_;
		} catch (const std::exception &e) {
			std::cerr << outputPrefix_ << "Error destroying server: " << e.what() << std::endl;
		}
		server_ = nullptr;
	}
    // Delete canvases
    for (auto &canvas : canvases_) {
        try {
            if (canvas.second) {
                delete canvas.second;
            }
        } catch (const std::exception &e) {
            std::cerr << outputPrefix_ << "Error destroying canvas: " << e.what() << std::endl;
        }
        canvas.second = nullptr;
    }
    canvases_.clear();
	// Delete histograms
	for (auto &hist : hists_) {
		try {
			if (hist.second) {
				delete hist.second;
			}
		} catch (const std::exception &e) {
			std::cerr << outputPrefix_ << "Error destroying histogram: " << e.what() << std::endl;
		}
		hist.second = nullptr;
	}
	hists_.clear();

	// Delete graphs
	for (auto &graph : graphs_) {
		try {
			if (graph.second) {
				delete graph.second;
			}
		} catch (const std::exception &e) {
			std::cerr << outputPrefix_ << "Error destroying graph: " << e.what() << std::endl;
		}
		graph.second = nullptr;
	}
	graphs_.clear();

	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "Destructor complete" << std::endl;
	}
}

void CrvDQM::beginJob() {
	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "Begin Job" << std::endl;
	}

	// Install signal handlers for shutdown
	std::signal(SIGINT, SignalHandler);
	std::signal(SIGTERM, SignalHandler);

	// Create HTTP server
	server_ = new THttpServer(Form("http:%d", port_));

	// Set global plot style
	CrvDQMStyle::SetStyle();

	// Book histograms
	hists_["febChannel"] = new TH1D("febChannel", ";FEB channel;Hits", 64, -0.5, 63.5);
    hists_["ADC"] = new TH1D("ADC", ";Sample ADC;Samples", 401, -200, 200);
    hists_["hitTime"] = new TH1D("hitTime", ";Hit time [ns];Entries", (spillLength_)+1, 0, (spillLength_) * crvClockTick_);
    hists_["nHits"] = new TH1D("nHits", ";Hits / block;Blocks", 11, -.5, 10.5);
    
	// Create canvases
    canvases_["Overview"] = new TCanvas("Overview", "CRV Overview");
    canvases_["Overview"]->Divide(2, 2);

	// Overview canvas

    canvases_["Overview"]->cd(1);
    CrvDQMStyle::FormatHist(hists_["febChannel"], plotCol_);
    hists_["febChannel"]->Draw("HIST");
    
    canvases_["Overview"]->cd(2);
    CrvDQMStyle::FormatHist(hists_["ADC"], plotCol_);
    hists_["ADC"]->Draw("HIST");
    
    canvases_["Overview"]->cd(3);
    CrvDQMStyle::FormatHist(hists_["hitTime"], plotCol_);
    hists_["hitTime"]->Draw("HIST");
    
    canvases_["Overview"]->cd(4);
    CrvDQMStyle::FormatHist(hists_["nHits"], plotCol_);
    hists_["nHits"]->Draw("HIST");

	// ROC graphs
	

	// Set custom HTML 
	server_->SetDefaultPage(webPath_ + "CrvDQM.html");

	// Last update variable
	lastUpdate_ = std::chrono::steady_clock::now();

	// Print info
	std::cout << outputPrefix_ << "*** Server running on http://localhost:" << port_ << " ***" << std::endl;

	// Start an independent thread for server...
	// This is really just for running on a closed file
	// We want the page to stay alive after EOF
	keepAliveThread_ = std::thread([this]() {
		while (!g_interrupted.load() && !shuttingDown_) {
			try {
				gSystem->ProcessEvents();

				// Check for interruption more frequently
				std::unique_lock<std::mutex> lock(g_cv_m);
				g_cv.wait_for(lock, std::chrono::milliseconds(100), [] {
					return g_interrupted.load();
				});
			} catch (const std::exception &e) {
				std::cerr << outputPrefix_ << "Error in keepAliveThread: " << e.what() << std::endl;
				// Break on exception to avoid tight error loop
				break;
			}
		}
		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "---> Keep-alive thread exiting..." << std::endl;
		}
	});

	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "beginJob complete" << std::endl;
	}
}

/*
void CrvDQM::SetupDisplayPages() {
    
    
    // Create the main overview canvas
    canvases_["overview"] = new TCanvas("overview", "CRV Overview");
    canvases_["overview"]->Divide(2, 2);
    
    // Create additional canvases as placeholders for now
    // canvases_["waveforms"] = new TCanvas("waveforms", "CRV Waveforms");
    // canvases_["waveforms"]->Divide(2, 1);
    
    // canvases_["hits"] = new TCanvas("hits", "CRV Hit Details");
    // canvases_["hits"]->Divide(2, 2);
    
    // canvases_["latency"] = new TCanvas("latency", "CRV Latency");
    // canvases_["latency"]->Divide(1, 1);
    
    // Book histograms for the overview page (same as existing CreateMainPage)
    hists_["febChannel"] = new TH1D("febChannel", ";FEB channel;Hits", 64, -0.5, 63.5);
    hists_["ADC"] = new TH1D("ADC", ";Sample ADC;Samples", 401, -200, 200);
    hists_["hitTime"] = new TH1D("hitTime", ";Hit time [ns];Entries", (spillLength_)+1, 0, (spillLength_) * crvClockTick_);
    hists_["nHits"] = new TH1D("nHits", ";Hits / block;Blocks", 11, -.5, 10.5);
    
    // Add the missing numSamples histogram
    // hists_["numSamples"] = new TH1D("numSamples", ";Samples / block;Entries", 16, 0.5, 15.5);
    
    // // Create the latency graph
    // graphs_["latency"] = new TGraph();
    // graphs_["latency"]->SetTitle(";Subevent;ROC latency [ns]");
    
    // Format and draw - overview canvas
    canvases_["overview"]->cd(1);
    CrvDQMStyle::FormatHist(hists_["febChannel"], plotCol_);
    hists_["febChannel"]->Draw("HIST");
    
    canvases_["overview"]->cd(2);
    CrvDQMStyle::FormatHist(hists_["ADC"], plotCol_);
    hists_["ADC"]->Draw("HIST");
    
    canvases_["overview"]->cd(3);
    CrvDQMStyle::FormatHist(hists_["hitTime"], plotCol_);
    hists_["hitTime"]->Draw("HIST");
    
    canvases_["overview"]->cd(4);
    CrvDQMStyle::FormatHist(hists_["nHits"], plotCol_);
    hists_["nHits"]->Draw("HIST");
    
    // Create simple placeholder histograms for other pages
    // // Waveforms page placeholders
    // hists_["waveform_raw"] = new TH1D("waveform_raw", ";Sample index;ADC", 20, -0.5, 19.5);
    // canvases_["waveforms"]->cd(1);
    // CrvDQMStyle::FormatHist(hists_["waveform_raw"], plotCol_);
    // hists_["waveform_raw"]->Draw("HIST");
    
    // canvases_["waveforms"]->cd(2);
    // CrvDQMStyle::FormatHist(hists_["numSamples"], plotCol_);
    // hists_["numSamples"]->Draw("HIST");
    
    // // Hits page placeholders - just use copies of histograms we already have
    // canvases_["hits"]->cd(1);
    // hists_["hitTime_detail"] = new TH1D("hitTime_detail", ";Hit time [ns];Entries", 
    //                                  (spillLength_)+1, 0, (spillLength_) * crvClockTick_);
    // CrvDQMStyle::FormatHist(hists_["hitTime_detail"], plotCol_);
    // hists_["hitTime_detail"]->Draw("HIST");
    
    // canvases_["hits"]->cd(2);
    // hists_["febChannel_detail"] = new TH1D("febChannel_detail", ";FEB channel;Hits", 64, -0.5, 63.5);
    // CrvDQMStyle::FormatHist(hists_["febChannel_detail"], plotCol_);
    // hists_["febChannel_detail"]->Draw("HIST");
    
    // canvases_["hits"]->cd(3);
    // hists_["febDistribution"] = new TH1D("febDistribution", ";FEB ID;Hits", 20, 0.5, 20.5);
    // CrvDQMStyle::FormatHist(hists_["febDistribution"], plotCol_);
    // hists_["febDistribution"]->Draw("HIST");
    
    // canvases_["hits"]->cd(4);
    // hists2D_["channelVsTime"] = new TH2D("channelVsTime", ";Hit time [ns];Channel", 
    //                                    (spillLength_)+1, 0, (spillLength_) * crvClockTick_,
    //                                    64, -0.5, 63.5);
    // hists2D_["channelVsTime"]->Draw("COLZ");
    
    // // Format and draw - latency canvas
    // canvases_["latency"]->cd(1);
    // CrvDQMStyle::FormatGraph(graphs_["latency"], plotCol_);
    // graphs_["latency"]->Draw("APL");
    
    // Register each canvas at a specific URL path
    server_->Register("/overview", canvases_["overview"]);
    // server_->Register("/waveforms", canvases_["waveforms"]);
    // server_->Register("/hits", canvases_["hits"]);
    // server_->Register("/latency", canvases_["latency"]);

	server_->SetItemField("/", "_browser", "off"); // Sidebar off
	server_->SetItemField("/", "_monitoring", Form("%f", onlineRefreshPeriod_)); // Update period
	server_->SetItemField("/overview", "_drawitem", "overview"); // Set DQM canvas as default item
	server_->SetItemField("/", "_http_cache", "0"); 
	server_->SetItemField("/", "_toplevel", "overview");
    
    // Create a simple navigation page with a clean design
	
    std::string navPage = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <title>CRV Monitor</title>
            <style>
                body { 
                    font-family: Arial, sans-serif; 
                    max-width: 800px; 
                    margin: 0 auto; 
                    padding: 20px; 
                    background-color: #f5f5f5; 
                }
                h1 { 
                    color: #336699; 
                    text-align: center;
                    border-bottom: 2px solid #336699;
                    padding-bottom: 10px;
                }
                .status-bar {
                    background-color: #eee;
                    padding: 10px;
                    margin-bottom: 20px;
                    border-radius: 5px;
                    text-align: center;
                }
                .status-item {
                    display: inline-block;
                    margin: 0 15px;
                    font-weight: bold;
                }
                .nav-grid { 
                    display: grid; 
                    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); 
                    gap: 20px; 
                    margin-top: 30px;
                }
                .nav-card { 
                    padding: 20px;
                    background: white; 
                    border-radius: 8px;
                    text-align: center;
                    box-shadow: 0 4px 6px rgba(0,0,0,0.1);
                    transition: transform 0.2s, box-shadow 0.2s;
                }
                .nav-card:hover {
                    transform: translateY(-5px);
                    box-shadow: 0 6px 8px rgba(0,0,0,0.15);
                }
                .nav-card a { 
                    display: block;
                    font-size: 20px;
                    text-decoration: none;
                    color: #336699;
                    padding: 15px;
                }
                .nav-card p {
                    color: #666;
                    margin-top: 5px;
                }
            </style>
        </head>
        <body>
            <h1>CRV Data Quality Monitor</h1>
            
            <div class="status-bar">
                <div class="status-item">Events: <span id="event-count">0</span></div>
                <div class="status-item">CRV Hits: <span id="hit-count">0</span></div>
                <div class="status-item">Last Update: <span id="last-update">-</span></div>
            </div>
            
            <div class="nav-grid">
                <div class="nav-card">
                    <a href="/overview" target="_blank">Overview</a>
                    <p>Main monitoring dashboard with critical metrics</p>
                </div>
                <div class="nav-card">
                    <a href="/waveforms" target="_blank">Waveforms</a>
                    <p>Detailed waveform analysis and sample distributions</p>
                </div>
                <div class="nav-card">
                    <a href="/hits" target="_blank">Hit Details</a>
                    <p>Comprehensive information about hit patterns and timing</p>
                </div>
                <div class="nav-card">
                    <a href="/latency" target="_blank">Latency</a>
                    <p>ROC latency measurements and performance metrics</p>
                </div>
            </div>
            
            <script>
                // Update the status display
                document.getElementById('event-count').textContent = )" + std::to_string(eventCounts_) + R"(;
                document.getElementById('hit-count').textContent = )" + std::to_string(crvHitCounts_) + R"(;
                document.getElementById('last-update').textContent = new Date().toLocaleTimeString();
            </script>
        </body>
        </html>
    )";
	
    
    // // Register the navigation page as the main page
    // server_->CreateItem("/index.htm", navPage.c_str());
    // server_->SetItemField("/", "_toplevel", "index.htm");
    
    if (diagLevel_ > 0) {
        std::cout << outputPrefix_ << "Display pages configured" << std::endl;
    }
}
*/

void CrvDQM::UpdatePlots() {
	// Check interrupt flag first
	if (g_interrupted.load() || shuttingDown_) return;

	// Get time
	auto currentTime = std::chrono::steady_clock::now();
	std::chrono::duration<double, std::milli> elapsed = currentTime - lastUpdate_;

	if (elapsed.count() >= onlineRefreshPeriod_) {
		try {
			// // Auto scale histogram y-axis
			for (auto &hist : hists_) {
				double maxContent = hist.second->GetBinContent(hist.second->GetMaximumBin());
				hist.second->GetYaxis()->SetRangeUser(0, 1.15 * maxContent);
			}

			/*
			for (auto &graph : graphs_) {
				// Get the number of points in the graph
				int nPoints = graph.second->GetN();
				// Get the x and y values of the graph
				double *xValues = graph.second->GetX();
				double *yValues = graph.second->GetY();

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
			} */

		    // // Update counter information for the sidebar
            // std::string counterJson = Form("{ \"events\": %zu, \"hits\": %zu }", eventCounts_, crvHitCounts_);
            // server_->CreateItem("/Counters", counterJson.c_str(), kTRUE); // kTRUE to update existing item

            // std::string counterParams = Form("?events=%zu&hits=%zu", eventCounts_, crvHitCounts_);
			// server_->SetItemField("/Main", "_url_args", counterParams.c_str());

			// Update all canvases
            for (auto &canvas : canvases_) {
                canvas.second->Modified();
                canvas.second->Update();
            }

			gSystem->ProcessEvents(); // Update display
			lastUpdate_ = currentTime; // Update the time
		} catch (const std::exception &e) {
			std::cerr << outputPrefix_ << "Error updating plots: " << e.what() << std::endl;
		}
	}
}

void CrvDQM::analyze(art::Event const &e) {
	// Check if we're interrupted
	if (g_interrupted.load() || shuttingDown_) return;

	if (diagLevel_ > 1) {
		std::cout << outputPrefix_ << "=================== " << e.id() << " ===================" << std::endl;
	}

	// Summary counters for current event
	size_t currentEventSubEvents = 0;
	size_t currentEventBlocks = 0;
	size_t currentEventPackets = 0;
	size_t currentEventCrvBlocks = 0;
	size_t currentEventCrvHits = 0;

	// Try getting CRV decoder
	try {
		// Get CRV data decoders from the event
		// decoderTag is used to specify a CRV producer
		auto crvDecodersHandle = e.getValidHandle<std::vector<mu2e::CRVDataDecoder>>(decoderTag_);
		currentEventSubEvents = crvDecodersHandle->size();

		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "Found CRV decoders for " << crvDecodersHandle->size() << " subevent(s)" << std::endl;
		}

		// Process the decoder objects, each contains data from one subevent
		// (see DTCLib::DTC_SubEvent)
		for (size_t iSubEvent = 0; iSubEvent < crvDecodersHandle->size(); ++iSubEvent) {
			const mu2e::CRVDataDecoder &crvDecoder((*crvDecodersHandle)[iSubEvent]);
			// crvDecoder.setup_event(); // not required

			// Access the SubEvent through the internal event_ member
			auto subEvent = crvDecoder.event_;
			auto subEventHeader = subEvent.GetHeader();
			currentEventBlocks = subEvent.GetDataBlockCount();

			// Get EWT from first block
			auto EWT = crvDecoder.dataAtBlockIndex(0)->GetHeader()->GetEventWindowTag().GetEventWindowTag(true);
			// auto EWT = crvDecoder.event_->GetEventWindowTag().GetEventWindowTag(true);

			if (diagLevel_ > 1) {
				std::cout << outputPrefix_ << "---> Subevent [" << iSubEvent << "]" << std::endl;
				std::cout << outputPrefix_ << subEventHeader->toJson() << std::endl;
				std::cout << outputPrefix_ << "EWT " << EWT << std::endl;
			}

			// ROC0 latency
			// auto link0_latency = subEventHeader->link0_drp_rx_latency * rocClockTick_;
			//graphs_["latency"]->SetPoint(subEventCounts_, EWT, link0_latency);

			// Process the blocks (DTCLib::DTC_DataBlock)
			for (size_t iBlock = 0; iBlock < currentEventBlocks; ++iBlock) {

				// Get block at this index
				auto block = crvDecoder.dataAtBlockIndex(iBlock);
				auto blockheader = block->GetHeader();
				auto subsystem = blockheader->GetSubsystem();
				currentEventPackets += blockheader->GetPacketCount();

				if (diagLevel_ > 1) {
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

					if (diagLevel_ > 2) {
						for (int iPacket = 0; iPacket < blockheader->GetPacketCount(); ++iPacket) {
							std::cout << outputPrefix_ << "---> Packet [" << iPacket << "]: " << DTCLib::DTC_DataPacket(((uint8_t *)block->blockPointer) + ((iPacket + 1) * 16)).toJSON() << std::endl;
						}
					}
				}

				// Make sure we only process CRV data
				if (blockheader->GetSubsystem() == DTCLib::DTC_Subsystem_CRV) // && blockheader->isValid() && blockheader->GetVersion() == 0x0)
				{
					++currentEventCrvBlocks;

					if (diagLevel_ > 0) {
						std::cout << outputPrefix_ << "*** CRV BLOCK FOUND ***" << std::endl;
						std::cout << outputPrefix_ << "CRV Block Status: " << (int)blockheader->GetStatus() << std::endl;
					}

					if (blockheader->isValid()) {
						if (blockheader->GetVersion() == 0x0) {

							// Get ROC status packet
							auto crvRocStatus = crvDecoder.GetCRVROCStatusPacket(iBlock);

							if (crvRocStatus != nullptr && diagLevel_ > 0) {
								std::cout << outputPrefix_ << "CRV ROC Status Packet:" << std::endl
								          << outputPrefix_ << "  Controller ID: " << (int)crvRocStatus->ControllerID << std::endl
								          << outputPrefix_ << "  Active FEB Flags: " << crvRocStatus->GetActiveFEBFlags().to_string() << std::endl
								          << outputPrefix_ << "  Trigger Count: " << crvRocStatus->TriggerCount << std::endl
								          << outputPrefix_ << "  Event Window Tag: " << crvRocStatus->GetEventWindowTag() << std::endl;
							}

							// Get CRV hits for this block
							auto crvHits = crvDecoder.GetCRVHits(iBlock);
							currentEventCrvHits += crvHits.size();

							if (diagLevel_ > 0) {
								std::cout << outputPrefix_ << "Found " << crvHits.size() << " CRV hits" << std::endl;
							}

							// Output detailed hit information
							if (diagLevel_ > 1 && !crvHits.empty()) {
								std::cout << "---> First CRV Hit Details:" << std::endl;
								auto &firstHit = crvHits.front();
								std::cout << outputPrefix_ << "  FEB Channel: " << firstHit.first.febChannel << std::endl
								          << outputPrefix_ << "  Port Number: " << firstHit.first.portNumber << std::endl
								          << outputPrefix_ << "  Controller Number: " << firstHit.first.controllerNumber << std::endl
								          << outputPrefix_ << "  Hit Time: " << firstHit.first.HitTime << std::endl
								          << outputPrefix_ << "  Num Samples: " << firstHit.first.NumSamples << std::endl;

								if (!firstHit.second.empty()) {
									std::cout << outputPrefix_ << "  First Sample ADC: " << firstHit.second.front().ADC << std::endl;
								}
							}

							// Deeper waveform details for even higher diagnostic levels
							if (diagLevel_ > 2 && !crvHits.empty()) {
								auto &firstHit = crvHits.front();
								std::cout << outputPrefix_ << "Waveform for first hit:" << std::endl;
								for (size_t i = 0; i < firstHit.second.size(); i++) {
									if (i % 8 == 0) std::cout << "  ";
									std::cout << std::setw(5) << firstHit.second[i].ADC;
									if (i % 8 == 7 || i == firstHit.second.size() - 1) std::cout << std::endl;
								}
							}

							// Fill nHits histogram
							hists_["nHits"]->Fill(crvHits.size());

							// Process CRV hits in this block
							for (auto &crvHit : crvHits) {

								// Fill hit info level histograms
								hists_["febChannel"]->Fill(crvHit.first.febChannel);
								hists_["hitTime"]->Fill(crvHit.first.HitTime * crvClockTick_); // ns
								//hists_["numSamples"]->Fill(crvHit.first.NumSamples);

								// Process waveforms
								for (auto &waveforms : crvHit.second) {
									hists_["ADC"]->Fill(waveforms.ADC);
								}
							}
						} else {
							if (diagLevel_ > 0) {
								std::cout << outputPrefix_ << "CRV block with unsupported version: 0x"
								          << std::hex << (int)blockheader->GetVersion() << std::dec << std::endl;
							}
						}
					} else if (diagLevel_ > 0) {
						std::cout << outputPrefix_ << "Invalid CRV block header!" << std::endl;
					}
				}

			} // blocks

		} // subevents
	} catch (const std::exception &e) // Catch errors on event level
	{
		if (diagLevel_ > 0) {
			std::cerr << outputPrefix_ << "Error processing event!" << e.what() << std::endl;
		}
	}

	// Update plots
	CrvDQM::UpdatePlots();

	// Print summary for this event
	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "Event Summary: "
		          << currentEventSubEvents << " SubEvents; "
		          << currentEventBlocks << " Blocks; "
		          << currentEventPackets << " Packets; "
		          << currentEventCrvBlocks << " CRV Blocks; "
		          << currentEventCrvHits << " CRV Hits."
		          << std::endl;
	}

	// Iterate  counts
	++eventCounts_;
	subEventCounts_ += currentEventSubEvents;
	blockCounts_ += currentEventBlocks;
	packetCounts_ += currentEventPackets;
	crvBlockCounts_ += currentEventCrvBlocks;
	crvHitCounts_ += currentEventCrvHits;

} // analyze

// End job printouts
void CrvDQM::endJob() {

	if (diagLevel_ > 0) {
		// Print job-level statistics
		std::cout << outputPrefix_ << "================= End Job Summary =================" << std::endl;
		std::cout << outputPrefix_ << "Total Events: " << eventCounts_ << std::endl;
		std::cout << outputPrefix_ << "Total SubEvents: " << subEventCounts_ << std::endl;
		std::cout << outputPrefix_ << "Total Blocks: " << blockCounts_ << std::endl;
		std::cout << outputPrefix_ << "Total CRV Blocks: " << crvBlockCounts_ << std::endl;
		std::cout << outputPrefix_ << "Total CRV Hits: " << crvHitCounts_ << std::endl;
		std::cout << outputPrefix_ << "===============================================" << std::endl;
	}

	// We're shutting down
	shuttingDown_ = true;

	// Keep alive (if keeping alive) until timeout or signal interrupt
	if (keepAlive_ && !g_sigint_received.load()) {
		if (diagLevel_ > 0) {
			std::cout << outputPrefix_ << "Keeping server alive for " << keepAliveDuration_ << " minute(s)" << std::endl;
		}

		// Keep alive end time
		auto endTime = std::chrono::steady_clock::now() + std::chrono::minutes(keepAliveDuration_);

		// Loop until end time is reached, but allow for interrupt
		while (std::chrono::steady_clock::now() < endTime && !g_sigint_received.load()) {
			// Check for interrupts in between small sleeps
			std::unique_lock<std::mutex> lock(g_cv_m);
			g_cv.wait_for(lock, std::chrono::milliseconds(100), [] {
				return g_interrupted.load() || g_sigint_received.load();
			});
			// Process any pending events
			gSystem->ProcessEvents();
		}
	}

	// No matter what, ensure we signal the thread to exit
	g_interrupted.store(true);
	g_cv.notify_all();

	// Cleanup
	if (g_interrupted.load() || g_sigint_received.load()) {
		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "Received termination signal" << std::endl;
		}

	} else {
		if (diagLevel_ > 1) {
			std::cout << outputPrefix_ << "Keep-alive period ended" << std::endl;
		}
	}

	if (diagLevel_ > 0) {
		std::cout << outputPrefix_ << "endJob complete" << std::endl;
	}
}

DEFINE_ART_MODULE(CrvDQM)

} // namespace ots