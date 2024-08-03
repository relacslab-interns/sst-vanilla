if (NOT TARGET SST::SSTMacro)
  add_library(SST::SSTMacro IMPORTED UNKNOWN)

  set_target_properties(SST::SSTMacro PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "/home/grvosku/releases/12.1/build/sst-macro/include;/home/grvosku/releases/12.1/build/sst-macro/include/sprockit"
    INTERFACE_COMPILE_FEATURES cxx_std_11
    INSTALL_RPATH /home/grvosku/releases/12.1/build/sst-macro/lib
  )
  if (APPLE)
    set_target_properties(SST::SSTMacro PROPERTIES
      IMPORTED_LOCATION /home/grvosku/releases/12.1/build/sst-macro/lib/libsstmac.dylib
    )
  else()
    set_target_properties(SST::SSTMacro PROPERTIES
      IMPORTED_LOCATION /home/grvosku/releases/12.1/build/sst-macro/lib/libsstmac.so
    )
  endif()
endif()
