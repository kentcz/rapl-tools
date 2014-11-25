rapl-tools
==========

Tools for monitoring CPU power with Intel's RAPL interface

Project contents
-------
AppPowerMeter - Measures the energy and monitors the power of an app
PowerMonitor  - Monitors CPU power of the system 
Rapl.cpp      - CPP class designed to function as a RAPL interface 

Running
-------
AppPowerMeter is a commandline wrapper for application commands. For example,
```
./AppPowerMeter sleep 5
```
will measure the total energy and average power of the CPU while it runs "sleep 5". The CPU power is sampled every 100ms then written to rapl.csv. 

The csv column headings in rapl.csv are "pkg, pp0, pp1, dram, time", measured in watts. See the Intel specification for more information about the different measurement types. 

Building
-------
The project is built with Gnu Make. G++ >= 4.7 is required.

```
make
```

Setup on Ubuntu
-------
Before using the tools, you will need to load the msr module and adjust the permissions.

```
# Install and load the msr module
sudo apt-get install msr-tools
sudo modprobe msr

# Set permissions
sudo chmod o+rw /dev/cpu/0/msr
```
Recent versions of the linux kernel require special permission to access the msr.
```
sudo apt-get install libcap2-bin
sudo setcap cap_sys_rawio+ep ./AppPowerMeter
sudo setcap cap_sys_rawio+ep ./PowerMonitor
```
