PROJECT(aird)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(CMAKE_BUILD_TYPE release)

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
                     )

INSTALL(TARGETS aird
        RUNTIME DESTINATION bin)

INSTALL(FILES etc/aird.cfg
        DESTINATION /etc
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ)
