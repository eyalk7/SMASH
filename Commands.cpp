#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

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
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
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

//---------------------------JOBS LISTS------------------------------

void JobsList::addJob(Command* cmd, bool is_stopped) {
    // call removeFinishedJobs
    // if empty, jobID = 1
    // insert into map with jobID = max jobid + 1
}
void JobsList::printJobsList() {
    // call removeFinishedJobs
    // iterate the map
    // print each job by the format
}
void JobsList::killAllJobs() {
    // interate on map, print message and send SIGKILL
}
void JobsList::removeFinishedJobs() {
    // iterate on map, and waitpid with each pid WNOHANG flag
    // check return waitpid, if job was "waited" then remove from map
}
JobEntry* JobsList::getJobById(JobID jobId) {
    // return from map
}
void JobsList::removeJobById(JobID jobId) {
    // remove from map
}
JobEntry* JobsList::getLastJob(JobID* lastJobId) {
    // return last in map
}
JobEntry* JobsList::getLastStoppedJob(JobID* jobId) {
    // iterate and find last stopped job return it
}

//-------------------------SPECIAL COMMANDS-------------------------
PipeCommand::PipeCommand(const char* cmd_line, SmallShell* shell) {
    // save cmd and shell
}
void PipeCommand::execute() {
    // create new pipe
    // fork two new childs
        // first child:
        // call setpgrp() syscall - make sure that the child get different GROUP ID
        // close read in pipe
        // put write side of pipe in stdout or stderr (if with |&)
        // shell.execute command with first half of the cmd

        // second child:
        // call setpgrp() syscall - make sure that the child get different GROUP ID
        // close write in pipe
        // put read side in stdin
        // shell.execute command with second half of the cmd

        //father:
        // close both sides of pipe
        // if & add both jobs to jobs list
       // else CURR_FORK =..., waitpid for both of them
        // CURR_ FORK.. =0
}

RedirectionCommand::RedirectionCommand(const char* cmd_line, SmallShell* shell) {
    // save cmd and shell
}
void RedirectionCommand::execute() {
    // fork
        // child:
        // stpgrp
        // close stdout
        // open in place 2 new file
        // if >> you need append, if > overwrite
        // shell.execute with the & if there is one

        // parent:
        // else CURR_FORK =..., waitpid
        // CURR_ FORK.. =0
}

//---------------------------EXTERNAL CLASSES------------------------------
ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) {
   // save jobs, and cmd
}

void ExternalCommand::execute() {
    // fork to child
    //child:
    // remove &
    // call setpgrp() syscall - make sure that the child get different GROUP ID
    // execv to shell with cmd without &

    //parent:
    // if with "&" add job to JOBS LIST and return
    // else, // else CURR_FORK =..., waitpid with UNTRACED
    //        // CURR_ FORK.. =0
    // if son still running (STOPPED and not zombie) add to jobs list
}

//---------------------------BUILT IN CLASSES------------------------------
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, SmallShell* shell) {
    // no argument = change to default prompt
    // get first argument
    // save prompt string
    // save shell pointer
}
void ChangePromptCommand::execute() {
    // shell change prompt with string
}

void ShowPidCommand::execute() {
    // "smash pid is:" ...
    // syscall getpid()
}

void GetCurrDirCommand::execute() {
    //
    // syscall getcwd()
}

ChangeDirCommand(const char* cmd_line, string* last_dir) {
    // save new_path and last_dir
    // more than one argument == print error "too many argument"
}
void ChangeDirCommand::execute() {
    // if new_path == "" return
    // get curr dir with syspath
    // if new_path == "-" syscall with last_dir
        // if no last_dir print error "..."
    // else, syscall with new_path
    // last_dir = curr dir

    // if syscall fails use perror to print error
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) {
    // save jobs list
}
void JobsCommand::execute() {
    // jobs.print...
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) {
    // parse: type of signal and jobID
        // if syntax not valid print error
    // if job id not exist print error message
    // get pid from jobs list
    // if sig_num not valid print error message

    // if something not valid save pid = 0
}
void KillCommand::execute() {
    // if pid = 0 return
    // send signal with syscall kill()
    // print message
    // if syscall fails use perror to print error
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) {
    // if num of argument not valid or syntax problem print error
    // save job_id, jobs list
    // if job_id not exist in job list print error message
    // if job id not valid print error
    // if no job_id, job_id = -1
}
void ForegroundCommand::execute() {
    // if job_id == -1 than get the last job
        // if jobs list empty print error
    // remove from jobs list
    // print job's command line
    // send SIGCONT to job's pid
    // CURR_FORK =..., waitpid
    // CURR_ FORK.. =0
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) {
    // if no job id, job id = -1
    // if job not stopped error
    // if job not exist error
    // syntax problem - error
}
void BackgroundCommand::execute() {
    // if job_id = -1 , get last stop job
        // if no stopped jobs, print error
    // print cmd line of job
    // change to NOT STOPPED in jobs list
    // send SIGCONT to job's pid
    // no waitpid
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) {
    // set kill_all if "kill" and jobs list

}
void QuitCommand::execute() {
    // if (kill_all) ...
    // exit(0);
}

CopyCommand::CopyCommand(const char* cmd_line) {
    // save cmd line;
}
void CopyCommand::execute() {
    // open the new file place and the prev file
    // fork
        // child
        // setpgrp
        // loop:
        // read from prev and write to new file in chuncks

        // father
        // if with & add to job list
        // else CURR_FORK =..., waitpid
        // CURR_ FORK.. =0
}
//---------------------------END OF BUILT IN--------------------------------


SmallShell::SmallShell() {
    // prompt = Smash
    // CURR_FORK.. = 0
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::CreateCommand(const char* cmd_line) {
	// For example:
    // > >> | |&
    // if one of those, create special class
/*
  string cmd_s = string(cmd_line);
  if (cmd_s.find("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if ...
  .....
  else {
    return new ExternalCommand(cmd_line, jobs);
  }
  */
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
  // Command* cmd = CreateCommand(cmd_line);
  // maybe nullptr
  // cmd->execute();
  // Please note that you must fork smash process for some commands (e.g., external commands....)
  // delete class
}