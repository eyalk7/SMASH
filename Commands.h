#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <limits>

#include <ctime>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>
#include <sstream>
#include <iomanip>

#include "Commands.h"

using std::vector;
using std::string;
using std::map;

// macros
#define COMMAND_MAX_ARGS (20)
#define COMMAND_MAX_CHARS (80)
#define COPY_DATA_BUFFER_SIZE (1024)

#define STDIN 0
#define STDOUT 1
#define STDERR 2

class JobsList;
class SmallShell;

// declaration of global variables
extern pid_t CURR_FORK_CHILD_RUNNING;   // PID of the process currently in the foreground
extern pid_t SMASH_PROCESS_PID;         // PID of the SMASH process
extern JobsList* GLOBAL_JOBS_POINTER;   // pointer to the Jobs list in SmallSHell
extern bool QUIT_SHELL;                 // While this is false the smash will keep running

// global variables for managing multiple alarms:
extern double TIME_UNTIL_NEXT_ALARM;    // leftover duration until next alarm signal is sent
extern time_t TIME_AT_LAST_UPDATE;      // used for updating TIME_UNTIL_NEXT_ALARM


//---------------------------JOBS LISTS------------------------------
struct JobEntry {
    pid_t pid;
    string cmd_str;
    bool is_stopped;        //  is the job stopped
    bool is_timeout;        // is this a timeout command
    unsigned int time_limit;  // relevant if this is a timeout command
    time_t start_time;
    time_t original_start_time; // relevant if this is a timeout command

    explicit JobEntry(pid_t pid = 0, const string& cmd_str = "",
                      bool is_stopped = false, bool is_timeout = false,
                      unsigned int time_limit = 0);
    void SetTime();
};
typedef int JobID;

class JobsList {
public:
    map<JobID,JobEntry> jobs;

    JobsList() = default;
    ~JobsList() = default;
    JobEntry* addJob(pid_t pid, const string& cmd_str, bool is_stopped = false,
                     bool is_timeout = false, unsigned int time_limit = 0);

    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry* getJobById(JobID jobId);
    void removeJobById(JobID jobId);
    JobEntry* getLastJob(JobID* lastJobId);
    JobEntry* getLastStoppedJob(JobID* jobId);
};

//-------------------------ABSTRACT COMMAND------------------------

class Command {
protected:
    string original_cmd;

public:
    explicit Command(const char* cmd_line) : original_cmd(cmd_line) {};
    virtual ~Command() = default;
    virtual void execute() = 0;
};


//-------------------------SPECIAL COMMANDS-------------------------

class PipeCommand : public Command {
    SmallShell* shell;
    bool has_ampersand, background;
    string command1, command2;

public:
    PipeCommand(const char* cmd_line, SmallShell* shell);
    virtual ~PipeCommand() = default;
    void execute() override;

private:
    /// A function that forks two children and sets their write/read
    /// to the ends of the given pipe accordingly.
    /// \param my_pipe - The pipe that both children have to use to communicate
    /// \param pid1 - Pointer to the first child's PID
    /// \param pid2 - Pointer to the second child's PID
    /// \return False if one of the fork()s failed, else True
    bool Pipe(int my_pipe[], pid_t* pid1, pid_t* pid2);
};

class RedirectionCommand : public Command {
    SmallShell* shell;
    bool to_append;     // true if ">>"
    bool to_background;
    bool cmd_is_built_in;     // built-in commands should not fork
    string cmd_part;
    string pathname;

public:
    RedirectionCommand(const char* cmd_line, SmallShell* shell);
    virtual ~RedirectionCommand() = default;
    void execute() override;
};

class TimeoutCommand : public Command {
    SmallShell* shell;
    bool to_background;
    unsigned int duration;
    string cmd_part;
    bool cmd_is_built_in; // built in command should not fork

public:
    TimeoutCommand(const char* cmd_line, SmallShell* shell);
    virtual ~TimeoutCommand() = default;
    void execute() override;
};

//---------------------------EXTERNAL CLASS------------------------------

class ExternalCommand : public Command {
    string cmd_to_son;
    JobsList* jobs;
    bool to_background;

public:
    ExternalCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ExternalCommand() = default;
    void execute() override;
};


//---------------------------BUILT IN CLASSES------------------------------

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char* cmd_line) : Command(cmd_line) {};
    virtual ~BuiltInCommand() = default;
};

class ChangePromptCommand : public BuiltInCommand {
    SmallShell* shell;
    string prompt;

public:
    ChangePromptCommand(const char* cmd_line, SmallShell* shell);
    virtual ~ChangePromptCommand() = default;
    void execute() override;
};


class ShowPidCommand : public BuiltInCommand {
public:
    explicit ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
    virtual ~ShowPidCommand() = default;
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    virtual ~GetCurrDirCommand() = default;
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    string* old_pwd;
    string new_path;

public:
    ChangeDirCommand(const char* cmd_line, string* last_dir);
    virtual ~ChangeDirCommand() = default;
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;

public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    virtual ~JobsCommand() = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
    int signum, job_id;
    JobEntry* job_entry;

    bool parseAndCheck(const char* cmd_line, int* signum, JobID* job_id);
    void printJobError();
    void printSignalSent();

public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    virtual ~KillCommand() = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
    JobEntry* job_entry;
    bool no_args;
    bool invalid_args;

public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ForegroundCommand() = default;
    void execute() override;

private:
    void printJobError();
};

class BackgroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
    JobEntry* job_entry;
    bool no_args;
    bool invalid_args;

public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~BackgroundCommand() = default;
    void execute() override;

private:
    void printJobError();
    void printNotStoppedError();
};

// parsing function for background and foreground commands
void parseAndCheckFgBgCommands(const char* cmd_line, JobID& job_id, bool& no_args, bool& invalid_args);

class QuitCommand : public BuiltInCommand {
    bool kill_all;
    JobsList* jobs;

public:
    QuitCommand(const char* cmd_line, JobsList* jobs);
    virtual ~QuitCommand() = default;
    void execute() override;
};

class CopyCommand : public BuiltInCommand {
    string old_path, new_path;
    bool background;
    JobsList* jobs;

public:
    explicit CopyCommand(const char* cmd_line, JobsList* jobs);
    virtual ~CopyCommand() = default;
    void execute() override;

    /// Checks if old_path and new_path reference the same file
    /// \return True if it's the same file, otherwise False
    bool comparePaths();

    bool openFiles(int* fd_read, int* fd_write);
    bool copyData(int fd_read, int fd_write, int SIZE);
};

//---------------------------SMALL SHELL--------------------------------

class SmallShell {
private:
    string prompt;
    string old_pwd;
    JobsList *jobs;

public:
    SmallShell();
    Command *CreateCommand(const char *cmd_line);
    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    ~SmallShell();
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    void executeCommand(const char *cmd_line);
    void changePrompt(const string &prompt);
    const string &getPrompt();
    JobEntry* addJob(pid_t pid, const string &str, bool is_stopped = false, bool is_timeout = false, unsigned int time_limit = 0);
    void updateJobs();
};

#endif //SMASH_COMMAND_H_