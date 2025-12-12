LIB_NAME := libubpe

reverse = $(if $(1),$(call reverse,$(wordlist 2,$(words $(1)),$(1)))) $(firstword $(1))

PYTHON_VERSION_FULL := $(shell python --version 2>&1 | awk '{print $$2}')
PYTHON_MAJOR_VERSION := $(word 1,$(subst ., ,$(PYTHON_VERSION_FULL)))
PYTHON_MINOR_VERSION := $(word 2,$(subst ., ,$(PYTHON_VERSION_FULL)))

# Detect the operating system
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
else
    # Use 'uname -s' to get the system name (Linux, Darwin, etc.)
    DETECTED_OS := $(shell uname -s)
endif

# Windows is just *special*
ifeq ($(DETECTED_OS),Windows)
    # I don't use windows, so it's only for the github actions
	PYTHON_INCLUDE := $(LOCALAPPDATA)/Programs/Python/Python$(PYTHON_MAJOR_VERSION)$(PYTHON_MINOR_VERSION)/include
	LIB_EXT := pyd
else 
	PYTHON_INCLUDE := /usr/include/python$(PYTHON_MAJOR_VERSION).$(PYTHON_MINOR_VERSION)
	LIB_EXT := so
endif

CXX := g++
CXX_FLAGS := -pthread -fno-strict-overflow -Wsign-compare -Wall -fPIC -std=c++20 -Os# -DNDEBUG -O2
CXX_INCLUDE := -Iubpe_cpp/headers -Iubpe_cpp/include
CYTHON_SPECIFIC_FLAGS := -I$(PYTHON_INCLUDE)

CYTHON_DIR := ubpe_cython
CYTHON_SRC_DIR := $(CYTHON_DIR)/ubpe

BUILD_DIR = build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

build_cython: cythonize
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDE) $(CYTHON_SPECIFIC_FLAGS) -c $(BUILD_DIR)/$(LIB_NAME).cython.cpp -o $(BUILD_DIR)/$(LIB_NAME).cython.o

cythonize: $(BUILD_DIR)
	cython --cplus $(CYTHON_SRC_DIR)/$(LIB_NAME).pyx -o $(BUILD_DIR)/$(LIB_NAME).cython.cpp

build_lib: build_cython
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDE) $(CYTHON_SPECIFIC_FLAGS) -shared $(BUILD_DIR)/$(LIB_NAME).cython.o -o $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT)

copy_lib: $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT)
	cp $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT) $(CYTHON_DIR)/$(LIB_NAME).$(LIB_EXT)
