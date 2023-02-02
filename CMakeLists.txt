cmake_minimum_required(VERSION 3.24)
project(wormsysinfo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CompilerOptions.cmake)

find_package(fmt REQUIRED)

include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(example)

include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake" COMPATIBILITY SameMajorVersion)

install(EXPORT ${CMAKE_PROJECT_NAME}Targets
        NAMESPACE ${CMAKE_PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
        COMPONENT dev)

install(FILES cmake/${CMAKE_PROJECT_NAME}Config.cmake "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}
        COMPONENT dev
        )
