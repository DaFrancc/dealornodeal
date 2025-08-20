# ---- Toolchain (default to clang++; override with: make CXX=g++) ----
CXX ?= clang++

# Detect compiler family
IS_CLANG := $(shell $(CXX) --version 2>/dev/null | head -1 | grep -ci clang)
IS_GCC   := $(shell $(CXX) --version 2>/dev/null | head -1 | grep -ci gcc)

# ---- Packages ----
PKGS := sdl2 SDL2_ttf
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS   := $(shell pkg-config --libs   $(PKGS))

# ---- Project ----
SRC        := main.cpp
BIN_DIR    := bin
BUILD_DIR  := build
DEBUG_DIR  := $(BUILD_DIR)/debug
TSAN_DIR   := $(BUILD_DIR)/tsan
RELEASE_DIR:= $(BUILD_DIR)/release

DEBUG_BIN  := $(BIN_DIR)/hello_sdl2_dbg
TSAN_BIN   := $(BIN_DIR)/hello_sdl2_tsan
RELEASE_BIN:= $(BIN_DIR)/hello_sdl2

# ---- Common flags ----
CXXSTD   := -std=c++17
DEPFLAGS := -MMD -MP

WARNINGS_COMMON := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
                   -Wcast-qual -Wold-style-cast -Woverloaded-virtual -Wnull-dereference \
                   -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough -Wvla -Wundef \
                   -Wnon-virtual-dtor

ifeq ($(IS_CLANG),1)
  COLORFLAGS := -fcolor-diagnostics
  EXTRA_WARN := -Wextra-semi
  SAN_EXTRA  := -fsanitize-address-use-after-scope
else ifeq ($(IS_GCC),1)
  COLORFLAGS := -fdiagnostics-color=always
  EXTRA_WARN :=
  SAN_EXTRA  :=
else
  COLORFLAGS :=
  EXTRA_WARN :=
  SAN_EXTRA  :=
endif

WARNINGS := $(WARNINGS_COMMON) $(EXTRA_WARN) $(COLORFLAGS)

# Sanitizers / debug knobs
DBG      := -g3 -O0 -fno-omit-frame-pointer
ASANUB   := -fsanitize=address,undefined -fno-sanitize-recover=all $(SAN_EXTRA)
TSAN     := -fsanitize=thread -fno-omit-frame-pointer

CXXFLAGS_DEBUG    := $(CXXSTD) $(WARNINGS) $(DEPFLAGS) $(DBG) $(ASANUB) $(PKG_CFLAGS)
LDFLAGS_DEBUG     := $(ASANUB) $(PKG_LIBS)

CXXFLAGS_TSAN     := $(CXXSTD) $(WARNINGS) $(DEPFLAGS) $(DBG) $(TSAN) $(PKG_CFLAGS)
LDFLAGS_TSAN      := $(TSAN) $(PKG_LIBS)

CXXFLAGS_RELEASE  := $(CXXSTD) $(WARNINGS) $(DEPFLAGS) -O3 -DNDEBUG -flto -fno-omit-frame-pointer $(PKG_CFLAGS)
LDFLAGS_RELEASE   := -flto $(PKG_LIBS)

# ---- Objects/Deps ----
DEBUG_OBJ   := $(DEBUG_DIR)/main.o
TSAN_OBJ    := $(TSAN_DIR)/main.o
RELEASE_OBJ := $(RELEASE_DIR)/main.o

DEBUG_DEPS   := $(DEBUG_OBJ:.o=.d)
TSAN_DEPS    := $(TSAN_OBJ:.o=.d)
RELEASE_DEPS := $(RELEASE_OBJ:.o=.d)

# ---- LeakSanitizer suppressions ----
SUPPRESS_FILE := tools/lsan.supp
ASAN_ENV := ASAN_OPTIONS=alloc_dealloc_mismatch=0,strict_string_checks=1
LSAN_ENV := LSAN_OPTIONS=suppressions=$(SUPPRESS_FILE),print_suppressions=0

# Create a default suppression file if it doesn't exist
$(SUPPRESS_FILE):
	@mkdir -p tools
	@{ \
	  echo "leak:libdbus-1.so"; \
	  echo "leak:libnvidia-glcore.so"; \
	  echo "leak:libSDL3.so"; \
	  echo "leak:libGLX_nvidia"; \
	} > $(SUPPRESS_FILE)
	@echo "Created $(SUPPRESS_FILE)"

# ---- Default ----
.PHONY: all
all: debug

# ---- Build targets ----
.PHONY: debug tsan release
debug:   $(DEBUG_BIN)
tsan:    $(TSAN_BIN)
release: $(RELEASE_BIN)

$(DEBUG_BIN): $(DEBUG_OBJ) | $(BIN_DIR)
	$(CXX) $(DEBUG_OBJ) -o $@ $(LDFLAGS_DEBUG)

$(TSAN_BIN): $(TSAN_OBJ) | $(BIN_DIR)
	$(CXX) $(TSAN_OBJ) -o $@ $(LDFLAGS_TSAN)

$(RELEASE_BIN): $(RELEASE_OBJ) | $(BIN_DIR)
	$(CXX) $(RELEASE_OBJ) -o $@ $(LDFLAGS_RELEASE)

# ---- Compile rules ----
$(DEBUG_DIR)/%.o: %.cpp | $(DEBUG_DIR)
	$(CXX) $(CXXFLAGS_DEBUG) -c $< -o $@

$(TSAN_DIR)/%.o: %.cpp | $(TSAN_DIR)
	$(CXX) $(CXXFLAGS_TSAN) -c $< -o $@

$(RELEASE_DIR)/%.o: %.cpp | $(RELEASE_DIR)
	$(CXX) $(CXXFLAGS_RELEASE) -c $< -o $@

# ---- Convenience ----
.PHONY: run run-noscan gdb clean
run: debug $(SUPPRESS_FILE)
	$(ASAN_ENV) $(LSAN_ENV) ./$(DEBUG_BIN)

# Run with leak detection disabled (keeps ASan/UBSan for everything else)
run-noscan: debug
	ASAN_OPTIONS=detect_leaks=0 ./$(DEBUG_BIN)

gdb: debug
	gdb ./$(DEBUG_BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(SUPPRESS_FILE)

# ---- Dirs ----
$(BIN_DIR) $(DEBUG_DIR) $(TSAN_DIR) $(RELEASE_DIR):
	mkdir -p $@

# ---- Auto-deps ----
-include $(DEBUG_DEPS) $(TSAN_DEPS) $(RELEASE_DEPS)
