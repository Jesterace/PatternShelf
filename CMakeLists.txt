cmake_minimum_required(VERSION 3.20)

project(JesterPatternShelf VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(jesterpatternshelf
    src/main.cpp
)

target_link_libraries(jesterpatternshelf PRIVATE Qt6::Widgets)

install(TARGETS jesterpatternshelf
    RUNTIME DESTINATION bin
)

install(FILES packaging/jesterpatternshelf.desktop
    DESTINATION share/applications
)
