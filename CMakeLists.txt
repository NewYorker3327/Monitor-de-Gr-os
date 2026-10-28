cmake_minimum_required(VERSION 3.16)
project(GrainMonitor_UI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ativa o processamento do Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Encontra as bibliotecas visuais do Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Sql Charts Network)

qt6_standard_project_setup()

# Compila o executável
qt6_add_executable(GrainMonitor
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
    silo2dwidget.cpp
    silo2dwidget.h
    databasemanager.cpp
    databasemanager.h
    networkmanager.cpp
    networkmanager.h
    siloglwidget.cpp
    siloglwidget.h
    thememanager.cpp
    sensorwidget.cpp
)

# Linka todas as bibliotecas necessárias do Qt6 e opengl32 nativo
target_link_libraries(GrainMonitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Sql
    Qt6::Charts
    Qt6::Network
    opengl32
)