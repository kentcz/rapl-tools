#include <cstdio>
#include <string>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "Rapl.h"

#define MSR_RAPL_POWER_UNIT            0x606

/*
 * Platform specific RAPL Domains.
 * Note that PP1 RAPL Domain is supported on 062A only
 * And DRAM RAPL Domain is supported on 062D only
 */
/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT       0x610
#define MSR_PKG_ENERGY_STATUS          0x611
#define MSR_PKG_PERF_STATUS            0x13
#define MSR_PKG_POWER_INFO             0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT            0x638
#define MSR_PP0_ENERGY_STATUS          0x639
#define MSR_PP0_POLICY                 0x63A
#define MSR_PP0_PERF_STATUS            0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT            0x640
#define MSR_PP1_ENERGY_STATUS          0x641
#define MSR_PP1_POLICY                 0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT           0x618
#define MSR_DRAM_ENERGY_STATUS         0x619
#define MSR_DRAM_PERF_STATUS           0x61B
#define MSR_DRAM_POWER_INFO            0x61C

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET              0
#define POWER_UNIT_MASK                0x0F

#define ENERGY_UNIT_OFFSET             0x08
#define ENERGY_UNIT_MASK               0x1F00

#define TIME_UNIT_OFFSET               0x10
#define TIME_UNIT_MASK                 0xF000

#define SIGNATURE_MASK                 0xFFFF0
#define IVYBRIDGE_E                    0x306F0
#define SANDYBRIDGE_E                  0x206D0


Rapl::Rapl() {

	open_msr();
	pp1_supported = detect_pp1();

	/* Read MSR_RAPL_POWER_UNIT Register */
	uint64_t raw_value = read_msr(MSR_RAPL_POWER_UNIT);
	power_units = pow(0.5,	(double) (raw_value & 0xf));
	energy_units = pow(0.5,	(double) ((raw_value >> 8) & 0x1f));
	time_units = pow(0.5,	(double) ((raw_value >> 16) & 0xf));

	/* Read MSR_PKG_POWER_INFO Register */
	raw_value = read_msr(MSR_PKG_POWER_INFO);
	thermal_spec_power = power_units * ((double)(raw_value & 0x7fff));
	minimum_power = power_units * ((double)((raw_value >> 16) & 0x7fff));
	maximum_power = power_units * ((double)((raw_value >> 32) & 0x7fff));
	time_window = time_units * ((double)((raw_value >> 48) & 0x7fff));

	reset();
}

void Rapl::reset() {

	prev_state = &state1;
	current_state = &state2;
	next_state = &state3;

	// sample twice to fill current and previous
	sample();
	sample();

	// Initialize running_total
	running_total.pkg = 0;
	running_total.pp0 = 0;
	running_total.dram = 0;
	gettimeofday(&(running_total.tsc), NULL);
}

bool Rapl::detect_pp1() {
	uint32_t eax_input = 1;
	uint32_t eax;
	__asm__("cpuid;"
			:"=a"(eax)               // EAX into b (output)
			:"0"(eax_input)          // 1 into EAX (input)
			:"%ebx","%ecx","%edx");  // clobbered registers

	uint32_t cpu_signature = eax & SIGNATURE_MASK;
	if (cpu_signature == SANDYBRIDGE_E || cpu_signature == IVYBRIDGE_E) {
		return false;
	}
	return true;
}

void Rapl::open_msr() {
	std::stringstream filename_stream;
	filename_stream << "/dev/cpu/" << core << "/msr";
	fd = open(filename_stream.str().c_str(), O_RDONLY);
	if (fd < 0) {
		if ( errno == ENXIO) {
			fprintf(stderr, "rdmsr: No CPU %d\n", core);
			exit(2);
		} else if ( errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
			exit(3);
		} else {
			perror("rdmsr:open");
			fprintf(stderr, "Trying to open %s\n",
					filename_stream.str().c_str());
			exit(127);
		}
	}
}

