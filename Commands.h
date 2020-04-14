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

// mecros
#define COMMAND_MAX_ARGS (20)
#define COMMAND_MAX_CHARS (80)
#define COPY_DATA_BUFFER_SIZE (1024);

#define STDIN 0
#define STDOUT 1
#define STDERR 2

// declaration of global variables
extern pid_t CURR_FORK_CHILD_RUNNING;
extern pid_t SMASH_PROCESS_PID;

class SmallShell;

//---------------------------JOBS LISTS------------------------------
struct JobEntry {
    explicit JobEntry(pid_t pid = 0, const string& cmd_str = "", bool is_stopped = false);
    pid_t pid;
    string cmd_str;
    bool is_stopped;
    time_t start_time;
};
typedef int JobID;

class JobsList {
public:
    JobsList() = default;
    ~JobsList() = default;
    void addJob(pid_t pid, const string& cmd_str, bool is_stopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry* getJobById(JobID jobId);
    void removeJobById(JobID jobId);
    JobEntry* getLastJob(JobID* lastJobId);
    JobEntry* getLastStoppedJob(JobID* jobId);

private:
  map<JobID,JobEntry> jobs;
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
};

class RedirectionCommand : public Command {
    SmallShell* shell;
    string cmd_part;
    string pathname;
    bool to_append;
    bool to_background;
public:
    RedirectionCommand(const char* cmd_line, SmallShell* shell);
    virtual ~RedirectionCommand() = default;
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
    string new_path;
    string* old_pwd;
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
    int signum, job_id;
    JobsList* jobs;
    bool parseAndCheck(const char* cmd_line, int* signum, JobID* job_id);
    void printArgumentsError();
    void printJobError();
    void printSignalSent(int signum, pid_t pid);

public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    virtual ~KillCommand() = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
    void printArgumentsError();
    void printJobError(JobID job_id);
    void printNoJobsError();
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ForegroundCommand() = default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
    void printArgumentsError();
    void printJobError(JobID job_id);
    void printNoJobsError();
    void printNotStoppedError(JobID job_id);
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~BackgroundCommand() = default;
    void execute() override;
};

// parsing function for background and foreground commands
bool parseAndCheckFgBgCommands(const char* cmd_line, JobID* job_id);

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
};

//---------------------------SMALL SHELL--------------------------------

class SmallShell {
 private:
  string prompt;
  string old_pwd;
  JobsList* jobs;
 public:
  SmallShell();
  Command* CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  ~SmallShell();
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  void executeCommand(const char* cmd_line);
  void changePrompt(const string& prompt);
  const string& getPrompt();
  void addJob(pid_t pid, const string& str, bool is_stopped = false);
  void updateJobs();
};

#endif //SMASH_COMMAND_H_