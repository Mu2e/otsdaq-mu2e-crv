///////////////////////////////////////////////////////////////////////////////
// P.Murat: a 0-th order example of a DQM client using ROOT web-based graphics
//          cloned from  WFViewer_module.cc w/o much thought
//          the only purpose is to demonstrate the use of the web-based GUI
// - creates two canvases with the following URLs:
//   http://127.0.0.1:8877/win1/
//   http://127.0.0.1:8877/win2/
// key points: 
// - create a TApplication running in batch mode and using a certain URL
// - call gSystem->ProcessEvents() once per event 
//
// for illustration only, save histograms once in the end of the run 
// (an online application has to do it periodically during the run)
///////////////////////////////////////////////////////////////////////////////
#include "TRACE/tracemf.h"
#define TRACE_NAME "CrvVstDemoViewer"

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "canvas/Utilities/InputTag.h"
#include "cetlib_except/exception.h"

#include "artdaq-core/Data/ContainerFragment.hh"
#include "artdaq-core/Data/Fragment.hh"

#include "artdaq-core-mu2e/Data/CRVDataDecoder.hh"
#include "artdaq-core-mu2e/Overlays/DTCEventFragment.hh"
#include "artdaq-core-mu2e/Overlays/FragmentType.hh"

//#include "artdaq-core-demo/Overlays/FragmentType.hh"
//#include "artdaq-core-demo/Overlays/ToyFragment.hh"

#include <TApplication.h>
#include <TSystem.h>
#include <TAxis.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TRootCanvas.h>
#include <TStyle.h>

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "tracemf.h"
#define TRACE_NAME "CrvVstDemoViewer"

namespace demo {
/**
 * \brief An example art analysis module which plots events both as histograms and event snapshots (plot of ADC value vs ADC number)
 */
class CrvVstDemoViewer : public art::EDAnalyzer {
public:
	/**
	 * \brief CrvVstDemoViewer Constructor
	 * \param p ParameterSet used to configure CrvVstDemoViewer
	 *
	 * \verbatim
	 * CrvVstDemoViewer accepts the following Parameters:
     * "port" port the web server is run on, default is 8877
	 * "fileName": (Default: artdaqdemo_onmon.root): File name for output, if
	 * "write_to_file": (Default: false): Whether to write output histograms to "fileName"
     * "diag_level": if > 0 the package content is printed, defaults to 0 
     * "online_refresh_period_ms" refresh period of canvases in milli seconds, defaults to 500 ms
     * "sleep_per_event" of not 0, an additional sleep in seconds is added to each event, useful for running on offline data
	 * \endverbatim
	 */
	explicit CrvVstDemoViewer(fhicl::ParameterSet const& p);

	~CrvVstDemoViewer() override;

	void analyze(art::Event const& e) override;

	void beginJob()                   override;
	void beginRun(art::Run const&  e) override;
	void endRun  (art::Run const&  e) override;

private:
	CrvVstDemoViewer(CrvVstDemoViewer const&) = delete;
	CrvVstDemoViewer(CrvVstDemoViewer&&) = delete;
	CrvVstDemoViewer& operator=(CrvVstDemoViewer const&) = delete;
	CrvVstDemoViewer& operator=(CrvVstDemoViewer&&) = delete;

	TCanvas*                  _hCanvas;
	TCanvas*                  _gCanvas;
	std::vector<Double_t>     x_;
	//int                       prescale_;
	art::RunNumber_t          current_run_;

	//size_t                    max_num_x_plots_;
	//size_t                    max_num_y_plots_;
	//std::size_t               num_x_plots_;
	//std::size_t               num_y_plots_;

	//std::string               raw_data_label_;

	std::unordered_map<std::string, TGraph*> graphs_;
	std::unordered_map<std::string, TH1D*> histograms_;

	//std::map<artdaq::Fragment::fragment_id_t, std::size_t> id_to_index_;

	std::string    outputFileName_;
	TFile*         fFile_;
	bool           writeOutput_;
	bool           newCanvas_;
	//bool           dynamicMode_;

	TApplication*  _app;
	bool           force_new_;
	bool           dont_quit_;
    int            diagLevel_;
    int            port_;
    double         onlineRefreshPeriod_;
    std::chrono::time_point<std::chrono::steady_clock> lastUpdate_;


	//void getXYDims_ ();
	void bookCanvas_();
};

