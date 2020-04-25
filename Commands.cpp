#include "Commands.h"

using namespace std;

// definition of CURR_FORK_CHILD_RUNNING`
pid_t CURR_FORK_CHILD_RUNNING = 0;
JobsList* GLOBAL_JOBS_POINTER = nullptr;
double TIME_UNTIL_NEXT_ALARM = numeric_limits<double>::max();
time_t TIME_AT_LAST_UPDATE = 0;

//----------------------GIVEN PARSING FUNCTIONS------------------------------------

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}
string _rtrim(const std::string& s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
string _trim(const std::string& s) {
    return _rtrim(_ltrim(s));
}
int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = nullptr;
    }
    return i;

    FUNC_EXIT()
}

//----------------------------OUR CODE-------------------------------------------

bool checkAndRemoveAmpersand(string& str) {
    if (str.empty()) return false;

    bool has_ampersand = false;
    int to_delete = str.size()-1;
    while (to_delete >= 0 && (str[to_delete] == ' ' || str[to_delete] == '&') ) {
        if (str[to_delete] == '&') has_ampersand = true;
        to_delete--;
    }

    // erase
    if (to_delete < 0) { // all spaces and ampersands
        str = "";
    } else {
        str.erase(str.begin()+to_delete+1, str.end());
    }

    return has_ampersand;
}

bool isSmashChild() {
    return getppid() == SMASH_PROCESS_PID;
}
bool isSmash() {
    return getpid() == SMASH_PROCESS_PID;
}

bool childWait(pid_t pid) {
    // i'm child of SMASH, just wait for grandchild and return
    if (!isSmash()) {
        if (waitpid(pid, nullptr, 0) < 0) perror("smash error: waitpid failed");
        return true;
    }
    return false;
}

void printError(const string& msg) {
    std::cerr << "smash error: " << msg << endl;
}

bool isBuiltInCommand(const string& cmd_part) {
    if (cmd_part.compare("chprompt") == 0 || cmd_part.compare("chprompt&") == 0 || cmd_part.find("chprompt ") == 0) return true;
    if (cmd_part.compare("showpid") == 0 || cmd_part.compare("showpid&") == 0 || cmd_part.find("showpid ") == 0) return true;
    if (cmd_part.compare("pwd") == 0 || cmd_part.compare("pwd&") == 0 || cmd_part.find("pwd ") == 0) return true;
    if (cmd_part.compare("cd") == 0 || cmd_part.compare("cd&") == 0 || cmd_part.find("cd ") == 0) return true;
    if (cmd_part.compare("jobs") == 0 || cmd_part.compare("jobs&") == 0 || cmd_part.find("jobs ") == 0) return true;
    if (cmd_part.compare("kill") == 0 || cmd_part.compare("kill&") == 0 || cmd_part.find("kill ") == 0) return true;
    if (cmd_part.compare("fg") == 0 || cmd_part.compare("fg&") == 0 || cmd_part.find("fg ") == 0) return true;
    if (cmd_part.compare("bg") == 0 || cmd_part.compare("bg&") == 0 || cmd_part.find("bg ") == 0) return true;
    if (cmd_part.compare("quit") == 0 || cmd_part.compare("quit&") == 0 || cmd_part.find("quit ") == 0) return true;

// TODO: maybe timeout isn't built in commmand for that matter
    if (cmd_part.compare("timeout") == 0 || cmd_part.compare("timeout&") == 0 || cmd_part.find("timeout ") == 0) return true;

    return false;
}
//---------------------------JOBS LISTS------------------------------
JobEntry::JobEntry(pid_t pid, const string& cmd_str, bool is_stopped, bool is_timeout, unsigned int time_limit) : pid(pid),
                                                                                                                cmd_str(cmd_str),
                                                                                                                is_stopped(is_stopped),
                                                                                                                is_timeout(is_timeout),
                                                                                                                time_limit(time_limit) {
    SetTime();
    original_start_time = start_time;
}
void JobEntry::SetTime() {
    start_time = time(nullptr);
    if (start_time == (time_t)(-1)) perror("smash error: time failed");
}

