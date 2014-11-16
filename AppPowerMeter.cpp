#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>

#include "Rapl.h"

int main(int argc, char *argv[]) {

	Rapl *rapl = new Rapl();
	int ms_pause = 100;       // sample every 100ms
	std::ofstream outfile ("rapl.csv", std::ios::out | std::ios::trunc);

	pid_t child_pid = fork();
	if (child_pid >= 0) { //fork successful
		if (child_pid == 0) { // child process
			//printf("CHILD: child pid=%d\n", getpid());
			int exec_status = execvp(argv[1], argv+1);
			if (exec_status) {
				std::cerr << "execv failed with error " 
					<< errno << " "
					<< strerror(errno) << std::endl;
			}
		} else {              // parent process
			
			int status = 1;
			waitpid(child_pid, &status, WNOHANG);	
			while (status) {
				
				usleep(ms_pause * 1000);

				// rapl sample
				rapl->sample();
				outfile << rapl->pkg_current_power() << ","
					<< rapl->pp0_current_power() << ","
					<< rapl->pp1_current_power() << ","
					<< rapl->dram_current_power() << ","
					<< rapl->total_time() << std::endl;

				waitpid(child_pid, &status, WNOHANG);	
			}
			wait(&status); /* wait for child to exit, and store its status */
			std::cout << "PARENT: Child's exit code is: " 
				<< WEXITSTATUS(status) 
				<< std::endl;
			
			std::cout << std::endl 
				<< "\tTotal Energy:\t" << rapl->pkg_total_energy() << " J" << std::endl
				<< "\tAverage Power:\t" << rapl->pkg_average_power() << " W" << std::endl
				<< "\tTime:\t" << rapl->total_time() << " sec" << std::endl;
		}
	} else {
		std::cerr << "fork failed" << std::endl;
		return 1;
	}
	
	return 0;
}
