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
    time_t curr_time = time(nullptr);
    if (curr_time == (time_t)(-1)) {
        perror("smash error: time failed");
        return;
    }

    // search all jobs, for timeout commands
    // if pid  == 0 do nothing
    // if duration <= (curr_time - start_time) send sigkill
    TIME_UNTIL_NEXT_ALARM = numeric_limits<double>::max();
    map<JobID,JobEntry>& jobs = GLOBAL_JOBS_POINTER->jobs;
    for (auto& job : jobs) {
        if (!job.second.is_timeout || job.second.pid == 0) continue;

        double time_remain = (double)job.second.time_limit - difftime(curr_time, job.second.original_start_time);
        if (time_remain < 1.0) {

            cout << "smash: got an alarm" << endl;

            // get group pid
            pid_t gpid = getpgid(job.second.pid);
            if (gpid < 0) {
                perror("smash error: getgpid failed");
                continue;
            }

            // send signal, print message
            if (killpg(gpid, SIGKILL) < 0) {
                perror("smash error: killpg failed");
            } else {
                cout << "smash: " << job.second.cmd_str << " timed out!" << endl;
                job.second.is_timeout = false; // make sure that we don't "wake up" a job twice
            }

        } else if ((unsigned int)time_remain < TIME_UNTIL_NEXT_ALARM) {
            TIME_UNTIL_NEXT_ALARM = time_remain;
        }
    }

    // send another alarm for the next timeout commands
    if (TIME_UNTIL_NEXT_ALARM < numeric_limits<double>::max()) {
        alarm((unsigned int)TIME_UNTIL_NEXT_ALARM);
        TIME_AT_LAST_UPDATE = curr_time;
    }
}
