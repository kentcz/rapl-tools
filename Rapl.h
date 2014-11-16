#include <unistd.h>
#include <cstdint>

#ifndef RAPL_H_
#define RAPL_H_

struct rapl_state_t {
	uint64_t pkg;
	uint64_t pp0;
	uint64_t pp1;
	uint64_t dram;
	struct timeval tsc;
};

class Rapl {

private:
	// Rapl configuration
	int fd;
	int core = 0;
	bool pp1_supported = true;
	double power_units, energy_units, time_units;
	double thermal_spec_power, minimum_power, maximum_power, time_window;

	// Rapl state
	rapl_state_t *current_state;
	rapl_state_t *prev_state;
	rapl_state_t *next_state;
	rapl_state_t state1, state2, state3, running_total;

	bool detect_pp1();
	void open_msr();
	uint64_t read_msr(int msr_offset);
	double time_delta(struct timeval *begin, struct timeval *after);
	uint64_t energy_delta(uint64_t before, uint64_t after);
	double power(uint64_t before, uint64_t after, double time_delta);

public:
	Rapl();
	void reset();
	void sample();

	double pkg_current_power();
	double pp0_current_power();
	double pp1_current_power();
	double dram_current_power();

	double pkg_average_power();
	double pp0_average_power();
	double pp1_average_power();
	double dram_average_power();

	double pkg_total_energy();
	double pp0_total_energy();
	double pp1_total_energy();
	double dram_total_energy();

	double total_time();
	double current_time();
};

#endif /* RAPL_H_ */
