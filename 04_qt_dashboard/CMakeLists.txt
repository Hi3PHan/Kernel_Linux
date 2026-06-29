cmake_minimum_required(VERSION 3.16)

project(qt_dashboard LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(qt_dashboard
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/components/ActivityLogger.cpp
    src/components/ActivityLogger.h
    src/components/CommandRunner.cpp
    src/components/CommandRunner.h
    src/components/FileExplorer.cpp
    src/components/FileExplorer.h
    src/components/HeaderBar.cpp
    src/components/HeaderBar.h
    src/components/ProcessRegistry.cpp
    src/components/ProcessRegistry.h
    src/components/Sidebar.cpp
    src/components/Sidebar.h
    src/components/SimpleFormDialog.cpp
    src/components/SimpleFormDialog.h
    src/pages/ActivityLogPage.cpp
    src/pages/ActivityLogPage.h
    src/pages/PacketCapturePage.cpp
    src/pages/PacketCapturePage.h
    src/pages/ProcessNetworkPage.cpp
    src/pages/ProcessNetworkPage.h
    src/pages/ShellAutomationPage.cpp
    src/pages/ShellAutomationPage.h
    src/pages/SystemMonitorPage.cpp
    src/pages/SystemMonitorPage.h
)

target_link_libraries(qt_dashboard PRIVATE Qt6::Widgets Qt6::Network)
target_compile_definitions(qt_dashboard PRIVATE
    LAB_REPO_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

add_custom_command(TARGET qt_dashboard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:qt_dashboard>/resources
)
