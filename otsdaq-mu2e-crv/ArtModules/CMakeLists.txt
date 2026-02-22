include(BuildPlugins) 

cet_build_plugin(CrvVstDemoViewer art::module LIBRARIES 
    REG 
    TRACE::TRACE 
    ROOT::Core 
    ROOT::Gui
    artdaq-core-mu2e::Data_dict
    artdaq-core-mu2e::Overlays
    artdaq-core-mu2e::Data
    artdaq::DAQdata
)

cet_build_plugin(CrvDQM art::module LIBRARIES REG
    art_root_io::TFileService_service
    artdaq-core-mu2e::Data
    otsdaq-mu2e::ArtModules
    otsdaq::NetworkUtilities
    ROOT::Hist
    ROOT::Tree
    ROOT::Core
    ROOT::RIO
    ROOT::Gui
    ROOT::RHTTP 
)

# OTS-based DQM
cet_build_plugin(CrvOtsDqm art::module LIBRARIES REG
    art_root_io::TFileService_service
    Offline::RecoDataProducts
    artdaq-core-mu2e::Data
    otsdaq-mu2e::ArtModules
    otsdaq::NetworkUtilities
    ROOT::Hist
    ROOT::Tree
    ROOT::Core
    ROOT::RIO
    ROOT::Gui
    ROOT::RHTTP
)

install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)
install_fhicl(SUBDIRS fcl)
