cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/AppCUI/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

message("CMAKE_TOOLCHAIN_FILE => ${CMAKE_TOOLCHAIN_FILE}")

project(GView VERSION 1.0)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)

if( supported )
	message(STATUS "${PROJECT_NAME} => IPO / LTO enabled")
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	message(STATUS "${PROJECT_NAME} => IPO / LTO not supported: <${error}>")
endif()

# versioning
file(READ "GViewCore/include/GView.hpp" GView_content)
string(REGEX MATCH "#define GVIEW_VERSION \"([0-9]+.[0-9]+.[0-9]+)\"" _ ${GView_content})
set(GVIEW_VERSION ${CMAKE_MATCH_1})
message("${PROJECT_NAME} version: ${GVIEW_VERSION}")

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING 
	    "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." 
	    FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
     set(DEBUG_BUILD 1)
endif()

if (NOT MSVC)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin/${CMAKE_BUILD_TYPE}")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin/${CMAKE_BUILD_TYPE}")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/../bin/${CMAKE_BUILD_TYPE}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/../bin/${CMAKE_BUILD_TYPE}")

if (MSVC)
    add_definitions(-DBUILD_FOR_WINDOWS)
    add_compile_options(-W3 /MP)
    
elseif (APPLE)
    add_definitions(-DBUILD_FOR_OSX)
    if (DEBUG_BUILD)
        add_compile_options(-g)
        add_compile_options(-W)
    endif()
   
    SET(CMAKE_INSTALL_NAME_DIR @executable_path)
    message(STATUS "CMAKE_INSTALL_NAME_DIR => ${CMAKE_INSTALL_NAME_DIR}")
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    SET(CMAKE_INSTALL_RPATH @executable_path)
    message(STATUS "CMAKE_INSTALL_RPATH => ${CMAKE_INSTALL_RPATH}")
    
    if(CMAKE_LINK_LIBRARY_USING_weak_library_SUPPORTED)
        message(STATUS "${PROJECT_NAME} => CMAKE_LINK_LIBRARY_USING_weak_library_SUPPORTED")
    	target_link_libraries(${PROJECT_NAME} PRIVATE "$<LINK_LIBRARY:weak_library,lib,external>")
    else()
        message(STATUS "${PROJECT_NAME} => NOT CMAKE_LINK_LIBRARY_USING_weak_library_SUPPORTED")
    	# target_link_libraries(${PROJECT_NAME} PRIVATE lib external)
    endif()
    
    # install(TARGETS ${CURSES_LIBRARIES} LIBRARY DESTINATION "." NAMELINK_ONLY)    
elseif (UNIX)
    add_definitions(-DBUILD_FOR_UNIX)
    add_compile_options(-g)
    add_compile_options(-W)
    
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath -Wl,$ORIGIN")
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
     add_compile_options(-Wall -Wextra
             -Wno-switch
             -Wpointer-arith
             -Wcast-qual
             -Wredundant-decls
             -Wctor-dtor-privacy
             -Wwrite-strings
             -Wdisabled-optimization
             -Wzero-as-null-pointer-constant
             -Wvla) # -pedantic
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.0)
    add_compile_options(-Wtrampolines -Wduplicated-cond -Wduplicated-branches -Wnull-dereference)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(CXX_FILESYSTEM_LIBRARIES "stdc++fs")
endif()


if (NOT DEBUG_BUILD)
    if (MSVC)
        add_compile_options(-Ob2)
    else()
        add_compile_options(-O3)
    endif()
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(3rdPartyLibs/LLVMDemangle)

add_subdirectory(AppCUI)
add_subdirectory(GViewCore)
add_subdirectory(GView)

# Types supported by GView
add_subdirectory(Types/PE)
add_subdirectory(Types/ICO)
add_subdirectory(Types/CSV)
add_subdirectory(Types/BMP)
add_subdirectory(Types/MACHO)
add_subdirectory(Types/ISO)
add_subdirectory(Types/PREFETCH)
add_subdirectory(Types/MAM)
add_subdirectory(Types/LNK)
add_subdirectory(Types/JOB)
add_subdirectory(Types/PCAP)
add_subdirectory(Types/ELF)
add_subdirectory(Types/JT)
add_subdirectory(Types/INI)
add_subdirectory(Types/CPP)
add_subdirectory(Types/JSON)
add_subdirectory(Types/VBA)
add_subdirectory(Types/JS)
add_subdirectory(Types/PYEXTRACTOR)

# Generic plugins supported by GView
add_subdirectory(GenericPlugins/CharacterTable)
add_subdirectory(GenericPlugins/Hashes)
                                                                
if (APPLE)
    	set_property(TARGET "${PROJECT_NAME}" PROPERTY INSTALL_RPATH "@loader_path")
elseif (UNIX)
	set_property(TARGET "${PROJECT_NAME}" PROPERTY INSTALL_RPATH "$ORIGIN")
endif()
