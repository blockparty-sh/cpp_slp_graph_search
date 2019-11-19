# Include python
find_package(PythonLibs REQUIRED)
include_directories(
    ${CMAKE_SOURCE_DIR}/include
	${PYTHON_INCLUDE_PATH}
	../..
)


set(CMAKE_SWIG_FLAGS "")
set_source_files_properties(../cslp.i PROPERTIES CPLUSPLUS ON)

# Add swig module
swig_add_module(cslp python ../cslp.i)
swig_link_libraries(
	cslp cslp
	${PYTHON_LIBRARIES}
)

# Files to install with Python
set(PYTHON_INSTALL_FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cslp.py
        ${CMAKE_CURRENT_BINARY_DIR}/_cslp.so)

# Configure setup.py and copy to output directory
set(SETUP_PY_IN ${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in)
set(SETUP_PY_OUT ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
configure_file(${SETUP_PY_IN} ${SETUP_PY_OUT})

# Declare install target for python
#install(TARGETS cslp
#        COMMAND "${PYTHON_EXECUTABLE} setup.py"
#        COMPONENT swig-python)

# Install target to call setup.py
add_custom_target(install-python
        DEPENDS _cslp
        COMMAND python ${SETUP_PY_OUT} install)

