LIB_NAME := libubpe

WINDOWS := windows
MAC_OS := macos

# Windows is just *special*
ifeq ($(filter $(WINDOWS)%, $(OS)), $(OS))
	PLATFORM = Windows
	CXX_FLAGS = -pthread -fno-strict-overflow -Wsign-compare -Wall -std=c++20 -O2

    PYTHON_LIBS_DIR = $(shell python -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))" || echo $(shell python -c "import sys; print(sys.prefix + '\\libs')"))
	PYTHON_LIB_NAME = $(shell python -c "import sysconfig; print(sysconfig.get_config_var('LIBRARY'))" || echo python$(shell python -c "import sys; print(sys.version_info[0])")$(shell python -c "import sys; print(sys.version_info[1])").lib)
	LDFLAGS = -L$(PYTHON_LIBS_DIR) -l$(PYTHON_LIB_NAME)
	
	LIB_EXT = pyd
else ifeq ($(filter $(MAC_OS)%, $(OS)), $(OS))
	PLATFORM = macOS
	CXX_FLAGS = -pthread -fno-strict-overflow -Wsign-compare -Wall -fPIC -std=c++20 -O2

	LDFLAGS = $(shell python3-config --ldflags)
	
	LIB_EXT = so
else	
	PLATFORM = Linux
	CXX_FLAGS = -pthread -fno-strict-overflow -Wsign-compare -Wall -fPIC -std=c++20 -O2

	LDFLAGS = $(shell python3-config --ldflags)
	
	LIB_EXT = so
endif

CXX := clang++

CXX_INCLUDE := -Iubpe_cpp/headers -Iubpe_cpp/include
INCLUDEPY := -I$(shell python -c "import sysconfig; print(sysconfig.get_config_var('INCLUDEPY'))")

CYTHON_DIR := ubpe_cython
CYTHON_SRC_DIR := $(CYTHON_DIR)/ubpe

BUILD_DIR = build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

print:
	@echo $(PLATFORM)
	@echo $(CXX_FLAGS)
	@echo $(LDFLAGS)

build_cython: cythonize
	$(CXX) $(CXX_FLAGS) $(CXX_INCLUDE) $(INCLUDEPY) -c $(BUILD_DIR)/$(LIB_NAME).cython.cpp -o $(BUILD_DIR)/$(LIB_NAME).cython.o

cythonize: $(BUILD_DIR)
	cython --cplus $(CYTHON_SRC_DIR)/$(LIB_NAME).pyx -o $(BUILD_DIR)/$(LIB_NAME).cython.cpp

build_lib: build_cython
	$(CXX) $(CXX_FLAGS) -shared $(LDFLAGS) $(BUILD_DIR)/$(LIB_NAME).cython.o -o $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT)

copy_lib: $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT)
	cp $(BUILD_DIR)/$(LIB_NAME).$(LIB_EXT) $(CYTHON_DIR)/$(LIB_NAME).$(LIB_EXT)