  //-----------------------------------------------------------------------------
CrvVstDemoViewer::CrvVstDemoViewer(fhicl::ParameterSet const& ps)
    : art::EDAnalyzer  (ps)
    //, prescale_        (ps.get<int>        ("prescale"))
    , current_run_     (0)
    //, max_num_x_plots_ (ps.get<std::size_t>("num_x_plots", std::numeric_limits<std::size_t>::max()))
    //, max_num_y_plots_ (ps.get<std::size_t>("num_y_plots", std::numeric_limits<std::size_t>::max()))
    //, num_x_plots_     (0)
    //, num_y_plots_     (0)
    //, raw_data_label_  (ps.get<std::string>("raw_data_label", "daq"))
    , graphs_          ()
    , histograms_      ()
    , outputFileName_  (ps.get<std::string>("fileName", "artdaqdemo_onmon.root"))
    , writeOutput_     (ps.get<bool>       ("write_to_file", false))
    //, newCanvas_       (true)
    //, dynamicMode_     (ps.get<bool>       ("dynamic_mode", true))
    , diagLevel_       (ps.get<int>        ("diag_level", 0))
    , port_            (ps.get<int>        ("port", 8877))
    , onlineRefreshPeriod_ (ps.get<double> ("online_refresh_period_ms", 500))
{
	gStyle->SetOptStat("e");
	gStyle->SetMarkerStyle(22);
	gStyle->SetMarkerColor(4);
    lastUpdate_ = std::chrono::steady_clock::now();
    //std::cout << "CrvVstDemoViewer Constructor " << std::endl;


	//if (ps.has_key("fragment_ids")) {
	//	auto fragment_ids = ps.get<std::vector<artdaq::Fragment::fragment_id_t>>("fragment_ids");
	//	for (auto& id : fragment_ids) {
	//		auto index = id_to_index_.size();
	//		id_to_index_[id] = index;
	//	}
	//}
}
  //-----------------------------------------------------------------------------
void CrvVstDemoViewer::beginJob() {
  std::cout << "CrvVstDemoViewer beginJob " << std::endl;

  int           tmp_argc(2);
  char**        tmp_argv;

  tmp_argv    = new char*[2];
  tmp_argv[0] = new char[100];
  tmp_argv[1] = new char[100];

  strcpy(tmp_argv[0],"-b");
  //strcpy(tmp_argv[1],"--web=server:8877");
  sprintf(tmp_argv[1], "--web=server:%d", port_); 

  //std::cout << "Debug" << gEnv << std::endl;

  _app = new TApplication("CrvVstDemoViewer", &tmp_argc, tmp_argv);

  // app->Run()
  // app_->Run(true);
  delete [] tmp_argv;

    _hCanvas = nullptr;
	_gCanvas = nullptr;
	for (auto& x : graphs_    ) x.second = nullptr;
	for (auto& x : histograms_) x.second = nullptr;

	newCanvas_ = true;
	//if (!dynamicMode_) 
    bookCanvas_();

    // create "static" histograms
    histograms_["channels"] = new TH1D("channels","",64,-0.5,63+0.5);
    histograms_["channels"]->SetTitle("channels");
    histograms_["channels"]->GetXaxis()->SetTitle("Channel");
    histograms_["channels"]->GetYaxis()->SetTitle("counts");
    _hCanvas->cd(1);
    histograms_["channels"]->Draw();

    histograms_["timestamps"] = new TH1D("timestamps","",4095,-0.5,4095+0.5);
    histograms_["timestamps"]->SetTitle("Hit Times");
    histograms_["timestamps"]->GetXaxis()->SetTitle("Hit Time");
    histograms_["timestamps"]->GetYaxis()->SetTitle("counts");
    _hCanvas->cd(2);
    histograms_["timestamps"]->Draw();

    histograms_["nhits"] = new TH1D("nhits","",21,-0.5,20+0.5);
    histograms_["nhits"]->SetTitle("Number of Hits");
    histograms_["nhits"]->GetXaxis()->SetTitle("number of hits per EW");
    histograms_["nhits"]->GetYaxis()->SetTitle("counts");
    _hCanvas->cd(3);
    histograms_["nhits"]->Draw();

    histograms_["adc"] = new TH1D("adc","",4096,-2048.+0.5,2048.-0.5);
    histograms_["adc"]->SetTitle("ADC values");
    histograms_["adc"]->GetXaxis()->SetTitle("adc values");
    histograms_["adc"]->GetYaxis()->SetTitle("counts");
    _hCanvas->cd(4);
    histograms_["adc"]->Draw();
}

//-----------------------------------------------------------------------------
/*
void CrvVstDemoViewer::getXYDims_() {
	// Enforce positive maxes
	if (max_num_x_plots_ == 0) max_num_x_plots_ = std::numeric_limits<size_t>::max();
	if (max_num_y_plots_ == 0) max_num_y_plots_ = std::numeric_limits<size_t>::max();

	num_x_plots_ = num_y_plots_ = static_cast<std::size_t>(ceil(sqrt(id_to_index_.size())));

	// Do trivial check first to avoid multipling max * max -> undefined
	if (id_to_index_.size() > max_num_x_plots_ && id_to_index_.size() > max_num_x_plots_ * max_num_y_plots_) {
		num_x_plots_ = max_num_x_plots_;
		num_y_plots_ = max_num_y_plots_;
		auto max     = num_x_plots_ * num_y_plots_;
		auto it      = id_to_index_.begin();
		while (it != id_to_index_.end()) {
			if   (it->second >= max)  it = id_to_index_.erase(it); 
			else            				++it;
		}
	}

	// Some predefined "nice looking" plotscapes...

	if (max_num_x_plots_ >= 4 && max_num_y_plots_ >= 2)
	{
		switch (id_to_index_.size())
		{
			case 1:
				num_x_plots_ = num_y_plots_ = 1;
				break;
			case 2:
				num_x_plots_ = 2;
				num_y_plots_ = 1;
				break;
			case 3:
			case 4:
				num_x_plots_ = 2;
				num_y_plots_ = 2;
				break;
			case 5:
			case 6:
				num_x_plots_ = 3;
				num_y_plots_ = 2;
				break;
			case 7:
			case 8:
				num_x_plots_ = 4;
				num_y_plots_ = 2;
				break;
			default:
				break;
		}
	}
	else
	{
		// Make sure we fit within specifications
		while (num_x_plots_ > max_num_x_plots_)
		{
			num_x_plots_--;
			num_y_plots_ = static_cast<size_t>(ceil(id_to_index_.size() / num_x_plots_));
		}
	}
	TLOG(TLVL_DEBUG) << "id count: " << id_to_index_.size() << ", num_x_plots_: " << num_x_plots_ << " / " << max_num_x_plots_ << ", num_y_plots_: " << num_y_plots_ << " / " << max_num_y_plots_;
}
*/

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CrvVstDemoViewer::bookCanvas_() {
	newCanvas_ = false;
	//getXYDims_();

  _hCanvas = new TCanvas("wf0");
  //_hCanvas->Divide(num_x_plots_, num_y_plots_);
  _hCanvas->Divide(2, 2);
  _hCanvas->SetTitle("Distributions");
  _hCanvas->Update();

  _gCanvas = new TCanvas("wf1");
  //_gCanvas->Divide(num_x_plots_, num_y_plots_);
  _gCanvas->Divide(4, 4);
  _gCanvas->SetTitle("Waveforms");
  _gCanvas->Update();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CrvVstDemoViewer::~CrvVstDemoViewer() {
	// We're going to let ROOT's own garbage collection deal with histograms and Canvases...
	for (auto& histogram : histograms_) 	{
		histogram.second = nullptr;
	}
	histograms_.clear();
	for (auto& graph : graphs_) {
		graph.second = nullptr;
	}
	graphs_.clear();

	_hCanvas = nullptr;
	_gCanvas = nullptr;
	fFile_ = nullptr;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CrvVstDemoViewer::analyze(art::Event const& event) {
	static std::size_t evt_cntr = -1;
	evt_cntr++;
    
	// New code for CRV VST Demo
    std::vector<art::Handle<artdaq::Fragments> > fragmentHandles;
    fragmentHandles = event.getMany<std::vector<artdaq::Fragment> >();

    artdaq::FragmentPtrs containerFragments;
    artdaq::Fragments fragments;
	for (const auto& handle : fragmentHandles) {
        if (!handle.isValid() || handle->empty()) {
	        continue;
        }
        if (handle->front().type() == artdaq::Fragment::ContainerFragmentType) {
            for (const auto& cont : *handle) {
                artdaq::ContainerFragment contf(cont);
	            if (contf.fragment_type() != mu2e::FragmentType::DTCEVT) {
                    break;
                }
                for (size_t ii = 0; ii < contf.block_count(); ++ii) {
                    containerFragments.push_back(contf[ii]);
                    fragments.push_back(*containerFragments.back());
                }
            }
        } else {
            if (handle->front().type() == mu2e::FragmentType::DTCEVT) {
                for (auto frag : *handle) {
                    fragments.emplace_back(frag);
                }
            }
        }
    }
    if (diagLevel_ > 1) {
        TLOG(TLVL_INFO) << "[CrvVstDemoViewer::analyze] Found nFragments  " << fragments.size();
    }

    // handle the fragments
    for (const auto& frag : fragments) {
        mu2e::DTCEventFragment bb(frag);
        auto data = bb.getData();
        auto event = &data;
        if (diagLevel_ > 1)
            TLOG(TLVL_INFO) << "Event tag:\t" << "0x" << std::hex << std::setw(4) << std::setfill('0') << event->GetEventWindowTag().GetEventWindowTag(true);
        DTCLib::DTC_EventHeader* eventHeader = event->GetHeader();
        if (diagLevel_ > 1) {
            TLOG(TLVL_INFO) << eventHeader->toJson() << std::endl
            << "Subevents count: " << event->GetSubEventCount() << std::endl;
        }
        
        bool hist_updated = false;
        for (unsigned int i = 0; i < event->GetSubEventCount(); ++i) { // In future, use GetSubsystemData to only get CRV subevents
            DTCLib::DTC_SubEvent& subevent = *(event->GetSubEvent(i));
            if (diagLevel_ > 1) {
                TLOG(TLVL_INFO) << "Subevent [" << i << "]:" << std::endl;
                TLOG(TLVL_INFO) << subevent.GetHeader()->toJson() << std::endl;
                TLOG(TLVL_INFO) << "Number of Data Block: " << subevent.GetDataBlockCount() << std::endl;
            }
                
            for (size_t bl = 0; bl < subevent.GetDataBlockCount(); ++bl) {
                auto block = subevent.GetDataBlock(bl);
                auto blockheader = block->GetHeader();
                if (diagLevel_ > 1) {
                    TLOG(TLVL_INFO) << blockheader->toJSON() << std::endl;
                    for (int ii = 0; ii < blockheader->GetPacketCount(); ++ii) {
                        TLOG(TLVL_INFO) << DTCLib::DTC_DataPacket(((uint8_t*)block->blockPointer) + ((ii + 1) * 16)).toJSON() << std::endl;
                    }
                }
                // check if we want to decode this data block
                // make sure we only process CRV data
                if(blockheader->GetSubsystem() == 0x2) {
                    if(blockheader->isValid()) {
                        // DQM for version 0x0 data
                        if( blockheader->GetVersion() == 0x0 ) {
                            auto crvData = mu2e::CRVDataDecoder(subevent); // reference
                            //const auto crvStatus = crvData.GetCRVROCStatusPacket(bl);
                            auto hits = crvData.GetCRVHits(bl);
                            for (auto& hit : hits) {
                                //if (newCanvas_) {
                                //    bookCanvas_();
                                //}
                                histograms_["channels"]->Fill(hit.first.febChannel); // one hist per FPGA?
                                histograms_["timestamps"]->Fill(hit.first.HitTime);
                                hist_updated = true;
                                //std::copy(toyPtr->dataBeginADCs(), toyPtr->dataBeginADCs() + total_adc_values, graphs_[fid]->GetY()); 
                                for(auto& adc : hit.second) {
                                    histograms_["adc"]->Fill(adc.ADC);
                                }
                            }
                            histograms_["nhits"]->Fill(hits.size());
                        }
                    } else {
                        // TODO increase a non-valid counter?
                    }
                }
            }
        }
        if(hist_updated) {
            auto currentTime = std::chrono::steady_clock::now();
            std::chrono::duration<double, std::milli> elapsed = currentTime - lastUpdate_;
            if(elapsed.count() >= onlineRefreshPeriod_) {
                for(size_t i = 1; i <= histograms_.size(); i++) {
                    _hCanvas->cd(i);
                    _hCanvas->Pad()->Modified();
                }
                _hCanvas->cd(0);
                _hCanvas->Update();
                gSystem->ProcessEvents(); 
                lastUpdate_ = currentTime;
            }
        }
    }
}

//-----------------------------------------------------------------------------
void CrvVstDemoViewer::beginRun(art::Run const& e) {
	if (e.run() == current_run_) return;
	current_run_ = e.run();

    _hCanvas->SetTitle(Form("Run %d", e.run()));


    /*
    if (graphs_.count(fid) == 0 || graphs_[fid] == nullptr ||
        static_cast<std::size_t>(graphs_[fid]->GetN()) != total_adc_values) {

      graphs_[fid] = new TGraph(total_adc_values);
      graphs_[fid]->SetName(Form("Fragment_%d_graph", fid));
      graphs_[fid]->SetLineColor(4);
      std::copy(x_.begin(), x_.end(), graphs_[fid]->GetX());
        
      _gCanvas->cd(ind + 1);
      graphs_[fid]->Draw("ALP");
    }*/

}

//-----------------------------------------------------------------------------
void CrvVstDemoViewer::endRun(art::Run const& e) {
    //std::cout << "EndRun, keep alive for 2min " << std::endl;
    //sleep(120);
	if (e.run() == current_run_) return;
	current_run_ = e.run();

  if (writeOutput_) {
		fFile_ = new TFile(outputFileName_.c_str(), "RECREATE");

    _hCanvas->Write("wf0", TObject::kOverwrite);
    if (_gCanvas != nullptr) {
      _gCanvas->Write("wf1", TObject::kOverwrite);
    }
    fFile_->Write();
  }
}

DEFINE_ART_MODULE(CrvVstDemoViewer)  // NOLINT(performance-unnecessary-value-param)
}  // namespace demo
