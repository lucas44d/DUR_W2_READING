cmake_minimum_required(VERSION 3.16)
project(RefractometerViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt a besoin de ces trois options pour generer le code des signaux/slots des fichiers .ui et des ressources.
# On n'utilise pas de .ui ni de .qrc ici, mais les laisser actives ne coute rien et evite d'y penser si une évolution est nécessaire.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Chemin d'installation de Qt6
# à ajuster pour chaque machine 
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.11.2/mingw_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

add_executable(RefractometerViewer
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.hpp
)

target_link_libraries(RefractometerViewer PRIVATE Qt6::Widgets Qt6::Charts)

# Decommenter pour supprimer la console en arriere-plan une fois que tout fonctionne
# (utile pendant le developpement de garder la console, pour voir d'eventuels messages d'erreur Qt) :
# set_target_properties(RefractometerViewer PROPERTIES WIN32_EXECUTABLE TRUE)
