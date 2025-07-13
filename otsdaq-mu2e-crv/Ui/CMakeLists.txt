#
# include_directories($ENV{ROOTSYS}/include)
# link_directories($ENV{ROOTSYS}/lib)
#
find_package(otsdaq-mu2e-tracker REQUIRED)
#
#include_directories(SYSTEM
#    $ENV{OTSDAQ_MU2E_TRACKER_INC}
#)

cet_make_library(
        SOURCE          
        DtcInterface.cc #DtcInterface_print.cc
        LIBRARIES PUBLIC
	    #otsdaq-mu2e-tracker::otsdaq-mu2e-tracker_Ui
        otsdaq-mu2e-tracker::Ui
        artdaq::DAQdata
        # artdaq-core-demo::artdaq-core-demo_Overlays
        cetlib_except::cetlib_except
        mu2e-pcie-utils::CFOInterface
        mu2e-pcie-utils::DTCInterface
        ROOT::Core
        )

cet_add_compiler_flags(-g -O0)

install_headers()
install_source()
