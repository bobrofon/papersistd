# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.17)
project(papersistd LANGUAGES CXX)

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(pulse REQUIRED IMPORTED_TARGET libpulse)

add_executable(${PROJECT_NAME} main.cpp)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wextra
	-Werror
	-pedantic
	-pedantic-errors
)
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::pulse)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE -s)

configure_file(papersistd.service.in papersistd.service)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/papersistd.service
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/systemd/user)
