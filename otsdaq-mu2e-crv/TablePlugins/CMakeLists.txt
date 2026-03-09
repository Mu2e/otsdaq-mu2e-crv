include(otsdaq::table)

cet_build_plugin(SubsystemCRVRocTable otsdaq::table LIBRARIES REG
	otsdaq::SlowControlsTableBase
	otsdaq::XDAQContextTable
	Offline::DataProducts
  )



install_headers()
install_source()
