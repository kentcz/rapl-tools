CXX = g++
CXXFLAGS = -std=c++11 -g
LD = $(CXX)
LDFLAGS = -lm -lrt

all : PowerMonitor AppPowerMeter

run : PowerMonitor AppPowerMeter
	#./PowerMonitor
	./AppPowerMeter sleep 5

AppPowerMeter : AppPowerMeter.o Rapl.o
	$(LD) $(LDFLAGS) -o $@ $^

AppPowerMeter.o : AppPowerMeter.cpp Rapl.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

PowerMonitor : PowerMonitor.o Rapl.o
	$(LD) $(LDFLAGS) -o $@ $^

PowerMonitor.o : PowerMonitor.cpp Rapl.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

Rapl.o : Rapl.cpp Rapl.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< 

clean :
	rm -f *.o 
	rm -f rapl.csv
	rm -f AppPowerMeter
	rm -f PowerMonitor 
