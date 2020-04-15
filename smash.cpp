#include "Commands.h"
#include "signals.h"

pid_t SMASH_PROCESS_PID = 0;

int main(int argc, char* argv[]) {
    SMASH_PROCESS_PID = getpid();

    if(signal(SIGTSTP , ctrlZHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler)==SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    if(sigaction(SIGALRM , &act ,nullptr) < 0) {
        perror("smash error: sigaction failed");
    }

    SmallShell& smash = SmallShell::getInstance();
    while(true) {
        std::cout << smash.getPrompt() + ">";
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}