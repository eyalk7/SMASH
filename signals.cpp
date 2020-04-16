#include <iostream>
#include <csignal>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;

	// if CURR_FORK_CHILD_RUNNING == 0 do nothing
	if (CURR_FORK_CHILD_RUNNING == 0) return;

    // send SIGSTOP to CURR_FORK_CHILD_RUNNING
    pid_t gpid = getpgid(CURR_FORK_CHILD_RUNNING);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal, print message
    if (killpg(gpid, SIGSTOP) < 0) {
        perror("smash error: killpg failed");
    } else {
        cout << "smash: process " << CURR_FORK_CHILD_RUNNING << " was stopped" << endl;
    }
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;

    // if CURR_FORK_CHILD_RUNNING == 0 do nothing
    if (CURR_FORK_CHILD_RUNNING == 0) return;

    // send SIGSTOP to CURR_FORK_CHILD_RUNNING
    pid_t gpid = getpgid(CURR_FORK_CHILD_RUNNING);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal, print message
    if (killpg(gpid, SIGKILL) < 0) {
        perror("smash error: killpg failed");
    } else {
        cout << "smash: process " << CURR_FORK_CHILD_RUNNING << " was killed" << endl;
    }
}

void alarmHandler(int sig_num) {
    bool signal_sent = false;
    auto curr_time = time(nullptr);
    if (curr_time == (time_t)(-1)) {
        perror("smash error: time failed");
        return;
    }

    // search all jobs, for timeout commands
    // if pid  == 0 do nothing
    // if duration <= (curr_time - start_time) send sigkill
    auto jobs = GLOBAL_JOBS_POINTER->jobs;
    for (auto job : jobs) {
        if (!job.second.is_timeout || job.second.pid == 0) continue;

        auto diff_time = difftime(curr_time, job.second.start_time);
        if (job.second.time_limit <= (unsigned int)diff_time) {
            wakeUpCall(job.second.pid, job.second.cmd_str);
            signal_sent = true;
        }
    }

    if (!signal_sent) {
        cout << "smash: got an alarm" << endl;
    }
}
void wakeUpCall(int pid, const string& str) {
    cout << "smash: got an alarm" << endl;

    // send SIGSTOP to CURR_FORK_CHILD_RUNNING
    pid_t gpid = getpgid(pid);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal, print message
    if (killpg(gpid, SIGKILL) < 0) {
        perror("smash error: killpg failed");
    } else {
        cout << "smash: " << str << " timed out!" << endl;
    }
}
