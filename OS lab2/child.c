#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define ALARM_TIMER 10

// Global variables
time_t cur_time, birth_time;
volatile sig_atomic_t my_variable = 0;
int my_id;

void alarm_handler(int sigtype) {
    time(&cur_time);
    char buffer[100];
    int len = snprintf(buffer, sizeof(buffer), "PID: %d\n\trunning time : %.2f\n\tMy value: %d\n", 
                        my_id, (double)(cur_time - birth_time), my_variable);
    write(STDOUT_FILENO, buffer, len);
    alarm(ALARM_TIMER);
}

void user_handler(int sigtype) {
    if(sigtype == SIGUSR1){
        ++my_variable;
    }
    else if(sigtype == SIGUSR2){
        --my_variable;
    }
}

void terminate_handler(int sigtype){
    char buffer[50];
    int len = snprintf(buffer,sizeof(buffer), "Thread %d has been terminated!\n", my_id);
    write(STDOUT_FILENO, buffer, len);
    exit(0);
}

int main(int argc, char *argv[]) {
    alarm(ALARM_TIMER); // First alarm
    time(&birth_time);  // Getting the first time
    my_id = getpid();   // Getting PID of the process

    
    //Alarm Handler
    struct sigaction AlarmHandler;
    sigset_t block;
    sigfillset(&block);
    AlarmHandler.sa_mask = block;
    AlarmHandler.sa_handler = alarm_handler;
    AlarmHandler.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &AlarmHandler, NULL);
    
    
    //USER Handler
    struct sigaction UserHandler;
    UserHandler.sa_handler = user_handler;
    UserHandler.sa_flags = SA_RESTART;
    sigemptyset(&UserHandler.sa_mask);
    sigaction(SIGUSR1, &UserHandler, NULL);
    sigaction(SIGUSR2, &UserHandler, NULL);
    

    //Termination Handler
    struct sigaction TermHandler;
    TermHandler.sa_handler = terminate_handler;
    TermHandler.sa_flags = SA_RESTART;
    sigemptyset(&TermHandler.sa_mask);
    sigaction(SIGTERM, &TermHandler, NULL);

    while (1) {
        pause();
    }
}