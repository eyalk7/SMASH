#include <unistd.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include "Commands.h"
#include <limits>

using namespace std;

// definition of CURR_FORK_CHILD_RUNNING
pid_t CURR_FORK_CHILD_RUNNING = 0;

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

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}
string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}
string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}
int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = nullptr;
  }
  return i;

  FUNC_EXIT()
}
bool _isBackgroundCommand(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}
void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

bool checkAndRemoveAmpersand(string& str) {
    if (str.empty()) return false;
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);

    // if all characters are spaces then return
    if (idx == string::npos) {
        return false;
    }

    // if the command line does not end with & then return
    if (str[idx] != '&') {
        return false;
    }

    // erase
    str.erase(idx);

    return true;
}

//---------------------------JOBS LISTS------------------------------
JobEntry::JobEntry(pid_t pid, const string& cmd_str, bool is_stopped) :  pid(pid),
                                                                         cmd_str(cmd_str),
                                                                         is_stopped(is_stopped) {
    start_time = time(nullptr);
    if (start_time == (time_t)(-1)) perror("smash error: time failed");

}
void JobsList::addJob(pid_t pid, const string& cmd_str, bool is_stopped) {
    // remove zombies from jobs list
    removeFinishedJobs();

    // create new job entry
    JobEntry new_job(pid, cmd_str, is_stopped);
    JobID new_id = 1;
    if (!jobs.empty()) new_id = jobs.rbegin()->first + 1;

    // insert to map
    jobs[new_id] = new_job;
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
    cout << "smash: sending SIGKILL signal to " << jobs.size() << " jobs:" << endl;

    // interate on map, print message and send SIGKILL than wait them
    for (const auto& job : jobs) {
        cout << job.second.pid << " " << job.second.cmd_str << endl;
        if (kill(job.second.pid, SIGKILL) < 0) perror("smash error: kill failed");
        if (waitpid(job.second.pid, nullptr, 0) < 0) perror("smash error: waitpid failed");
    }
}
void JobsList::removeFinishedJobs() {
    vector<JobID> to_remove(100,0); // MAX_PROCESS_COUNT ?
    int to_remove_iter= 0;

    // iterate on map, and waitpid with each pid WNOHANG flag
    for (const auto& job : jobs) {
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
    // remove zombies from jobs list
    removeFinishedJobs();

    if (jobs.count(jobId) > 0) {
        jobs.erase(jobId);
    }
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
                                                                    shell(shell), has_ampersand(false), background(false) {
    string command(cmd_line);
    int pipe_index = command.find_first_of("|");

    command1 = command.substr(0, pipe_index);

    if (cmd_line[pipe_index + 1] == '&') {
        has_ampersand = true;
        pipe_index++;         // in order for command2 to start after the ampersand
    }

    command2 = command.substr(pipe_index+1, command.length() - pipe_index - 1);
}
void PipeCommand::execute() {
    int my_pipe[2];
    if (pipe(my_pipe) == -1) perror("smash error: pipe failed");

    pid_t pid1 = fork();
    if (pid1 == 0) {    // child process for command1
        setpgrp();              // make sure that the child get different GROUP ID

        // close read channel
        if (close(my_pipe[0]) == -1) perror("smash error: close failed");

        //set the new write channel
        int write_channel = has_ampersand ? STDERR : STDOUT;
        if (dup2(my_pipe[1], write_channel) == -1) perror("smash error: dup2 failed");

        shell->executeCommand(command1.c_str()); // execute the command before the pipe
    } else if (pid1 < 1) perror("smash error: fork failed");
    if (pid1 < 1) exit(0); // first child finished

    pid_t pid2 = fork();
    if (pid2 == 0) {    // child process for command2
        setpgrp();              // make sure that the child get different GROUP ID

        // close write channel
        if (close(my_pipe[1]) == -1) perror("smash error: close failed");

        // set the new read channel
        if (dup2(my_pipe[0], STDIN) == -1) perror("smash error: dup2 failed");

        shell->executeCommand(command2.c_str()); // execute the command after the pipe
    } else if (pid2 < 1) perror("smash error: fork failed");
    if (pid2 < 1) exit(0); // second child finished

    if (close(my_pipe[0]) == -1) perror("smash error: close failed");
    if (close(my_pipe[1]) == -1) perror("smash error: close failed");

    if (background) {
        shell->addJob(pid1, command1);
        shell->addJob(pid2, command2);
    } else {
        int status;

        CURR_FORK_CHILD_RUNNING = pid2;
        if (waitpid(pid2, &status, WUNTRACED) < 0) {  // wait for the second command to finish
            perror("smash error: waitpid failed");
        } else if (WIFSTOPPED(status)) { // if SIGSTOP was called
            // if SIGSTOP was called
            // check if command1 finished
            if (waitpid(pid1, &status, WNOHANG) < 0) {
                perror("smash error: waitpid failed");
            } else if (!WIFEXITED(status)) {
                // if command1 didn't finish, stop it
                // and add it to the jobs list
                if (kill(pid1, SIGTSTP) < 0) perror("smash error: kill failed");
                else shell->addJob(pid1, command1, true);
            }

            // add command2 to the jobs list
            shell->addJob(pid2, command2, true);
        }

        CURR_FORK_CHILD_RUNNING = 0;
    }
}
RedirectionCommand::RedirectionCommand(const char* cmd_line, SmallShell* shell) :   Command(cmd_line),
                                                                                    shell(shell),
                                                                                    to_append(false) {
    // find split place
    int split_place = 0;
    while (cmd_line[split_place] && cmd_line[split_place] != '>') split_place++;
    if (cmd_line[split_place+1] == '>') to_append = true;

    // save command part
    cmd_part = string(cmd_line, split_place);

    // and file address part
    if (to_append) split_place++;
    pathname = cmd_line+split_place+1;
    pathname = _ltrim(pathname);
}
void RedirectionCommand::execute() {
    // open file, if to_append == true open in append mode
    int flags = O_CREAT | O_WRONLY;
    if (to_append) flags |= O_APPEND;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    int file_fd = open(pathname.c_str(), flags, mode);
    if (file_fd < 0) perror("smash error: open failed");

    // move stdout to other file descriptor
    int stdout_fd = dup(STDOUT);
    if (stdout_fd < 0) perror("smash error: dup failed");

    // put file descriptor in 1st place
    if (dup2(file_fd, STDOUT) < 0) perror("smash error: dup2 failed");

    // shell.execute the command
    shell->executeCommand(cmd_part.c_str());

    // restore stdout and close all new file descriptors
    if (dup2(stdout_fd, STDOUT) < 0) perror("smash error: dup2 failed");
    if (close(stdout_fd) < 0) perror("smash error: close failed");
    if (close(file_fd) < 0) perror("smash error: close failed");
}
//---------------------------EXTERNAL CLASS------------------------------
ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) :    Command(cmd_line),
                                                                            jobs(jobs),
                                                                            to_background(false),
                                                                            cmd_to_son(cmd_line) {
    if (checkAndRemoveAmpersand(cmd_to_son)) to_background = true;
}
void ExternalCommand::execute() {
    pid_t pid = fork();

    if (pid == 0) { //child:
        setpgrp(); // no possible errors

        // exec to bash with cmd_line
        string arg1 = "-c";
        if (execl("/bin/bash", "/bin/bash", arg1.c_str(), cmd_to_son.c_str(), (char*) nullptr) < 0) {
            perror("smash error: execl failed");
        }
        exit(0);    // child finished (if execl failed)
    }
    else if (pid > 0) { //parent
        // if with "&" add to JOBS LIST and return
        if (to_background) {
            jobs->addJob(pid, cmd_line);
        } else {
            // wait for job
            // add to jobs list if stopped
            CURR_FORK_CHILD_RUNNING = pid;
            int status;
            if (waitpid(pid, &status, WUNTRACED) < 0) {
                perror("smash error: waitpid failed");
            } else {
                if (WIFSTOPPED(status)) jobs->addJob(pid, cmd_line, true);
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
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 1) prompt = args[1]; // save prompt string
    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void ChangePromptCommand::execute() {
    // change shell prompt
    shell->changePrompt(prompt);
}

void ShowPidCommand::execute() {
    // print shell's pid
    std::cout << "smash pid is " << getpid() << endl;
}

void GetCurrDirCommand::execute() {
    char* dir = getcwd(nullptr, COMMAND_MAX_CHARS + 1);
    std::cout << dir << endl;
    free(dir);
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, string* last_dir) : BuiltInCommand(cmd_line),
                                                                             new_path(""), old_pwd(last_dir) {
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);

    if (num_of_args > 2) // more than one argument
        std::cout <<"smash error: cd: too many arguments" << std::endl;
    else if (num_of_args == 2)
        new_path = args[1];
    // else new_path = ""; (in initializer list)

    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void ChangeDirCommand::execute() {
    if (new_path.empty()) return; // no path given

    // get current directory to save after
    char* dir = getcwd(nullptr, COMMAND_MAX_CHARS + 1);
    string updated_old_pwd(dir);
    free(dir);

    int retVAl = 0;
    if (new_path == "-") // change to last directory
    {
        if (old_pwd->empty()) // if no old_pwd print error
            std::cout << "smash error: cd: OLDPWD not set" << std::endl;
        else
            retVAl = chdir(old_pwd->c_str());
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
                                                                    signum(0),
                                                                    job_id(0),
                                                                    jobs(jobs) {
    // parse: type of signal and jobID, if syntax not valid print error
    if (!parseAndCheck(cmd_line, &signum, &job_id)) {
        printArgumentsError();
        return;
    }

    // if job id not exist print error message
    auto job_entry = jobs->getJobById(job_id);
    if (!job_entry) {
        printJobError();
        job_id = 0;
        return;
    }
}
void KillCommand::execute() {
    if (job_id == 0 || signum == 0) return;
    auto job_entry = jobs->getJobById(job_id);

    // send signal, print message
    if (kill(job_entry->pid, signum) < 0) perror("smash error: kill failed");
    printSignalSent(signum, job_entry->pid);

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
    if (*sig < 1 || *sig > 31) return false;

    // check second argument
    if ((int)second_arg.size() > 10) return false;
    for (auto letter : second_arg) if (!isdigit(letter)) return false;
    long job = stol(second_arg);
    if (job > numeric_limits<int>::max() || job < 1) return false;

    // all OK
    *j_id = (int)job;
    return true;
}
void KillCommand::printArgumentsError() {
    cout << "smash error: kill: invalid arguments" << endl;
}
void KillCommand::printJobError() {
    cout << "smash error: kill: job-id " << job_id << " does not exist" << endl;
}
void KillCommand::printSignalSent(int sig, pid_t p) {
    cout << "signal number " << sig << " was sent to pid " << p << endl;
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) :    BuiltInCommand(cmd_line),
                                                                                job_id(0),
                                                                                jobs(jobs) {
    // if num of argument not valid or syntax problem print error
    if (!parseAndCheckFgBgCommands(cmd_line, &job_id)) {
        printArgumentsError();
        job_id = -1;
        return;
    }

    auto job_entry = jobs->getJobById(job_id);
    if (!job_entry) {
        printJobError(job_id);
        job_id = -1;
        return;
    }
}
void ForegroundCommand::execute() {
    if (job_id < 0) return; // error in arguments or job not exist

    JobEntry* job;
    // if job_id == 0 than get the last job
    // if jobs list empty print error
    if (job_id == 0) {
        job = jobs->getLastJob(&job_id);
        if (!job) {
            printNoJobsError();
            return;
        }
    } else {
        job = jobs->getJobById(job_id);
    }
    int pid = job->pid;
    string cmd_str = job->cmd_str;

    // remove from jobs list
    jobs->removeJobById(job_id);

    // print job's command line
    cout << cmd_str << " : " << pid << endl;

    // send SIGCONT to job's pid
    if (kill(pid, SIGCONT) < 0) perror("smash error: kill failed");

    // wait for job
    // add to jobs list if stopped
    CURR_FORK_CHILD_RUNNING = pid;
    int status;
    if (waitpid(pid, &status, WUNTRACED) < 0) {
        perror("smash error: waitpid failed");
    } else {
        if (WIFSTOPPED(status)) jobs->addJob(pid, cmd_str, true);
    }
    CURR_FORK_CHILD_RUNNING = 0;
}
void ForegroundCommand::printArgumentsError() {
    cout << "smash error: fg: invalid arguments" << endl;
}
void ForegroundCommand::printJobError(JobID job_id) {
    cout << "smash error: fg: job-id " << job_id << " does not exist" << endl;
}
void ForegroundCommand::printNoJobsError() {
    cout << "smash error: fg: jobs list is empty" << endl;
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                                             job_id(0),
                                                                             jobs(jobs) {
    // if num of argument not valid or syntax problem print error
    if (!parseAndCheckFgBgCommands(cmd_line, &job_id)) {
        printArgumentsError();
        job_id = -1;
        return;
    }

    auto job_entry = jobs->getJobById(job_id);
    if (!job_entry) {
        printJobError(job_id);
        job_id = -1;
        return;
    }
    if (!job_entry->is_stopped) {
        printNotStoppedError(job_id);
        job_id = -1;
        return;
    }
}
void BackgroundCommand::execute() {
    if (job_id < 0) return; // error in arguments or job not exist

    JobEntry* job = nullptr;
    // if job_id == 0 than get the last job
    // if jobs list empty print error
    if (job_id == 0) {
        job = jobs->getLastStoppedJob(&job_id);
        if (!job) {
            printNoJobsError();
            return;
        }
    } else {
        job = jobs->getJobById(job_id);
    }

    // print job's command line
    cout << job->cmd_str << " : " << job->pid << endl;

    // send SIGCONT to job's pid
    // update is_stopped
    job->is_stopped = false;
    if (kill(job->pid, SIGCONT) < 0) perror("smash error: kill failed");
}
void BackgroundCommand::printArgumentsError() {
    cout << "smash error: b g: invalid arguments" << endl;
}
void BackgroundCommand::printJobError(JobID job_id) {
    cout << "smash error: bg: job-id " << job_id << " does not exist" << endl;
}
void BackgroundCommand::printNoJobsError() {
    cout << "smash error: bg: there is no stopped jobs to resume" << endl;
}
void BackgroundCommand::printNotStoppedError(JobID job_id) {
    cout << "smash error: bg: job-id " << job_id << " is already running in the background" << endl;
}

bool parseAndCheckFgBgCommands(const char* cmd_line, JobID* job_id) {
    string arg;

    // parse
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args == 2) arg = args[1];
    for (int i = 0; i < num_of_args; i++) free(args[i]);

    if (num_of_args > 2) return false;
    if (num_of_args == 1) return true;

    for (auto letter : arg) if (!isdigit(letter)) return false;
    long job = stol(arg);
    if (job > numeric_limits<int>::max() || job < 1) return false;

    // all OK
    *job_id = (int)job;
    return true;
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) :    BuiltInCommand(cmd_line),
                                                                    kill_all(false),
                                                                    jobs(jobs) {
    // parse
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 1) {
        string arg = args[1];
        if (arg == "kill") kill_all = true;
    }
    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void QuitCommand::execute() {
    if (kill_all) jobs->killAllJobs();
    exit(0);
}

CopyCommand::CopyCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line),
                                                 old_path(""), new_path(""),
                                                 background(false), jobs(jobs) {
    char* args[COMMAND_MAX_ARGS+1];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 3) {
        old_path = args[1];
        new_path = args[2];
    }
    if (*args[num_of_args-1] == '&') background = true;
    for (int i = 0; i < num_of_args; i++) free(args[i]);
}
void CopyCommand::execute() {
    if (old_path.empty() || new_path.empty()) return;

    int read_flags = O_RDONLY | O_EXCL;
    int fd_read = open(old_path.c_str(), read_flags);
    if (fd_read == -1) perror("smash error: open failed");

    int write_flags = O_WRONLY | O_CREAT | O_TRUNC;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

    int fd_write = open(new_path.c_str(), write_flags, mode);
    if (fd_write == -1) perror("smash error: open failed");

    pid_t pid = fork();
    if (pid == 0) { // copy data in child process
        setpgrp();

        int SIZE = COPY_DATA_BUFFER_SIZE;

        char buff[SIZE];
        int read_retVal = read(fd_read, buff, SIZE);
        int write_retVal;
        while (read_retVal > 0) {
            write_retVal = write(fd_write, buff, read_retVal);
            if (write_retVal == -1) perror("smash error: write failed");
            if (write_retVal != read_retVal) perror("smash error: incomplete write");

            read_retVal = read(fd_read, buff, SIZE);
        }

        if (read_retVal == -1) perror("smash error: read failed");
    } else if (pid < 1) perror("smash error: fork failed");

    // both parent and child close the read/write channels
    if (close(fd_read) == -1) perror("smash error: close failed");
    if (close(fd_write) == -1) perror("smash error: close failed");

    if (pid < 1) exit(0);   // child process finished

    // only parent process continues from here

    if (background)     // run in background
        jobs->addJob(pid, cmd_line);
    else {              // run in foreground
        int status;
        CURR_FORK_CHILD_RUNNING = pid;
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            perror("smash error: waitpid failed");
        } else if (WIFSTOPPED(status)) {
            jobs->addJob(pid, cmd_line, true);
        }

        CURR_FORK_CHILD_RUNNING = 0;
    }
}
//---------------------------END OF BUILT IN--------------------------------

//---------------------------SMALL SHELL--------------------------------------
SmallShell::SmallShell() : prompt("smash"), old_pwd("") {
    jobs = new JobsList();
    CURR_FORK_CHILD_RUNNING = 0;
}

SmallShell::~SmallShell() {
    delete jobs;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char* cmd_line) {
    string cmd_s = _ltrim(string(cmd_line));
    if (cmd_s.find("|") != string::npos) {
        return new PipeCommand(cmd_line, this);
    } else if (cmd_s.find(">") != string::npos) {
        return new RedirectionCommand(cmd_line, this);
    } else if (cmd_s.compare("chprompt") == 0 || cmd_s.find("chprompt ") == 0) {
        return new ChangePromptCommand(cmd_line, this);
    } else if (cmd_s.compare("showpid") == 0 || cmd_s.find("showpid ") == 0) {
        return new ShowPidCommand(cmd_line);
    } else if (cmd_s.compare("pwd") == 0 || cmd_s.find("pwd ") == 0) {
        return new GetCurrDirCommand(cmd_line);
    } else if (cmd_s.compare("cd") == 0 || cmd_s.find("cd ") == 0) {
        return new ChangeDirCommand(cmd_line, &this->old_pwd);
    } else if (cmd_s.compare("jobs") == 0 || cmd_s.find("jobs ") == 0) {
        return new JobsCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("kill") == 0 || cmd_s.find("kill ") == 0) {
        return new KillCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("fg") == 0 || cmd_s.find("fg ") == 0) {
        return new ForegroundCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("bg") == 0 || cmd_s.find("bg ") == 0) {
        return new BackgroundCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("quit") == 0 || cmd_s.find("quit ") == 0) {
        return new QuitCommand(cmd_line, this->jobs);
    } else if (cmd_s.compare("cp") == 0 || cmd_s.find("cp ") == 0) {
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

void SmallShell::addJob(pid_t pid, const string& str, bool is_stopped) {
    jobs->addJob(pid, str, is_stopped);
}
