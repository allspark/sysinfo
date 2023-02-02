add_library(sysinfo STATIC)
add_library(wormhole::sysinfo ALIAS sysinfo)

CompilerWarningsAsError(sysinfo)

set(headers
        include/wormhole/sysinfo/errno_error.hpp
        include/wormhole/sysinfo/helper.hpp
        include/wormhole/sysinfo/NetlinkSocket.hpp
        include/wormhole/sysinfo/NetlinkSocketError.hpp
        include/wormhole/sysinfo/types.hpp
        )

set(sources
        errno_error.cpp
        NetlinkSocket.cpp
        NetlinkSocketError.cpp
        types.cpp
        )

target_sources(sysinfo PRIVATE ${headers} ${sources})

target_include_directories(sysinfo PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(sysinfo PUBLIC fmt::fmt)

install(TARGETS sysinfo
        EXPORT ${CMAKE_PROJECT_NAME}Targets
        ARCHIVE
        COMPONENT bin
        COMPONENT dev
        )

install(DIRECTORY include/wormhole DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