JobEntry* JobsList::addJob(pid_t pid, const string& cmd_str, bool is_stopped, bool is_timeout, unsigned int time_limit) {
    // remove zombies from jobs list
    removeFinishedJobs();

    // create new job entry
    JobEntry new_job(pid, cmd_str, is_stopped, is_timeout, time_limit);
    JobID new_id = 1;
    if (!jobs.empty()) new_id = jobs.rbegin()->first + 1;

    // insert to map
    jobs[new_id] = new_job;

    return &jobs[new_id];
}
void JobsList::printJobsList() {
    // remove zombies from jobs list
    removeFinishedJobs();

    // iterate the map and print each job by the format
    for (const auto& job : jobs) {
        auto curr_time = time(nullptr);
        if (curr_time == (time_t)(-1)) perror("smash error: time failed");
        auto diff_time = difftime(curr_time, job.second.start_time);
        if (diff_time == (time_t)(-1)) perror("smash error: difftime failed");

        cout << "[" << job.first << "]";
        cout << " " << job.second.cmd_str;
        cout << " : " << job.second.pid;
        cout << " " << diff_time << " secs";
        if (job.second.is_stopped) cout << " (stopped)";
        cout << endl;
    }
}
void JobsList::killAllJobs() {
    // remove zombies from jobs list
    removeFinishedJobs();

    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;

    // iterate on map, print message and send SIGKILL then wait them
    for (auto& job : jobs) {
        cout << job.second.pid << ": " << job.second.cmd_str << endl;
        pid_t gpid = getpgid(job.second.pid);
        if (gpid < 0) {
            perror("smash error: getgpid failed");
        } else {
            // send sigkill to a process group
            if (killpg(gpid, SIGKILL) < 0) {
                perror("smash error: killpg failed");
            } else {
                if (waitpid(job.second.pid, nullptr, 0) < 0)
                    perror("smash error: waitpid failed");
            }
        }
        job.second.pid = 0;
    }

    jobs.clear();
}

void JobsList::removeFinishedJobs() {
    if (!isSmash()) return; // not the SMASH

    vector<JobID> to_remove(100,0);
    int to_remove_iter= 0;

    // iterate on map looking for finished jobs
    for (const auto& job : jobs) {
        // pid = 0 --> it's set to be removed
        if (job.second.pid == 0) {
            to_remove[to_remove_iter++] = job.first;
            continue;
        }

        // check if job finished using waitpid with WNOHANG
        pid_t waited = waitpid(job.second.pid, nullptr, WNOHANG);
        if (waited < 0) perror("smash error: waitpid failed");
        if (waited > 0) to_remove[to_remove_iter++] = job.first;
    }

    // remove from map all waited jobs
    for (int i = 0; i < to_remove_iter; i++) {
        jobs.erase(to_remove[i]);
    }
}
JobEntry* JobsList::getJobById(JobID jobId) {
    // remove zombies from jobs list
    removeFinishedJobs();

    if (jobs.count(jobId) == 0) return nullptr;

    // return from map
    return &jobs[jobId];
}
void JobsList::removeJobById(JobID jobId) {
    // if not exist nothing happens
    jobs.erase(jobId);
}

JobEntry* JobsList::getLastJob(JobID* lastJobId) {
    // remove zombies from jobs list
    removeFinishedJobs();

    if (jobs.empty()) return nullptr;

    // return last in map
    auto last_job = jobs.rbegin();
    if (lastJobId) *lastJobId = last_job->first;
    return &(last_job->second);
}
JobEntry* JobsList::getLastStoppedJob(JobID* jobId) {
    // remove zombies from jobs list
    removeFinishedJobs();

    // iterate and find last stopped job return it
    for (auto iter = jobs.rbegin(); iter != jobs.rend(); iter++) {
        if (iter->second.is_stopped) {
            if (jobId) *jobId = iter->first;
            return &(iter->second);
        }
    }

    return nullptr;
}
//-------------------------SPECIAL COMMANDS-------------------------
PipeCommand::PipeCommand(const char* cmd_line, SmallShell* shell) : Command(cmd_line),
                                                                    shell(shell),
                                                                    has_ampersand(false),
                                                                    background(false) {
    // parse
    string command(cmd_line);
    int pipe_index = command.find_first_of("|");

    command1 = _trim(command.substr(0, pipe_index));
    checkAndRemoveAmpersand(command1); // don't run inner command in background

    if (cmd_line[pipe_index + 1] == '&') {
        has_ampersand = true;
        pipe_index++;         // in order for command2 to start after the ampersand
    }

    command2 = _trim(command.substr(pipe_index+1, command.length() - pipe_index - 1));
    if (checkAndRemoveAmpersand(command2)) background = true;   // check for & at the end

    // if the first command is jobs, update jobs because child can't
    if (command1.compare("jobs") == 0 || command1.find("jobs ") == 0) {
        shell->updateJobs();
    }

    // if the second command is jobs, update jobs because child can't
    if (command2.compare("jobs") == 0 || command2.find("jobs ") == 0) {
        shell->updateJobs();
    }
}
void PipeCommand::execute() {
    // if first command fg, just call fg
    if (command1.find("fg ") == 0) {
        shell->executeCommand(command1.c_str());
        return;
    }

    pid_t pid = fork();

    if (pid == 0) { // child process
        if (isSmashChild()) setpgrp();      // make sure that the child gets a different GROUP ID

        int my_pipe[2];
        if (pipe(my_pipe) == -1) {
            perror("smash error: pipe failed");
            exit(0);
        }

        pid_t pid1, pid2;
        bool success = Pipe(my_pipe, &pid1, &pid2);

        // close pipe
        if (close(my_pipe[0]) == -1) perror("smash error: close failed");
        if (close(my_pipe[1]) == -1) perror("smash error: close failed");

        if (!success) {
            pid_t gpid = getpgrp();
            if (gpid < 0) {
                perror("smash error: getpgrp failed");
                exit(0);
            }
            // kill this process and it's children
            if (killpg(gpid, SIGINT) < 0) perror("smash error: killpg failed");
            exit(0);
        }

        if (waitpid(pid1, nullptr, 0) < 0) perror("smash error: waitpid failed");
        if (waitpid(pid2, nullptr, 0) < 0) perror("smash error: waitpid failed");
        exit(0);
    } else if (pid < 0) {
        perror("smash error: fork failed");
        return;
    }

    if (childWait(pid)) return;

    if (background) {   // run in background
        shell->addJob(pid, original_cmd);
    } else {            // run in foreground
        CURR_FORK_CHILD_RUNNING = pid;
        int status;
        // wait for child to finish
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("smash error: waitpid failed");
        } else {
            // add to jobs list if stopped
            if (WIFSTOPPED(status)) shell->addJob(pid, original_cmd, true);
        }
        CURR_FORK_CHILD_RUNNING = 0;
    }
}

