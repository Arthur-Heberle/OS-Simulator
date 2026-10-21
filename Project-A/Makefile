# Build file for the scheduling simulator (Project A).
#   make          -> Linux binary   (build/simulator)
#   make windows  -> Windows binary (build/simulator.exe), needs mingw-w64
#   make clean    -> remove build/

SOURCES  := src/main.cpp $(wildcard src/core/*.cpp)
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude

# Cross-compiler that runs on Linux and produces Windows executables.
WIN_CXX  := x86_64-w64-mingw32-g++-posix

all: build/simulator

build/simulator: $(SOURCES) $(wildcard include/core/*.h)
	mkdir -p build
	g++ $(CXXFLAGS) $(SOURCES) -o $@

windows: build/simulator.exe

# -static embeds the C++ runtime in the .exe, so no extra DLLs are needed
# on the target machine (general requirement 5: run without installing anything).
build/simulator.exe: $(SOURCES) $(wildcard include/core/*.h)
	mkdir -p build
	$(WIN_CXX) $(CXXFLAGS) -static $(SOURCES) -o $@

clean:
	rm -rf build

.PHONY: all windows clean
