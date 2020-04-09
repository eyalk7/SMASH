#include <iostream>
#include <csignal>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	// if CURR_FORK_CHILD_RUNNING == 0 do nothing
	if (CURR_FORK_CHILD_RUNNING == 0) return;

    // send SIGSTOP to CURR_FORK_CHILD_RUNNING
    if (kill(CURR_FORK_CHILD_RUNNING, SIGTSTP) < 0) perror("smash error: kill failed");

    // print message
    cout << "smash: process " << CURR_FORK_CHILD_RUNNING << " was stopped" << endl;
}

void ctrlCHandler(int sig_num) {
    // if CURR_FORK_CHILD_RUNNING == 0 do nothing
    if (CURR_FORK_CHILD_RUNNING == 0) return;

    // send SIGSTOP to CURR_FORK_CHILD_RUNNING
    if (kill(CURR_FORK_CHILD_RUNNING, SIGKILL) < 0) perror("smash error: kill failed");

    // print message
    cout << "smash: process " << CURR_FORK_CHILD_RUNNING << " was killed" << endl;

}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

