BUILD_DIR   := build
BUILD_TYPE  := Release
PREFIX      := /usr
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
JOBS        := $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build install clean distclean help

all: build

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) --parallel $(JOBS)

install: build
	sudo cmake --install $(BUILD_DIR) --prefix $(PREFIX)

clean:
	@[ -d $(BUILD_DIR) ] && cmake --build $(BUILD_DIR) --target clean || true

distclean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  all        - configure and build (default)"
	@echo "  configure  - run cmake configure"
	@echo "  build      - build the plugin"
	@echo "  install    - install to PREFIX (default: /usr)"
	@echo "  clean      - clean build artifacts"
	@echo "  distclean  - remove build directory entirely"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_TYPE  - Release or Debug (default: Release)"
	@echo "  PREFIX      - install prefix (default: /usr)"
	@echo "  CMAKE_FLAGS - extra cmake flags"
