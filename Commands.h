#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)

class Command {
// TODO: Add your data members
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line);
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class JobsList;

/*
class CommandsHistory {
 protected:
  class CommandHistoryEntry {
	  // TODO: Add your data members
  };
 // TODO: Add your data members
 public:
  CommandsHistory();
  ~CommandsHistory() {}
  void addRecord(const char* cmd_line);
  void printHistory();
};

class HistoryCommand : public BuiltInCommand {
 // TODO: Add your data members
 public:
  HistoryCommand(const char* cmd_line, CommandsHistory* history);
  virtual ~HistoryCommand() {}
  void execute() override;
};
*/

class JobsList {
 public:
  class JobEntry {
   int pid;
   string cmd_str;
  };
 // TODO: Add your data members
 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, bool isStopped = false);
  void printJobsList();
  void killAllJobs();
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  // TODO: Add extra methods or modify exisitng ones as needed
};

//---------------------------BUILT IN CLASSES------------------------------
// maybe timeout ?
class ChangePromptCommand : public BuiltInCommand {
    string prompt;
    SmallShell* shell;
public:
    ChangePromptCommand(const char* cmd_line, SmallShell* shell);
    virtual ~ChangePromptCommand() {}
    void execute() override;
};


class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char* cmd_line) = default;
    virtual ~ShowPidCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    GetCurrDirCommand(const char* cmd_line);
    virtual ~GetCurrDirCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    string new_path;
    string* last_dir;
public:
    ChangeDirCommand(const char* cmd_line, string* last_dir);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    virtual ~JobsCommand() {}
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    int sig_num, pid;
public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    virtual ~KillCommand() {}
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~ForegroundCommand() {}
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
    int job_id;
    JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    virtual ~BackgroundCommand() {}
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
    bool kill_all;
    JobsList* jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs);
    virtual ~QuitCommand() {}
    void execute() override;
};

// TODO: should it really inhirit from BuiltInCommand ?
class CopyCommand : public BuiltInCommand {
public:
    CopyCommand(const char* cmd_line);
    virtual ~CopyCommand() {}
    void execute() override;
};
//---------------------------END OF BUILT IN--------------------------------

class SmallShell {
 private:
  // TODO: Add your data members
  SmallShell();
  JobsList jobs;
  string prompt;
  string last_dir;
 public:
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  void changePrompt(string prompt);
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_