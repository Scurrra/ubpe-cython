LIB_NAME := libubpe

WINDOWS := windows

# Windows is just *special*
ifeq ($(filter $(WINDOWS)%, $(OS)), $(OS))
	CXX_FLAGS = -fno-strict-overflow -Wsign-compare -Wall -std=c++20 -O2

    PYTHON_LIBS_DIR = $(shell echo $(shell python -c "import sys; print(sys.prefix + '\\libs')"))
	PYTHON_LIB_NAME = $(shell echo python$(shell python -c "import sys; print(sys.version_info[0])")$(shell python -c "import sys; print(sys.version_info[1])"))
	LDFLAGS = -L$(PYTHON_LIBS_DIR) -l$(PYTHON_LIB_NAME)
	
	LIB_FILE = $(LIB_NAME).pyd
else 	
	CXX_FLAGS = -pthread -fno-strict-overflow -Wsign-compare -Wall -fPIC -std=c++20 -O2

	LDFLAGS = $(shell python3-config --ldflags --embed)
	
	LIB_FILE = $(LIB_NAME).so
endif

CXX := clang++

CXX_INCLUDE := -Iubpe_cpp/headers -Iubpe_cpp/include
INCLUDEPY := -I$(shell python -c "import sysconfig; print(sysconfig.get_config_var('INCLUDEPY'))")

CYTHON_DIR := ubpe_cython
CYTHON_SRC_DIR := $(CYTHON_DIR)/ubpe

BUILD_DIR = build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

build_cython: cythonize
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDE) $(INCLUDEPY) -c $(BUILD_DIR)/$(LIB_NAME).cython.cpp -o $(BUILD_DIR)/$(LIB_NAME).cython.o

cythonize: $(BUILD_DIR)
	cython --cplus $(CYTHON_SRC_DIR)/$(LIB_NAME).pyx -o $(BUILD_DIR)/$(LIB_NAME).cython.cpp

build_lib: build_cython
	$(CXX) $(CXX_FLAGS) -shared $(LDFLAGS) $(BUILD_DIR)/$(LIB_NAME).cython.o -o $(CYTHON_DIR)/$(LIB_FILE)
