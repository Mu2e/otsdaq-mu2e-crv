include(BuildPlugins) # Art Module

cet_build_plugin(CrvVstDemoViewer art::module LIBRARIES 
    REG 
    TRACE::TRACE 
    ROOT::Core 
    ROOT::Gui
    artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
    artdaq_core_mu2e::artdaq-core-mu2e_Overlays
    artdaq_core_mu2e::artdaq-core-mu2e_Data
    artdaq::DAQdata
)


install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)
install_fhicl(SUBDIRS fcl)
