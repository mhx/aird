PROJECT(aird)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(CMAKE_BUILD_TYPE release)

INCLUDE(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)

IF(COMPILER_SUPPORTS_CXX11)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
ELSEIF(COMPILER_SUPPORTS_CXX0X)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
ELSE()
    MESSAGE(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
ENDIF()

FIND_PACKAGE(Boost COMPONENTS
             date_time
             filesystem
             program_options
             system
            )

ADD_EXECUTABLE(aird
               src/main
               src/event_device
               src/event_source
               src/log
               src/monitor
               src/mouse_device
               src/server
               src/settings
              )

TARGET_LINK_LIBRARIES(aird
                      ${Boost_LIBRARIES}
                      pthread
                     )

INSTALL(TARGETS aird
        RUNTIME DESTINATION bin)

INSTALL(FILES etc/aird.cfg
        DESTINATION /etc
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