bool PipeCommand::Pipe(int my_pipe[], pid_t* pid1, pid_t* pid2) {
    *pid1 = fork();
    if (*pid1 == 0) {    // child process for command1
        // close read channel
        if (close(my_pipe[0]) == -1) perror("smash error: close failed");

        //set the new write channel
        int write_channel = has_ampersand ? STDERR : STDOUT;
        if (dup2(my_pipe[1], write_channel) == -1) {
            perror("smash error: dup2 failed");
            if (close(my_pipe[1]) == -1) perror("smash error: close failed");
            exit(0);
        }

        shell->executeCommand(command1.c_str()); // execute the command before the pipe
        exit(0);

    } else if (*pid1 < 0)  {
        perror("smash error: fork failed");
        return false;
    }

    *pid2 = fork();
    if (*pid2 == 0) {    // child process for command2
        // close write channel
        if (close(my_pipe[1]) == -1) perror("smash error: close failed");

        // set the new read channel
        if (dup2(my_pipe[0], STDIN) == -1) {
            perror("smash error: dup2 failed");
            if (close(my_pipe[0]) == -1) perror("smash error: close failed");
            exit(0);
        }

        shell->executeCommand(command2.c_str()); // execute the command after the pipe
        exit(0);

    }  else if (*pid2 < 0) {
        perror("smash error: fork failed");
        return false;
    }

    return true;
}


