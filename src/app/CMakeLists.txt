# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/src/app/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

set(SSTV_APP_SOURCES)
set(SSTV_APP_PUBLIC_LIBRARIES sstv_project_options)
if(SSTV_BUILD_IMAGE)
	list(APPEND SSTV_APP_SOURCES offline_tx_editor.cpp)
	list(APPEND SSTV_APP_PUBLIC_LIBRARIES sstv::image sstv::core)
endif()
if(SSTV_BUILD_AUDIO AND SSTV_BUILD_RIG)
	list(APPEND SSTV_APP_SOURCES rendered_transmit.cpp transmit_coordinator.cpp)
	list(APPEND SSTV_APP_PUBLIC_LIBRARIES sstv::audio sstv::core sstv::rig)
endif()
if(SSTV_ENABLE_LIVE_TX)
	list(APPEND SSTV_APP_SOURCES live_transmit.cpp)
endif()

add_library(sstv_app STATIC ${SSTV_APP_SOURCES})
add_library(sstv::app ALIAS sstv_app)
target_link_libraries(
	sstv_app
	PUBLIC ${SSTV_APP_PUBLIC_LIBRARIES}
	PRIVATE sstv_project_warnings
)
target_include_directories(
	sstv_app PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
