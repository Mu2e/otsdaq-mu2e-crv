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

cet_build_plugin(CrvEventFilter art::module LIBRARIES REG
    artdaq-core-mu2e::Overlays
    artdaq-core-mu2e::Data
    artdaq::DAQdata
)

install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)
install_fhicl(SUBDIRS fcl)
