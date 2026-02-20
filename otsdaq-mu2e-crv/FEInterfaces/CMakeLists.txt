
include(otsdaq::FEInterface)

cet_build_plugin(ROCCosmicRayVetoInterface otsdaq::FEInterface LIBRARIES REG otsdaq-mu2e::ROCCore
 )


install_headers()
install_source()
