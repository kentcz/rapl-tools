#include <iostream>
#include <unistd.h>
#include <fstream>

#include "Rapl.h"

using namespace std;


int main(int argc, char *argv[]) {

	// Default settings
	int runtime = 60;         // run for 60 seconds	
	int ms_pause = 100;       // sample every 100ms
	bool use_outfile = false; // no output file
	ofstream outfile;

	/**
	 *  Read Commandline Parameters
	 *
	 *  -p -- pause duration (in milliseconds)
	 *  -o -- write output to a file
	 *  -t -- run time (in sec) 
	 *
	 *  Example: ./a.out -p 500 -o outfile.csv
	 *
	 **/
	char input_char;
	while ((input_char = getopt(argc, argv, "p:o:")) != -1) {
		switch (input_char) {
		case 'p':
			ms_pause = atoi(optarg);
			break;
		case 't':
			runtime = atoi(optarg);
			break;
		case 'o':
			printf("ouput file:%s\n",optarg);
			use_outfile = true;
			outfile.open(optarg, ios::out | ios::trunc);
			break;
		default:
			abort();
		}
	}

	Rapl *rapl = new Rapl();
	while (rapl->total_time() < runtime) {
		usleep(1000 * ms_pause);
		rapl->sample();

		// Write sample to outfile
		if (use_outfile) {
			outfile << rapl->pkg_current_power() << ","
					<< rapl->pp0_current_power() << ","
					<< rapl->pp1_current_power() << ","
					<< rapl->dram_current_power() << ","
					<< rapl->total_time() << endl;
		}

		// Write sample to terminal
		cout << "\33[2K\r" // clear line
				<< "power=" << rapl->pkg_current_power()
				<< "\tTime=" << rapl->current_time();
		cout.flush();
	}

	// Print totals
	cout << endl 
		<< "\tTotal Energy:\t" << rapl->pkg_total_energy() << " J" << endl
		<< "\tAverage Power:\t" << rapl->pkg_average_power() << " W" << endl
		<< "\tTime:\t" << rapl->total_time() << " sec" << endl;

	return 0;
}
