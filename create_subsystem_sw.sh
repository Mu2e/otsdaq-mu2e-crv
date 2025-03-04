#on mu2edaq01...
#$ ksu mu2edaq
#$ cd ~mu2edaq/sync_demo/ots/
#$ source setup_ots.sh

##
# CHANGE the following by HAND  !!!!!!!!!!!! (don't know how to get sed to handle
##
subsystem="CRV"
smallletters="crv"

#########
# create the directory based on the ostdaq demo
#########
cd ${MRB_SOURCE}
create_ots_repo.sh otsdaq_mu2e_${smallletters}

#########
# get the emulator/modified files from the calorimeter repository
#########
cd ${MRB_SOURCE}
mrb gitCheckout -d git_tmp ssh://p-mu2e-otsdaq-calorimeter@cdcvs.fnal.gov/cvs/projects/mu2e-otsdaq-calorimeter
cp -p git_tmp/otsdaq-mu2e-calorimeter/FEInterfaces/FEOtsEthernetProgramInterface_interface.cc otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
cp -p git_tmp/otsdaq-mu2e-calorimeter/FEInterfaces/FEOtsEthernetProgramInterface.h otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
cp -p git_tmp/otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterEmulator_interface.cc otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
cp -p git_tmp/otsdaq-mu2e-calorimeter/FEInterfaces/ROCCalorimeterEmulator.h otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
cd otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
sed "s/Calorimeter/${subsystem}/" ROCCalorimeterEmulator.h > ROC${subsystem}Emulator.h
sed "s/Calorimeter/${subsystem}/" ROCCalorimeterEmulator_interface.cc | sed "s/Calorimeter/${subsystem}/" | sed "s/calorimeter/${smallletters}/" > ROC${subsystem}Emulator_interface.cc


#######
# modify CMake files in order to compile
#######
cd $MRB_SOURCE
cp -p git_tmp/otsdaq-mu2e-calorimeter/FEInterfaces/CMakeLists.txt otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
cd $MRB_SOURCE/otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/
sed "s/Calorimeter/${subsystem}/" CMakeLists.txt > tmp.txt
mv tmp.txt CMakeLists.txt
cd $MRB_SOURCE/otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/
sed "s/\#add_subdirectory(FE/add_subdirectory(FE/" CMakeLists.txt > tmp.txt
mv tmp.txt CMakeLists.txt
cd $MRB_SOURCE
cp otsdaq_mu2e_calorimeter/ups/product_deps otsdaq_mu2e_${smallletters}/ups/
sed "s/calorimeter/${smallletters}/" otsdaq_mu2e_${smallletters}/ups/product_deps > tmp.txt
mv tmp.txt otsdaq_mu2e_${smallletters}/ups/product_deps

#######
# clean up
#######
cd $MRB_SOURCE
rm -rf git_tmp
rm otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/ROCCalorimeterEmulator.h
rm otsdaq_mu2e_${smallletters}/otsdaq-mu2e-${smallletters}/FEInterfaces/ROCCalorimeterEmulator_interface.cc

######
# copy stuff into a repository that can be updated
######
mv otsdaq_mu2e_${smallletters} otsdaq_mu2e_${smallletters}_tmp
mrb gitCheckout -d otsdaq_mu2e_${smallletters} ssh://p-mu2e-otsdaq-$smallletters@cdcvs.fnal.gov/cvs/projects/mu2e-otsdaq-$smallletters
cd otsdaq_mu2e_${smallletters}
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/Data .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/UserWebGUI .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/databases .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/doc .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/otsdaq-mu2e-${smallletters} .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/test .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/tools .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/ups .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/CMakeLists.txt .
cp -rpd ../otsdaq_mu2e_${smallletters}_tmp/LICENSE .

cd ${MRB_SOURCE}
rm -rf otsdaq_mu2e_${smallletters}_tmp

#cd ~mu2edaq/sync_demo/ots
#mrb z
#mrbsetenv
#mrb b
