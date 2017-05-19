## set suppress messages ##
if(SUPPRESS_MESSAGES)
    add_definitions(-DSUPPRESS_MESSAGES)
endif(SUPPRESS_MESSAGES)

## set verbose mode ##
if(VERBOSE)
    add_definitions(-DVERBOSE)
endif(VERBOSE)

## set boost fusion sizes ##
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFUSION_MAX_VECTOR_SIZE=${BOOST_FUSION_MAX_SIZE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DFUSION_MAX_MAP_SIZE=${BOOST_FUSION_MAX_SIZE}")

## structured grids ##
if(STRUCTURED_GRIDS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -DSTRUCTURED_GRIDS" )
else()
  set(ENABLE_CXX11 "ON" CACHE BOOL "Enable examples and tests featuring C++11 features" FORCE)
endif()

## enable cxx11 ##
if(ENABLE_CXX11)
    add_definitions(-DBOOST_RESULT_OF_USE_TR1 -DBOOST_NO_CXX11_DECLTYPE)
else()
    set(USE_MPI OFF)
endif()

## get boost ##
if(WIN32)
  # Auto-linking happens on Windows, so we don't need to specify specific components
  find_package( Boost 1.58 REQUIRED )
else()
  # On other platforms, me must be specific about which libs are required
  find_package( Boost 1.58 COMPONENTS timer system chrono REQUIRED )
endif()

if(Boost_FOUND)
    include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
    set(exe_LIBS "${Boost_LIBRARIES}" "${exe_LIBS}")
endif()

if(NOT USE_GPU)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mtune=native -march=native")
endif(NOT USE_GPU)

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ")

## gnu coverage flag ##
if(GNU_COVERAGE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
message (STATUS "Building executables for coverage tests")
endif()

## gnu profiling information ##
if(GNU_PROFILE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs")
message (STATUS "Building profiled executables")
endif()

## enable cxx11 and things ##
if ( ENABLE_CXX11 )
   message (STATUS "CXX11 enabled")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11")
else()
   message (STATUS "CXX11 disabled")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCXX11_DISABLE")
endif()

## cuda support ##
if( USE_GPU )
  message(STATUS "Using GPU")
  find_package(CUDA REQUIRED)
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-DCUDA_VERSION_MINOR=${CUDA_VERSION_MINOR}")
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-DCUDA_VERSION_MAJOR=${CUDA_VERSION_MAJOR}")
  string(REPLACE "." "" CUDA_VERSION ${CUDA_VERSION})
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-DCUDA_VERSION=${CUDA_VERSION}")
  set(CUDA_PROPAGATE_HOST_FLAGS ON)
  if( ${CUDA_VERSION} VERSION_GREATER "60")
      if (NOT ENABLE_CXX11 )
          set(GPU_SPECIFIC_FLAGS "-D_USE_GPU_ -DCXX11_DISABLE")
      else()
         set(GPU_SPECIFIC_FLAGS "-D_USE_GPU_ ")
      endif()
  else()
      error(STATUS "CUDA 6.0 or lower does not supported")
  endif()
  set( CUDA_ARCH "sm_35" CACHE STRING "Compute capability for CUDA" )

  include_directories(SYSTEM ${CUDA_INCLUDE_DIRS})

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_USE_GPU_")
  set(exe_LIBS  ${exe_LIBS} ${CUDA_CUDART_LIBRARY} )
  set (CUDA_LIBRARIES "")
  # adding the additional nvcc flags
  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}" "-arch=${CUDA_ARCH}" "-Xcudafe" "--diag_suppress=dupl_calling_convention")
  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}" "-Xcudafe" "--diag_suppress=code_is_unreachable" "-Xcudafe")
  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}" "--diag_suppress=implicit_return_from_non_void_function" "-Xcudafe")
  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}" "--diag_suppress=calling_convention_not_allowed" "-Xcudafe")
  set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS}" "--diag_suppress=conflicting_calling_conventions")

  if ("${CUDA_HOST_COMPILER}" MATCHES "(C|c?)lang")
    set(CUDA_NVCC_FLAGS "${CUDA_NVCC_FLAGS} ${NVCC_CLANG_SPECIFIC_OPTIONS}")
  endif()

else()
  set (CUDA_LIBRARIES "")
endif()