RedirectionCommand::RedirectionCommand(const char* cmd_line, SmallShell* shell) :   Command(cmd_line),
                                                                                    shell(shell),
                                                                                    to_append(false),
                                                                                    to_background(false) {
    // find split place
    int split_place = original_cmd.find_first_of(">");

    // check if need to append
    if (cmd_line[split_place+1] == '>') to_append = true;

    // save command part
    cmd_part = _trim(original_cmd.substr(0, split_place));

    // and file address part
    if (to_append) split_place++;
    pathname = _trim(original_cmd.substr(split_place+1));

    // move ampersand from pathname to cmd_part if there is one
    if (checkAndRemoveAmpersand(pathname)) to_background = true;

    // get first argument and ignore everything after it
    int end_of_pathname = pathname.find_first_of(" ");
    pathname = pathname.substr(0,end_of_pathname);

    // check if cmd is built-in command
    cmd_is_built_in = isBuiltInCommand(cmd_part);
}
void RedirectionCommand::execute() {

    // open file, if to_append is true open in append mode
    int flags = O_CREAT | O_WRONLY;
    if (to_append) {
        flags |= O_APPEND;
    } else {
        flags |= O_TRUNC;
    }
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;
    int file_fd = open(pathname.c_str(), flags, mode);
    if (file_fd < 0) { // can't continue
        perror("smash error: open failed");
        return;
    }

    if (cmd_is_built_in) {
        // save old stdout
        auto stdout_fd = dup(STDOUT);
        if (stdout_fd < 0) { // dup error - can't continue
            perror("smash error: dup failed");
            if (close(file_fd) < 0) perror("smash error: close failed");
            return;
        }
        // change stdout to given FD
        if (dup2(file_fd, STDOUT) < 0) { // dup2 error - can't continue
            perror("smash error: dup2 failed");
            if (close(stdout_fd) < 0) perror("smash error: close failed");
            if (close(file_fd) < 0) perror("smash error: close failed");
            return;
        }

        // execute the fg command
        shell->executeCommand(cmd_part.c_str());

        // revert stdout back to old stdout
        if (dup2(stdout_fd, STDOUT) < 0) perror("smash error: dup2 failed");
        if (close(stdout_fd) < 0) perror("smash error: close failed");
        if (close(file_fd) < 0) perror("smash error: close failed");
        return;
    }

    pid_t pid = fork();

    if (pid == 0) { // child
        if (isSmashChild()) setpgrp();  // make sure that the child get different GROUP ID

        // put file descriptor in STDOUT place
        if (dup2(file_fd, STDOUT) < 0) {  // dup2 error - can't continue
            perror("smash error: dup2 failed");
            if (close(file_fd) < 0) perror("smash error: close failed");
            exit(0);
        }

        shell->executeCommand(cmd_part.c_str());

        if (close(file_fd) < 0) perror("smash error: close failed");
        exit(0);

    } else if (pid > 0) { // parent
        if (childWait(pid)) return;

        if (to_background) {    // run in background
            // if with "&" add to JOBS LIST and return
            shell->addJob(pid, original_cmd);
        } else {                // run in foreground
            CURR_FORK_CHILD_RUNNING = pid;
            int status;

            // wait for job
            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            } else {
                // add to jobs list if stopped
                if (WIFSTOPPED(status)) shell->addJob(pid, original_cmd, true);
            }
            CURR_FORK_CHILD_RUNNING = 0;
        }
    } else {
        perror("smash error: fork failed");
    }
}

TimeoutCommand::TimeoutCommand(const char* cmd_line, SmallShell* shell) :   Command(cmd_line),
                                                                            shell(shell),
                                                                            to_background (false),
                                                                            duration(0), cmd_part("") {
    if (!isSmash()) return;

    //parsing
    string tmp;
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 2) {
        bool is_num = true;
        for (int digit = 0; args[1][digit]; digit++) {
            if (!isdigit(args[1][digit])) is_num = false;
        }

        if (is_num) {
            duration = stoi(args[1]);
        }

        if (!is_num || (is_num && duration < 1)) {
            printError("timeout: invalid arguments");
            // duration = 0 so execute() will do nothing
        }

        for (int i = 2; i < num_of_args; i++) {
            cmd_part += string(args[i]);
            cmd_part += " ";
        }
    }
    for (int i = 0; i < num_of_args; i++) free(args[i]);
    if (num_of_args < 3) {  // too few arguments
        printError("timeout: invalid arguments");
        return;
        // cmd_part = "" so execute() will do nothing
    }

    // remove ampersand and check to background
    to_background = checkAndRemoveAmpersand(cmd_part);

    // check if it's built-in command
    cmd_is_built_in = isBuiltInCommand(cmd_part);
}
void TimeoutCommand::execute() {
    if (cmd_part.empty()) return;  // no command to execute
    if (duration < 1) return;      // invalid duration given

    if (cmd_is_built_in) {
        // TODO: what should we do here? (only real use can be with fg or bg)
        shell->executeCommand(cmd_part.c_str());
        return;
    }

    pid_t pid = fork();

    if (pid == 0) { // child
        if (isSmashChild()) setpgrp();  // make sure that the child get different GROUP ID

        shell->executeCommand(cmd_part.c_str());
        exit(0);

    } else if (pid > 0) { // parent

        // add the timeout command to the jobs list as a timeout job
        JobEntry* job_entry = shell->addJob(pid, original_cmd, false, true, duration);

       // update alarm
       time_t curr_time = time(nullptr);
       if (curr_time == (time_t)(-1)) {
           perror("smash error: time failed");
       } else if (TIME_UNTIL_NEXT_ALARM < numeric_limits<double>::max()) {
           // if there is an upcoming alarm
           // update the leftover duration
           TIME_UNTIL_NEXT_ALARM -= difftime(curr_time, TIME_AT_LAST_UPDATE);
       }

       // if the time limit (of the current command) is less than the time until next alarm
       if (duration < (int)TIME_UNTIL_NEXT_ALARM) {
           // set new alarm (corresponding to this command)
           alarm(duration);

           // update time until next alarm
           TIME_UNTIL_NEXT_ALARM = duration;
       }
       TIME_AT_LAST_UPDATE = curr_time;

        if (!to_background) {
            CURR_FORK_CHILD_RUNNING = pid;
            int status;

            // wait for job
            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            } else {
                if (WIFSTOPPED(status)) {
                    // set as stopped if stopped
                    // (it's already in jobs list)
                    job_entry->is_stopped = true;

                    // reset the job's timer
                    // (this is when it's supposed to have been added to the job's list)
                    job_entry->SetTime();
                } else {
                    // finished -> set to remove from jobs list
                    job_entry->pid = 0;
                }
            }
            CURR_FORK_CHILD_RUNNING = 0;
        }
    } else {
        perror("smash error: fork failed");
    }
}


