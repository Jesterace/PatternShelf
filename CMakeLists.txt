cmake_minimum_required(VERSION 3.20)

project(PatternShelf VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(patternshelf
    src/main.cpp
)

target_link_libraries(patternshelf PRIVATE Qt6::Widgets)

install(TARGETS patternshelf
    RUNTIME DESTINATION bin
)

install(FILES packaging/patternshelf.desktop
    DESTINATION share/applications
)

install(FILES packaging/patternshelf.svg
    DESTINATION share/icons/hicolor/scalable/apps
)
