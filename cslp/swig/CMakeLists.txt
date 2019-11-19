# Set up swig
find_package(SWIG REQUIRED)
include(${SWIG_USE_FILE})

# Add subdirectories for each language if desired
option(BUILD_PYTHON "Build Python SWIG module" ON)
if(BUILD_PYTHON)
    add_subdirectory(python)
endif()




#include_directories(pycslp PRIVATE
#    ${CMAKE_SOURCE_DIR}/include
#    ${PYTHON_INCLUDE_DIRS}
#)
##
#swig_add_library(pycslp
#    TYPE SHARED
#    LANGUAGE python
#    OUTPUT_DIR ${CMAKE_BINARY_DIR}/python/${PROJECT_NAME}/pycslp
#    SOURCES
#        ${CMAKE_CURRENT_SOURCE_DIR}/cslp.i
#        ${CMAKE_SOURCE_DIR}/cslp/cslp.cpp
#)






# set_property(SOURCE cslp.i PROPERTY SWIG_MODULE_NAME pycslp)

# set_property(TARGET pycslp PROPERTY SWIG_USE_TARGET_INCLUDE_DIRECTORIES ON)

#target_link_libraries(pycslp PRIVATE
#    cslp
#    ${PYTHON_LIBRARIES}
#)