//---------------------------EXTERNAL CLASS------------------------------
ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) :    Command(cmd_line),
                                                                            cmd_to_son(cmd_line),
                                                                            jobs(jobs),
                                                                            to_background(false) {
    if (checkAndRemoveAmpersand(cmd_to_son)) to_background = true;
}
void ExternalCommand::execute() {
    pid_t pid = fork();

    if (pid == 0) { //child:
        if (isSmashChild()) setpgrp();  // make sure that the child get different GROUP ID

        // exec to bash with cmd_line
        string arg1 = "-c";
        if (execl("/bin/bash", "/bin/bash", arg1.c_str(), cmd_to_son.c_str(), (char*) nullptr) < 0) {
            perror("smash error: execl failed");
        }
        exit(0);    // child finished (if execl failed)
    }
    else if (pid > 0) { //parent
        if (childWait(pid)) return;

        if (to_background) {    // run in background
            // if with "&" add to JOBS LIST and return
            jobs->addJob(pid, original_cmd);
        } else {                // run in foreground

            CURR_FORK_CHILD_RUNNING = pid;
            int status;

            // wait for job
            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            } else {
                // add to jobs list if stopped
                if (WIFSTOPPED(status)) jobs->addJob(pid, original_cmd, true);
            }
            CURR_FORK_CHILD_RUNNING = 0;
        }
    }
    else { // fork failed
        perror("smash error: fork failed");
    }
}

//---------------------------BUILT IN CLASSES------------------------------
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, SmallShell* shell) : BuiltInCommand(cmd_line),
                                                                                    shell(shell),
                                                                                    prompt("smash") {
    // no argument = change to default prompt "smash"
    // otherwise, get the new prompt text
    string tmp;
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 1) tmp = args[1]; // save prompt string
    for (int i = 0; i < num_of_args; i++) free(args[i]);

    // remove ampersand
    checkAndRemoveAmpersand(tmp);
    if (!tmp.empty()) prompt = tmp;
}
void ChangePromptCommand::execute() {
    // change shell prompt
    shell->changePrompt(prompt);
}

void ShowPidCommand::execute() {
    // print shell's pid
    std::cout << "smash pid is " << SMASH_PROCESS_PID << endl;
}

