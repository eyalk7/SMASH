#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
	// if CURR_FORKK == 0 do nothing
    // send SIGSTOP to CURR_FORK...
    // print message
}

void ctrlCHandler(int sig_num) {
    // if CURR_FORKK == 0 do nothing
    // send SIGKILL to CURR_FORK...
    // print message

}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

