# Builds the real RP2040-ETH_Door.ino as a program for a PC, to test it against the real gateway.
# Needs the AMLink library (ausbildungsmanager-amlink): set AMLINK_PATH, default is the sibling folder.
CXX      ?= g++
AMLINK   ?= $(abspath ../../../ausbildungsmanager-amlink)
CORE     := $(AMLINK)/src/amlink/Link.cpp $(AMLINK)/src/amlink/Entity.cpp $(AMLINK)/src/amlink/Json.cpp $(AMLINK)/src/amlink/Sha256.cpp $(AMLINK)/src/amlink/Storage.cpp
FLAGS    := -std=gnu++11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer \
            -DARDUINO=10819 -DAMLINK_ARDUINO_STUBS=1 -DARDUINO_ARCH_RP2040 -DAMLINK_CUSTOM_ENV \
            -I. -Istubs -I$(AMLINK)/test/stubs -I$(AMLINK)/src -I../..
BUILD    := build

all: $(BUILD)/host_test

$(BUILD)/host_test: host_main.cpp ../../RP2040-ETH_Door.ino $(wildcard stubs/*.h) env_host.cpp $(CORE) $(wildcard $(AMLINK)/src/amlink/*.h $(AMLINK)/src/amlink/arduino/*.h $(AMLINK)/src/*.h $(AMLINK)/test/stubs/*.h)
	@test -d $(AMLINK)/src || (echo "AMLink library not found: set AMLINK=/path/to/ausbildungsmanager-amlink"; exit 1)
	@mkdir -p $(BUILD)
	$(CXX) $(FLAGS) $(CORE) env_host.cpp host_main.cpp -o $@

clean:
	rm -rf $(BUILD)
.PHONY: all clean
