
include(otsdaq::FEInterface)

cet_build_plugin(ROCCosmicRayVetoInterface otsdaq::FEInterface LIBRARIES REG otsdaq-mu2e::ROCCore
 )

# Hardware-free FE that runs the PoE injector script on the DCS host (CRV page power button)
cet_build_plugin(CRVPoEControlInterface otsdaq::FEInterface LIBRARIES REG otsdaq::FECore
 )


install_headers()
install_source()