void GetCurrDirCommand::execute() {
    char* dir = getcwd(nullptr, COMMAND_MAX_CHARS + 1);
    if (!dir) {
        perror("smash error: getcwd failed");
    } else {
        std::cout << dir << endl;
        free(dir);
    }
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, string* last_dir) : BuiltInCommand(cmd_line),
                                                                             old_pwd(last_dir),
                                                                             new_path("") {
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);

    if (num_of_args > 2) { // more than one argument
        printError("cd: too many arguments");
    } else if (num_of_args == 2) {
        new_path = args[1];
    } // else new_path = "";  // command is "cd" without arguments

    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void ChangeDirCommand::execute() {
    // get current directory to save after
    char* dir = getcwd(nullptr, COMMAND_MAX_CHARS + 1);
    if (!dir) {
        perror("smash error: getcwd failed");
        return;
    }
    string updated_old_pwd(dir);
    free(dir);

    if (new_path.empty()) { // no path given (no arguments)
        *old_pwd = updated_old_pwd;
        return;
    }

    int retVAl = 0;
    if (new_path == "-") // change to last directory
    {
        if (old_pwd->empty()) {
            // if no old_pwd print error
            printError("cd: OLDPWD not set");
        } else {
            retVAl = chdir(old_pwd->c_str());
        }
    }
    else retVAl = chdir(new_path.c_str()); // change to given path

    if (retVAl == 0) {
        // if directory was changed successfully
        // update the last directory in the shell
        *old_pwd = updated_old_pwd;
    } else if (retVAl == -1) {
        // directory change error
        perror("smash error: chdir failed");
    }
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                                 jobs(jobs) {
}
void JobsCommand::execute() {
    jobs->printJobsList();
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) :    BuiltInCommand(cmd_line),
                                                                    jobs(jobs),
                                                                    signum(0),
                                                                    job_id(0),
                                                                    job_entry(nullptr) {
    // parse: type of signal and jobID, if syntax not valid print error
    if (!parseAndCheck(cmd_line, &signum, &job_id)) {
        printError("kill: invalid arguments");
        return;
    }

    // if job id not exist print error message
    job_entry = jobs->getJobById(job_id);
    if (!job_entry) {
        printJobError();
        job_id = 0;
        return;
    }
}
void KillCommand::execute() {
    if (job_id == 0 || signum == 0 || !job_entry) return;

    pid_t gpid = getpgid(job_entry->pid);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal to the process group
    if (killpg(gpid, signum) < 0) { // can't continue
        perror("smash error: killpg failed");
        return;
    }

    // print message reporting signal was sent
    printSignalSent();

    // if signal was SIGSTOP or SIGTSTP update job state to stopped
    if (signum == SIGSTOP || signum == SIGTSTP) job_entry->is_stopped = true;

    // if signal was SIGCONT update job state to not stopped
    if (signum == SIGCONT) job_entry->is_stopped = false;
}
bool KillCommand::parseAndCheck(const char* cmd_line, int* sig, JobID* j_id) {
    string first_arg, second_arg;

    // parse
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args == 3) {
        first_arg = args[1];
        second_arg = args[2];
    }
    for (int i = 0; i < num_of_args; i++) free(args[i]);
    if (num_of_args != 3) return false;

    // check first argument
    if ((int)first_arg.size() < 2) return false;
    if ((int)first_arg.size() > 3) return false;
    if (first_arg[0] != '-') return false;
    if (!isdigit(first_arg[1])) return false;
    if ((int)first_arg.size() == 3 && !isdigit(first_arg[2])) return false;
    *sig = stoi(first_arg.substr(1));
    if (*sig < 0 || *sig > 31) return false;

    // check second argument
    if ((int)second_arg.size() > 10) return false;
    for (auto letter : second_arg) if (!isdigit(letter)) return false;
    long job = stol(second_arg);
    if (job > numeric_limits<int>::max() || job < 1) return false;

    // all OK
    *j_id = (int)job;
    return true;
}
void KillCommand::printJobError() {
    string str = "kill: job-id ";
    str += to_string(job_id);
    str += " does not exist";
    printError(str);
}
void KillCommand::printSignalSent() {
    pid_t p = job_entry->pid;

    string str = "signal number ";
    str += to_string(signum);
    str += " was sent to pid ";
    str += to_string((int)p);
    std::cout << str << std::endl;
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) :    BuiltInCommand(cmd_line),
                                                                                job_id(1),
                                                                                jobs(jobs),
                                                                                job_entry(nullptr) {
    // if num of argument not valid or syntax problem print error
    parseAndCheckFgBgCommands(cmd_line, job_id, no_args, invalid_args);
    if (invalid_args) {
        if (job_id < 1) {
            printJobError();
        } else {
            printError("fg: invalid arguments");
        }

        return;
    }

    if (no_args) { // no arguments, get last job
        // get the last job
        job_entry = jobs->getLastJob(&job_id);
    } else {
        job_entry = jobs->getJobById(job_id);
        if (!job_entry) {
            printJobError();
            invalid_args = true;
        }
    }
}
void ForegroundCommand::execute() {
    if (invalid_args) return; // error in arguments or job not exist

    if (!job_entry) { // no arguments given + jobs list is empty
        printError("fg: jobs list is empty");
        return;
    }

    pid_t pid = job_entry->pid;
    string cmd_str = job_entry->cmd_str;

    // print job's command line
    cout << cmd_str << " : " << pid << endl;

    // update state to not stopped
    job_entry->is_stopped = false;

    // send SIGCONT to job's pid
    pid_t gpid = getpgid(pid);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal, print message
    if (killpg(gpid, SIGCONT) < 0) { // can't continue
        perror("smash error: killpg failed");
        return;
    }

    CURR_FORK_CHILD_RUNNING = pid;
    int status;

    // wait for job
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("smash error: waitid failed");
    } else {
        if (WIFSTOPPED(status)) { // if it gets stopped
            // reset process' time
            job_entry->start_time = time(nullptr);
            if (job_entry->start_time == (time_t)(-1)) perror("smash error: time failed");

            // update 'stopped' status
            job_entry->is_stopped = true;

        } else { // if it it finished
            jobs->removeJobById(job_id);    // remove from jobs list
        }
    }
    CURR_FORK_CHILD_RUNNING = 0;
}

