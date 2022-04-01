cmake_minimum_required(VERSION 3.10)
PROJECT(tsndemo)

# setup version
set(TSNDEMO_MAJOR_VERSION 0)
set(TSNDEMO_MINOR_VERSION 0)
set(TSNDEMO_MICRO_VERSION 1)
set(TSNDEMO_VERSION ${TSNDEMO_MAJOR_VERSION}.${TSNDEMO_MINOR_VERSION}.${TSNDEMO_MICRO_VERSION})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

# set default build typ if not specified by user
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE debug)
endif()
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)


# include custom Modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMakeModules/")
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/inc)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/lib)

find_package(LibXml2 REQUIRED)
include_directories(${LIBXML2_INCLUDE_DIR})
LINK_DIRECTORIES(${LIBXML2_LIBRARIES})

find_package(sysrepo REQUIRED)
include_directories(${SYSREPO_INCLUDE_DIR})
LINK_DIRECTORIES(${SYSREPO_LIBRARIES})

find_package (LibNL REQUIRED)
include_directories(${LibNL_INCLUDE_DIR})
LINK_DIRECTORIES(${LibNL_LIBRARIES})

find_package (LibYANG REQUIRED)
include_directories(${LIBYANG_INCLUDE_DIRS})
LINK_DIRECTORIES(${LIBYANG_LIBRARIES})

find_package (CJSON REQUIRED)
include_directories(${CJSON_INCLUDE_DIR})
LINK_DIRECTORIES(${CJSON_LIBRARIES})

SET(SRC_LIST
        lib/util.c
        lib/svec.c
        lib/dynamic-string.c
        lib/hash.c
        lib/hmap.c
        lib/shash.c
        lib/sset.c
        lib/log.c
        src/utils.c
        src/lldp.c
        src/interface.c
        src/hardware.c
        src/bridge.c
        src/repo.c
        src/main.c)

ADD_EXECUTABLE(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} ${LIBXML2_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SYSREPO_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${LibNL_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${LIBYANG_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${CJSON_LIBRARIES})
