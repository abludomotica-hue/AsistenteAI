cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/JC4880P443C_I_W/1-Demo/idf_examples/ESP-IDF/common_components"
)

# Incluir el archivo de configuración del proyecto ESP-IDF
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Agregar la ruta de los componentes de ESP-ADF si está definido ADF_PATH
if(DEFINED ENV{ADF_PATH})
    list(APPEND EXTRA_COMPONENT_DIRS $ENV{ADF_PATH}/components)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mno-relax")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-relax")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mno-relax")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-relax")

project(asistente_ai)
