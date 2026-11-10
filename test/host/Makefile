# Builds the real RP2040-ETH_Door.ino as a program for a PC, to test it against the real gateway.
# Uses the AMLink snapshot in src/AMLink and the Arduino stand-ins in arduino-stubs: no external dependency.
CXX      ?= g++
AMLINK   := $(abspath ../../src/AMLink)
CORE     := $(addprefix $(AMLINK)/amlink/,Link.cpp Entity.cpp Json.cpp Sha256.cpp Storage.cpp)
FLAGS    := -std=gnu++11 -Wall -Wextra -g -fsanitize=address,undefined -fno-omit-frame-pointer \
            -DARDUINO=10819 -DAMLINK_ARDUINO_STUBS=1 -DARDUINO_ARCH_RP2040 -DAMLINK_CUSTOM_ENV \
            -I. -Istubs -Iarduino-stubs -I$(AMLINK) -I../..
BUILD    ?= build
# Variants of the sketch, e.g.  make DEFS=-DDoor_Contact=0 BUILD=build-no-contact
DEFS     ?=

all: $(BUILD)/host_test

$(BUILD)/host_test: host_main.cpp ../../RP2040-ETH_Door.ino $(wildcard stubs/*.h) env_host.cpp $(CORE) $(wildcard $(AMLINK)/*.h $(AMLINK)/amlink/*.h $(AMLINK)/amlink/arduino/*.h arduino-stubs/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(FLAGS) $(DEFS) $(CORE) env_host.cpp host_main.cpp -o $@

clean:
	rm -rf build build-*
.PHONY: all clean
