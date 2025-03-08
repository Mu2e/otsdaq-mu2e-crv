include(BuildPlugins) 

# cet_build_plugin(CrvDumper art::module LIBRARIES 
#     REG 
#     TRACE::TRACE 
#     artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
#     artdaq_core_mu2e::artdaq-core-mu2e_Overlays
#     artdaq_core_mu2e::artdaq-core-mu2e_Data
#     artdaq::DAQdata
# )

# CrvDumper 
cet_build_plugin(CrvPrint art::module LIBRARIES
    REG
    art_root_io::TFileService_service
    artdaq_core_mu2e::artdaq-core-mu2e_Data
    artdaq_core_mu2e::artdaq-core-mu2e_Overlays
    artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
    artdaq::DAQdata
)

# CrvDumper 
cet_build_plugin(CrvDumper art::module LIBRARIES
    REG
    art_root_io::TFileService_service
    artdaq_core_mu2e::artdaq-core-mu2e_Data
    artdaq_core_mu2e::artdaq-core-mu2e_Overlays
    artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
    artdaq::DAQdata
    # artdaq-core::artdaq-core_Data
    ROOT::Tree
)

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

# CrvDQM module with HTTP server and GUI components
# Is this correct? 
cet_build_plugin(CrvDQM art::module LIBRARIES 
    REG 
    TRACE::TRACE 
    art_root_io::TFileService_service
    artdaq_core_mu2e::artdaq-core-mu2e_Data
    artdaq_core_mu2e::artdaq-core-mu2e_Overlays
    artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
    artdaq::DAQdata
    # artdaq-core::artdaq-core_Data
    otsdaq_mu2e::otsdaq-mu2e_ArtModules
    otsdaq::NetworkUtilities
    ROOT::Hist
    ROOT::Tree
    ROOT::Core
    ROOT::RIO
    ROOT::Gui
    ROOT::RHTTP 
)

# cet_build_plugin(CrvDQMOld art::module LIBRARIES 
#     REG 
#     TRACE::TRACE 
#     art_root_io::TFileService_service
#     artdaq_core_mu2e::artdaq-core-mu2e_Data
#     artdaq_core_mu2e::artdaq-core-mu2e_Overlays
#     artdaq_core_mu2e::artdaq-core-mu2e_Data_dict
#     artdaq::DAQdata
#     # artdaq-core::artdaq-core_Data
#     otsdaq_mu2e::otsdaq-mu2e_ArtModules
#     otsdaq::NetworkUtilities
#     ROOT::Hist
#     ROOT::Tree
#     ROOT::Core
#     ROOT::RIO
#     ROOT::Gui
#     ROOT::RHTTP 
# )

install_headers(SUBDIRS detail)
install_source(SUBDIRS detail)
install_fhicl(SUBDIRS fcl)
