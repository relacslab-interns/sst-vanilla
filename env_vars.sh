export SST_HOME=
export APP_HOME=
export MPIHOME=$SST_HOME/install/openmpi-4.0.5
export INTEL_PIN_DIRECTORY=$SST_HOME/pin-3.22-98547-g7a303a835-gcc-linux

export MPICC=$MPIHOME/bin/mpicc
export MPICXX=$MPIHOME/bin/mpic++
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

export LD_LIBRARY_PATH="$MPIHOME/lib:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="$SST_HOME/install/sst-macro/lib:$LD_LIBRARY_PATH"
export DYLD_LIBRARY_PATH="$MPIHOME/lib:$DYLD_LIBRARY_PATH"
export MANPATH="$MPIHOME/share/man:$DYLD_LIBRARY_PATH"
export PMIX_MCA_gds=hash

export PATH="$MPIHOME/bin:$PATH"
export PATH="$SST_HOME/install/sst-core/bin:$PATH"
export PATH="$SST_HOME/install/sst-elements/bin:$PATH"
export PATH="$SST_HOME/install/sst-macro/bin:$PATH"
