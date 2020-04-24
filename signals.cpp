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
    auto curr_time = time(nullptr);
    if (curr_time == (time_t)(-1)) {
        perror("smash error: time failed");
        return;
    }

    // search all jobs, for timeout commands
    // if pid  == 0 do nothing
    // if duration <= (curr_time - start_time) send sigkill
    TIME_UNTIL_NEXT_ALARM = numeric_limits<unsigned int>::max();
    auto jobs = GLOBAL_JOBS_POINTER->jobs;
    for (auto job : jobs) {
        if (!job.second.is_timeout || job.second.pid == 0) continue;

        unsigned int diff_time = difftime(curr_time, job.second.original_start_time);
        unsigned int time_remain = job.second.time_limit - diff_time;
        if (time_remain <= 0) {
            wakeUpCall(job.second.pid, job.second.cmd_str);
        } else if (time_remain < TIME_UNTIL_NEXT_ALARM) {
            TIME_UNTIL_NEXT_ALARM = time_remain;
        }
    }

    // send another alarm for the next timeout commands
    if (TIME_UNTIL_NEXT_ALARM < numeric_limits<unsigned int>::max()) alarm(TIME_UNTIL_NEXT_ALARM);
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