## clang ##
if((CUDA_HOST_COMPILER MATCHES "(C|c?)lang") OR (CMAKE_CXX_COMPILER_ID MATCHES "(C|c?)lang"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth-1024")
endif()

Find_Package( OpenMP )


## openmp ##
if(OPENMP_FOUND)
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}" )
    set( PAPI_WRAP_LIBRARY "OFF" CACHE BOOL "If on, the papi-wrap library is compiled with the project" )
else()
    set( ENABLE_PERFORMANCE_METERS "OFF" CACHE BOOL "If on, meters will be reported for each stencil" )
endif()

## performance meters ##
if(ENABLE_PERFORMANCE_METERS)
    add_definitions(-DENABLE_METERS)
endif(ENABLE_PERFORMANCE_METERS)

# always use fopenmp and lpthread as cc/ld flags
# be careful! deleting this flags impacts performance
# (even on single core and without pragmas).
set ( exe_LIBS -lpthread ${exe_LIBS} )

## papi wrapper ##
if ( PAPI_WRAP_LIBRARY )
  find_package(PapiWrap)
  if ( PAPI_WRAP_FOUND )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PAPI_WRAP" )
    set( PAPI_WRAP_MODULE "ON" )
    include_directories( "${PAPI_WRAP_INCLUDE_DIRS}" )
    set ( exe_LIBS "${exe_LIBS}" "${PAPI_WRAP_LIBRARIES}" )
  else()
    message ("papi-wrap not found. Please set PAPI_WRAP_PREFIX to the root path of the papi-wrap library. papi-wrap not used!")
  endif()
endif()

## papi ##
if(USE_PAPI)
  find_package(PAPI REQUIRED)
  if(PAPI_FOUND)
    include_directories( "${PAPI_INCLUDE_DIRS}" )
    set ( exe_LIBS "${exe_LIBS}" "${PAPI_LIBRARIES}" )
    set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_PAPI" )
  else()
    message("PAPI library not found. set the PAPI_PREFIX")
  endif()
endif()

## precision ##
if(SINGLE_PRECISION)
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-DFLOAT_PRECISION=4")
  message(STATUS "Computations in single precision")
else()
  set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS} "-DFLOAT_PRECISION=8")
  message(STATUS "Computations in double precision")
endif()

## gcl ##
if( "${GCL_GPU}" STREQUAL "ON" )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GCL_GPU_")
else()
  set (CUDA_LIBRARIES "")
endif()

## mpi ##
if( USE_MPI )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GCL_MPI_")
  if( USE_MPI_COMPILER )
    find_package(MPI REQUIRED)
    include_directories( "${MPI_CXX_INCLUDE_PATH}" )
    set( exe_LIBS  ${MPI_CXX_LIBRARIES} ${exe_LIBS} )
  endif()
endif()

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile WORKING_DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR} COMMENT "Generating API documentation with Doxygen" VERBATIM)
endif()

## test script generator ##
file(WRITE ${TEST_SCRIPT} "#!/bin/sh\n")
file(APPEND ${TEST_SCRIPT} "hostname\n")
file(APPEND ${TEST_SCRIPT} "res=0\n")
function(gridtools_add_test test_name test_script test_exec)
  file(APPEND ${test_script} "${test_exec}" " ${ARGN}" "\n")
  file(APPEND ${test_script} "res=$((res || $? ))\n")
endfunction(gridtools_add_test)

## test script generator for MPI tests ##
file(WRITE ${TEST_MPI_SCRIPT} "res=0\n")
function(gridtools_add_mpi_test test_name test_exec)
  file(APPEND ${TEST_MPI_SCRIPT} "\$LAUNCH_MPI_TEST ${test_exec}" " ${ARGN}" "\n")
  file(APPEND ${TEST_MPI_SCRIPT} "res=$((res || $? ))\n")
endfunction(gridtools_add_mpi_test)

file(WRITE ${TEST_CUDA_MPI_SCRIPT} "res=0\n")
function(gridtools_add_cuda_mpi_test test_name test_exec)
  file(APPEND ${TEST_CUDA_MPI_SCRIPT} "\$LAUNCH_MPI_TEST ${test_exec}" " ${ARGN}" "\n")
  file(APPEND ${TEST_CUDA_MPI_SCRIPT} "res=$((res || $? ))\n")
endfunction(gridtools_add_cuda_mpi_test)

## caching ##
if( NOT ENABLE_CACHING )
    add_definitions( -D__DISABLE_CACHING__ )
endif()

add_definitions(-DGTEST_COLOR )
include_directories( ${GTEST_INCLUDE_DIR} )
include_directories( ${GMOCK_INCLUDE_DIR} )
