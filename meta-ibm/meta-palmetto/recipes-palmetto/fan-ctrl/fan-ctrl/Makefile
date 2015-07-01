# Copyright 2014-present Facebook. All Rights Reserved.
all: fand kick_watchdog

fand: fand.cpp watchdog.cpp
	$(CXX) -pthread -o $@ $^ $(LDFLAGS) -lwedge_eeprom

kick_watchdog: kick_watchdog.cpp watchdog.cpp
	$(CXX) -pthread -o $@ $^ $(LDFLAGS) -lwedge_eeprom

.PHONY: clean

clean:
	rm -rf *.o fand kick_watchdog