uint64_t Rapl::read_msr(int msr_offset) {
	uint64_t data;
	if (pread(fd, &data, sizeof(data), msr_offset) != sizeof(data)) {
		perror("read_msr():pread");
		exit(127);
	}
	return data;
}

void Rapl::sample() {
	uint32_t max_int = ~((uint32_t) 0);

	next_state->pkg = read_msr(MSR_PKG_ENERGY_STATUS) & max_int;
	next_state->pp0 = read_msr(MSR_PP0_ENERGY_STATUS) & max_int;

	if (pp1_supported) {
		next_state->pp1 = read_msr(MSR_PP1_ENERGY_STATUS) & max_int;
		next_state->dram = 0;
	} else {
		next_state->pp1 = 0;
		next_state->dram = read_msr(MSR_DRAM_ENERGY_STATUS) & max_int;
	}

	gettimeofday(&(next_state->tsc), NULL);


	// Update running total
	running_total.pkg += energy_delta(current_state->pkg, next_state->pkg);
	running_total.pp0 += energy_delta(current_state->pp0, next_state->pp0);
	running_total.pp1 += energy_delta(current_state->pp0, next_state->pp0);
	running_total.dram += energy_delta(current_state->dram, next_state->dram);

	// Rotate states
	rapl_state_t *pprev_state = prev_state;
	prev_state = current_state;
	current_state = next_state;
	next_state = pprev_state;
}

double Rapl::time_delta(struct timeval *begin, struct timeval *end) {
        return (end->tv_sec - begin->tv_sec)
                + ((end->tv_usec - begin->tv_usec)/1000000.0);
}

double Rapl::power(uint64_t before, uint64_t after, double time_delta) {
	if (time_delta == 0.0f || time_delta == -0.0f) { return 0.0; }
	double energy = energy_units * ((double) energy_delta(before,after));
	return energy / time_delta;
}

uint64_t Rapl::energy_delta(uint64_t before, uint64_t after) {
	uint64_t max_int = ~((uint32_t) 0);
	uint64_t eng_delta = after - before;

	// Check for rollovers
	if (before > after) {
		eng_delta = after + (max_int - before);
	}

	return eng_delta;
}

double Rapl::pkg_current_power() {
	double t = time_delta(&(prev_state->tsc), &(current_state->tsc));
	return power(prev_state->pkg, current_state->pkg, t);
}

double Rapl::pp0_current_power() {
	double t = time_delta(&(prev_state->tsc), &(current_state->tsc));
	return power(prev_state->pp0, current_state->pp0, t);
}

double Rapl::pp1_current_power() {
	double t = time_delta(&(prev_state->tsc), &(current_state->tsc));
	return power(prev_state->pp1, current_state->pp1, t);
}

double Rapl::dram_current_power() {
	double t = time_delta(&(prev_state->tsc), &(current_state->tsc));
	return power(prev_state->dram, current_state->dram, t);
}

double Rapl::pkg_average_power() {
	return pkg_total_energy() / total_time();
}

double Rapl::pp0_average_power() {
	return pp0_total_energy() / total_time();
}

double Rapl::pp1_average_power() {
	return pp1_total_energy() / total_time();
}

double Rapl::dram_average_power() {
	return dram_total_energy() / total_time();
}

double Rapl::pkg_total_energy() {
	return energy_units * ((double) running_total.pkg);
}

double Rapl::pp0_total_energy() {
	return energy_units * ((double) running_total.pp0);
}

double Rapl::pp1_total_energy() {
	return energy_units * ((double) running_total.pp1);
}

double Rapl::dram_total_energy() {
	return energy_units * ((double) running_total.dram);
}

double Rapl::total_time() {
	return time_delta(&(running_total.tsc), &(current_state->tsc));
}

double Rapl::current_time() {
	return time_delta(&(prev_state->tsc), &(current_state->tsc));
}
