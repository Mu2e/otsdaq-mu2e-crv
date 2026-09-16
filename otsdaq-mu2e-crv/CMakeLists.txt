add_subdirectory(FEInterfaces)
add_subdirectory(ArtModules)
add_subdirectory(TablePlugins)

# Online configuration fragments (CrvDQM_online.fcl) on FHICL_FILE_PATH.
install_fhicl(SUBDIRS fcl)
