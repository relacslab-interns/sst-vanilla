# SST simulator 

## Prerequisite packages
1. Update packages and REBOOT
```
sudo apt update
sudo apt upgrade
```

2. Install packages
```
sudo apt install libtool-bin build-essential cmake patch uuid-dev python3-dev autoconf automake autotools-dev curl python3 python3-pip libmpc-dev libmpfr-dev libgmp-dev gawk bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev ninja-build git cmake libglib2.0-dev
```

## Build codes
1. Download codes
```
git clone [url]
export SST_HOME=<repo directory>

cd $SST_HOME

wget https://download.open-mpi.org/release/open-mpi/v4.0/openmpi-4.0.5.tar.gz
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
tar -xzvf openmpi-4.0.5.tar.gz
tar -xzvf pin-3.22-98547-g7a303a835-gcc-linux.tar.gz
```

2. Build OpenMPI 4.0.5 (strongly recommended by SST)
```
export MPIHOME=$SST_HOME/install/openmpi-4.0.5

mkdir -p $SST_HOME/build/openmpi-4.0.5
cd $SST_HOME/build/openmpi-4.0.5
../../openmpi-4.0.5/configure --prefix=$MPIHOME
make all
make install

export PATH=$MPIHOME/bin:$PATH
export MPICC=$MPIHOME/bin/mpicc
export MPICXX=$MPIHOME/bin/mpic++
export LD_LIBRARY_PATH=$MPIHOME/lib:$LD_LIBRARY_PATH
export MANPATH=$MPIHOME/share/man:$MANPATH
export PMIX_MCA_gds=hash
```

3. Build SST-core
```
cd $SST_HOME/sst-core
./autogen.sh
mkdir -p $SST_HOME/build/sst-core
cd $SST_HOME/build/sst-core
../../sst-core/configure --prefix=$SST_HOME/install/sst-core CC=<which gcc> CXX=<which g++> MPICC=$MPICC MPICXX=$MPICXX
make
make install
```

4. Build SST-elements (step-3 must be done)
```
export INTEL_PIN_DIRECTORY=$SST_HOME/pin-3.22-98547-g7a303a835-gcc-linux

cd $SST_HOME/sst-elements
./autogen.sh
mkdir -p $SST_HOME/build/sst-elements
cd $SST_HOME/build/sst-elements
../../sst-elements/configure --prefix=$SST_HOME/install/sst-elements --with-sst-core=$SST_HOME/install/sst-core --with-pin=$INTEL_PIN_DIRECTORY
make
make install
```

## Run 
1. Setup SST commands
```
export PATH=$SST_HOME/install/sst-core/bin:$PATH
export PATH=$SST_HOME/install/sst-elements/bin:$PATH
export PATH=$SST_HOME/install/sst-macro/bin:$PATH

// Test basic sst commands
sst --version
sst-info
sst-test-core
sst-test-elements // NOTE: sst-macro test will fail due to wrong base path -> 'make installcheck' and 'make check' are testable in 'build/sst-macro'
```

2. Run example (pick a configuration file to run (sst [options] <YOUR PYTHON CONFIG>))
```
// Simple in-order CPU run
cd $SST_HOME
sst -v sst-elements/src/sst/elements/miranda/tests/inorderstream.py

// Run a single-node far-memory system
// it needs to uncomment lines in elements/Samba/Samba.cc noted as `hklee`
cd $SST_HOME/sst-elements/src/sst/elements/Opal/tests/app
make all
cd ..
sst -v basic_1node_1smp.py
```

Notes: Setup environmental variables in case of running or building in new terminal
```
export SST_HOME=<repo root directory>
export MPIHOME=$SST_HOME/install/openmpi-4.0.5
export INTEL_PIN_DIRECTORY=$SST_HOME/pin-3.22-98547-g7a303a835-gcc-linux

export MPICC=$MPIHOME/bin/mpicc
export MPICXX=$MPIHOME/bin/mpic++
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

export LD_LIBRARY_PATH=$MPIHOME/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$SST_HOME/install/sst-macro/lib:$LD_LIBRARY_PATH 
export MANPATH=$MPIHOME/share/man:$MANPATH
export PMIX_MCA_gds=hash

export PATH=$MPIHOME/bin:$PATH
export PATH=$SST_HOME/install/sst-core/bin:$PATH
export PATH=$SST_HOME/install/sst-elements/bin:$PATH
export PATH=$SST_HOME/install/sst-macro/bin:$PATH
```


## Useful links

* [SST all document](http://sst-simulator.org/SSTPages/SSTMainDocumentation/)
* [Detailed build instructions](http://sst-simulator.org/SSTPages/SSTBuildAndInstall_12dot0dot1_SeriesDetailedBuildInstructions/)
* [Additional external components](http://sst-simulator.org/SSTPages/SSTBuildAndInstall_12dot0dot1_SeriesAdditionalExternalComponents/)
* [How to run examples](http://sst-simulator.org/SSTPages/SSTUserHowToRunSST/)
* [SST core document](http://sst-simulator.org/SSTDoxygen/12.0.1_docs/html/)
* [Element description](http://sst-simulator.org/SSTPages/SSTDeveloperElementSummaryInfo/)
* [Naming conventions](http://sst-simulator.org/SSTPages/SSTDeveloper_SSTNamingConventions/)
* [Python modules (SST11)](http://sst-simulator.org/SSTPages/SSTDeveloper_11dotx_PythonModule/)
