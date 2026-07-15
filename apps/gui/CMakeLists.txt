# SPDX-License-Identifier: GPL-3.0-or-later

qt_standard_project_setup(REQUIRES 6.5)

set(SCANLINE_SSTV_QML_URI "org.m6vpn.ScanlineSSTV")

qt_add_executable(scanline-sstv-gui main.cpp)

qt_add_qml_module(
	scanline-sstv-gui
	URI ${SCANLINE_SSTV_QML_URI}
	VERSION 1.0
	QML_FILES Main.qml
)

target_compile_definitions(
	scanline-sstv-gui
	PRIVATE SCANLINE_SSTV_QML_URI=\"${SCANLINE_SSTV_QML_URI}\"
)

target_link_libraries(
	scanline-sstv-gui
	PRIVATE
		sstv::core
		sstv_project_options
		sstv_project_warnings
		Qt6::Core
		Qt6::Gui
		Qt6::Qml
		Qt6::Quick
		Qt6::QuickControls2
)

if(BUILD_TESTING)
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
endif()

install(TARGETS scanline-sstv-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
