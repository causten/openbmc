all: power

power: power.c
	$(CXX) -pthread -o $@ $^ $(LDFLAGS) -fpermissive

.PHONY: clean

clean:
	rm -rf *.o power