void ForegroundCommand::printJobError() {
    string str = "fg: job-id ";
    str += to_string(job_id);
    str += " does not exist";
    printError(str);
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                                             job_id(1),
                                                                             jobs(jobs),
                                                                             job_entry(nullptr) {
    // if num of argument not valid or syntax problem print error
    parseAndCheckFgBgCommands(cmd_line, job_id, no_args, invalid_args);
    if (invalid_args) {
        if (job_id < 1) {
            printJobError();
        } else {
            printError("bg: invalid arguments");
        }

        return;
    }

    if (no_args) { // no arguments, get last job
        job_entry = jobs->getLastStoppedJob(&job_id);
    } else {
        job_entry = jobs->getJobById(job_id);
        if (!job_entry) {
            printJobError();
            invalid_args = true;
            return;
        }
        if (!job_entry->is_stopped) {
            printNotStoppedError();
            invalid_args = true;
            return;
        }
    }
}
void BackgroundCommand::execute() {
    if (invalid_args) return; // error in arguments or job not exist

    if (!job_entry) {   // no arguments given + no stopped jobs to resume
        printError("bg: there is no stopped jobs to resume");
        return;
    }

    // print job's command line
    cout << job_entry->cmd_str << " : " << job_entry->pid << endl;

    // send SIGCONT to job's pid
    // update is_stopped

    pid_t gpid = getpgid(job_entry->pid);
    if (gpid < 0) {
        perror("smash error: getgpid failed");
        return;
    }

    // send signal, print message
    if (killpg(gpid, SIGCONT) < 0) {
        // can't continue
        perror("smash error: killpg failed");
        return;
    } else {
        // update 'stopped' status
        job_entry->is_stopped = false;
    }
}

void BackgroundCommand::printJobError() {
    string str = "bg: job-id ";
    str += to_string(job_id);
    str += " does not exist";
    printError(str);
}

void BackgroundCommand::printNotStoppedError() {
    string str = "bg: job-id ";
    str += to_string(job_id);
    str += " is already running in the background";
    printError(str);
}

void parseAndCheckFgBgCommands(const char* cmd_line, JobID& job_id, bool& no_args, bool& invalid_args) {
    string arg;
    no_args = false;
    invalid_args = false;

    // parse
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args == 2) arg = args[1];
    for (int i = 0; i < num_of_args; i++) free(args[i]);

    if (num_of_args > 2) {
        invalid_args = true;
        return;
    }
    if (num_of_args == 1) {
        no_args = true;
        return;
    }

    int iter = 0;
    if (arg[0] == '-') iter++;
    while (iter < (int)arg.size()) {
        if (!isdigit(arg[iter++])) {
            invalid_args = true;
            return;
        }
    }
    job_id = stol(arg);
    if (job_id > numeric_limits<int>::max() || job_id < 1) {
        invalid_args = true;
        return;
    }
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) :    BuiltInCommand(cmd_line),
                                                                    kill_all(false),
                                                                    jobs(jobs) {
    // parse
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    for (int i = 0; i < num_of_args; i++) {
        if (i > 0 && string(args[i]) == "kill") kill_all = true;
        free(args[i]);
    }
}
void QuitCommand::execute() {
    if (kill_all) jobs->killAllJobs();
    QUIT_SHELL = true;
}

CopyCommand::CopyCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                                 old_path(""),
                                                                 new_path(""),
                                                                 background(false),
                                                                 jobs(jobs) {
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 2) {
        old_path = args[1];
        new_path = args[2];
        if (checkAndRemoveAmpersand(new_path)) background = true;
    }
    if (*args[num_of_args-1] == '&') background = true;
    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void CopyCommand::execute() {
    // too few arguments or empty string given as an argument
    if (old_path.empty() || new_path.empty()) return;
    if (old_path == new_path) {
        cout << "smash: " << old_path << " was copied to " << new_path << endl;
        return;
    }

    // open read file (where the user wants to copy from)
    int read_flags = O_RDONLY;
    int fd_read = open(old_path.c_str(), read_flags);
    if (fd_read == -1) {
        perror("smash error: open failed");
        return;
    }

    // open write file (where the user wants to copy to)
    int write_flags = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode = S_IRWXU | S_IRWXG | S_IRWXO;

    int fd_write = open(new_path.c_str(), write_flags, mode);
    if (fd_write == -1)  {
        perror("smash error: open failed");
        if (close(fd_read) == -1) perror("smash error: close failed");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) { // copy data in child process
        if (isSmashChild()) setpgrp();  // make sure that the child get different GROUP ID

        // copying will stop if SIGTSTP is received
        if (signal(SIGTSTP, SIG_DFL) == SIG_ERR){
            perror("smash error: failed to set SIGTSTP handler");
        }

        int SIZE = COPY_DATA_BUFFER_SIZE;

        bool copy_success = true;
        char buff[SIZE];
        ssize_t read_retVal = read(fd_read, buff, SIZE);
        ssize_t write_retVal;
        while (read_retVal > 0) {   // while there is something to write
            write_retVal = write(fd_write, buff, read_retVal);
            if (write_retVal == -1) {
                perror("smash error: write failed");
                copy_success = false;
            }

            // check that the read size equals the write size
            if (write_retVal != read_retVal) {
                perror("smash error: incomplete write");
                copy_success = false;
            }

            read_retVal = read(fd_read, buff, SIZE);
        }

        if (read_retVal == -1) {
            perror("smash error: read failed");
            copy_success = false;
        }

        if (copy_success) {
            cout << "smash: " << old_path << " was copied to " << new_path << endl;
        }

    } else if (pid < 1) perror("smash error: fork failed");

    // both parent and child close the read/write channels
    if (close(fd_read) == -1) perror("smash error: close failed");
    if (close(fd_write) == -1) perror("smash error: close failed");

    if (pid == 0) exit(0);  // child process finished

    // only parent process continues from here

    if (pid < 1) return;    // fork failed, parent process returns

    if (childWait(pid)) return;

    if (background)     // run in background
        // & was given - add to jobs list
        jobs->addJob(pid, original_cmd);
    else {              // run in foreground
        int status;
        CURR_FORK_CHILD_RUNNING = pid;

        // wait for child process
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("smash error: waitpid failed");
        } else if (WIFSTOPPED(status)) {
            // if stopped add to jobs list
            jobs->addJob(pid, original_cmd, true);
        }

        CURR_FORK_CHILD_RUNNING = 0;
    }
}

