#ifndef SMASH__SIGNALS_H_
#define SMASH__SIGNALS_H_

#include "Commands.h"

void wakeUpCall(int pid, const string& str);
void ctrlZHandler(int sig_num);
void ctrlCHandler(int sig_num);
void alarmHandler(int sig_num);

#endif //SMASH__SIGNALS_H_
