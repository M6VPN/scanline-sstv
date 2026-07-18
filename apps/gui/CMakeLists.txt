# Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
# scanline-sstv/apps/gui/CMakeLists.txt
# SPDX-License-Identifier: GPL-3.0-or-later

qt_standard_project_setup(REQUIRES 6.5)

set(SCANLINE_SSTV_QML_URI "org.m6vpn.ScanlineSSTV")

set(SSTV_GUI_MODEL_SOURCES
	audio_diagnostics_model.cpp
	prepared_image_provider.cpp
	tx_editor_model.cpp
)
set(SSTV_GUI_QML_FILES Main.qml)
if(SSTV_ENABLE_LIVE_TX)
	list(APPEND SSTV_GUI_MODEL_SOURCES live_transmit_model.cpp)
	list(APPEND SSTV_GUI_QML_FILES LiveTransmitPanel.qml)
endif()

add_library(sstv_gui_model STATIC ${SSTV_GUI_MODEL_SOURCES})
target_link_libraries(
	sstv_gui_model
	PUBLIC
		sstv::app
		sstv::audio
		Qt6::Core
		Qt6::Gui
		Qt6::Quick
	PRIVATE
		sstv_project_options
		sstv_project_warnings
		Qt6::Concurrent
)
target_include_directories(sstv_gui_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(SSTV_ENABLE_LIVE_TX)
	target_compile_definitions(sstv_gui_model PUBLIC SSTV_ENABLE_LIVE_TX=1)
endif()

qt_add_executable(scanline-sstv-gui main.cpp)

qt_add_qml_module(
	scanline-sstv-gui
	URI ${SCANLINE_SSTV_QML_URI}
	VERSION 1.0
	QML_FILES ${SSTV_GUI_QML_FILES}
)

target_compile_definitions(
	scanline-sstv-gui
	PRIVATE SCANLINE_SSTV_QML_URI=\"${SCANLINE_SSTV_QML_URI}\"
)
if(SSTV_ENABLE_LIVE_TX)
	target_compile_definitions(scanline-sstv-gui PRIVATE SSTV_ENABLE_LIVE_TX=1)
endif()

target_link_libraries(
	scanline-sstv-gui
	PRIVATE
		sstv_gui_model
		sstv_project_options
		sstv_project_warnings
		Qt6::Core
		Qt6::Gui
		Qt6::Qml
		Qt6::Quick
		Qt6::QuickControls2
)

if(BUILD_TESTING)
	find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
	qt_add_executable(scanline-sstv-gui-model-tests tx_editor_model_tests.cpp)
	target_link_libraries(
		scanline-sstv-gui-model-tests
		PRIVATE
			sstv_gui_model
			sstv_project_options
			sstv_project_warnings
			Qt6::Test
	)
	target_compile_definitions(
		scanline-sstv-gui-model-tests
		PRIVATE SSTV_IMAGE_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/image"
	)
	add_test(
		NAME scanline-sstv-gui-model-tests
		COMMAND $<TARGET_FILE:scanline-sstv-gui-model-tests>
	)
	set_tests_properties(
		scanline-sstv-gui-model-tests
		PROPERTIES
			ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
			TIMEOUT 60
	)
	qt_add_executable(
		scanline-sstv-audio-diagnostics-model-tests
		audio_diagnostics_model_tests.cpp
	)
	target_link_libraries(
		scanline-sstv-audio-diagnostics-model-tests
		PRIVATE
			sstv_gui_model
			sstv_project_options
			sstv_project_warnings
			Qt6::Test
	)
	add_test(
		NAME scanline-sstv-audio-diagnostics-model-tests
		COMMAND $<TARGET_FILE:scanline-sstv-audio-diagnostics-model-tests>
	)
	set_tests_properties(
		scanline-sstv-audio-diagnostics-model-tests
		PROPERTIES
			ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
			TIMEOUT 15
	)
	add_test(
		NAME scanline-sstv-gui-smoke
		COMMAND $<TARGET_FILE:scanline-sstv-gui> --smoke-test
	)
	set_tests_properties(
		scanline-sstv-gui-smoke
		PROPERTIES
			ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
			TIMEOUT 10
	)
	if(SSTV_ENABLE_LIVE_TX)
		qt_add_executable(scanline-sstv-live-transmit-model-tests
			live_transmit_model_tests.cpp)
		target_link_libraries(scanline-sstv-live-transmit-model-tests
			PRIVATE sstv_gui_model sstv_project_options sstv_project_warnings Qt6::Test)
		target_compile_definitions(scanline-sstv-live-transmit-model-tests
			PRIVATE SSTV_IMAGE_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/image")
		add_test(NAME scanline-sstv-live-transmit-model-tests
			COMMAND $<TARGET_FILE:scanline-sstv-live-transmit-model-tests>)
		set_tests_properties(scanline-sstv-live-transmit-model-tests PROPERTIES
			ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
			LABELS "live-tx-mock;audio-concurrency" TIMEOUT 30)
	endif()
endif()

install(TARGETS scanline-sstv-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