//---------------------------SMALL SHELL--------------------------------------
SmallShell::SmallShell() : prompt("smash"), old_pwd("") {
    jobs = new JobsList();
    CURR_FORK_CHILD_RUNNING = 0;
    GLOBAL_JOBS_POINTER = jobs;
}

SmallShell::~SmallShell() {
    delete jobs;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _trim(string(cmd_line));
    if (cmd_s.compare("timeout") == 0 || cmd_s.compare("timeout&") == 0 || cmd_s.find("timeout ") == 0) {
        return new TimeoutCommand(cmd_line, this);
    } else if (cmd_s.find("|") != string::npos) {
        return new PipeCommand(cmd_line, this);
    } else if (cmd_s.find(">") != string::npos) {
        return new RedirectionCommand(cmd_line, this);
    } else if (cmd_s.compare("chprompt") == 0 || cmd_s.compare("chprompt&") == 0|| cmd_s.find("chprompt ") == 0) {
        return new ChangePromptCommand(cmd_line, this);
    } else if (cmd_s.compare("showpid") == 0 || cmd_s.compare("showpid&") == 0  || cmd_s.find("showpid ") == 0) {
        return new ShowPidCommand(cmd_line);
    } else if (cmd_s.compare("pwd") == 0 || cmd_s.compare("pwd&") == 0 ||cmd_s.find("pwd ") == 0) {
        return new GetCurrDirCommand(cmd_line);
    } else if (cmd_s.compare("cd") == 0 || cmd_s.compare("cd&") == 0 || cmd_s.find("cd ") == 0) {
        return new ChangeDirCommand(cmd_line, &this->old_pwd);
    } else if (cmd_s.compare("jobs") == 0 || cmd_s.compare("jobs&") == 0 || cmd_s.find("jobs ") == 0) {
        return new JobsCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("kill") == 0 || cmd_s.compare("kill&") == 0 || cmd_s.find("kill ") == 0) {
        return new KillCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("fg") == 0 || cmd_s.compare("fg&") == 0 ||cmd_s.find("fg ") == 0) {
        return new ForegroundCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("bg") == 0 || cmd_s.compare("bg&") == 0 ||cmd_s.find("bg ") == 0) {
        return new BackgroundCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("quit") == 0 || cmd_s.compare("quit&") == 0 || cmd_s.find("quit ") == 0) {
        return new QuitCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("cp") == 0 || cmd_s.compare("cp&") == 0 || cmd_s.find("cp ") == 0) {
        return new CopyCommand(cmd_line, this->jobs);
    } else {
        return new ExternalCommand(cmd_line, this->jobs);
    }
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command *cmd = CreateCommand(cmd_line);
    cmd->execute();
    delete cmd;
}

void SmallShell::changePrompt(const string& str) {
    if (str.empty()) prompt = "smash";
    else prompt = str;
}

const string& SmallShell::getPrompt() {
    return prompt;
}

JobEntry* SmallShell::addJob(pid_t pid, const string& str, bool is_stopped, bool is_timeout, unsigned int time_limit) {
    return jobs->addJob(pid, str, is_stopped, is_timeout, time_limit);
}

void SmallShell::updateJobs() {
    jobs->removeFinishedJobs();
}
