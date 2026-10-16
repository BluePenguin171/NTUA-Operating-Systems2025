#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>


#define CHILLING 5 

int main(int argc, char *argv[]) {
    pid_t my_id = getpid();   // Getting PID of the process
    
    int val; 

    fd_set my_fd;

    while (1) {
        FD_ZERO(&my_fd);
        FD_SET(STDIN_FILENO , &my_fd);    //Standard input is the read end of the pipe


        int retval = select(STDIN_FILENO + 1, &my_fd, NULL, NULL, NULL);
        if(retval == -1){
            perror("Select()");
            continue;
        }
        if(FD_ISSET(STDIN_FILENO, &my_fd)){
            if(read(STDIN_FILENO, &val, sizeof(int)) < 0 ){
                perror("Could Not Read!");
                continue;
            }
            sleep(CHILLING);
            val = val*2; 
            write(STDOUT_FILENO, &val, sizeof(int));
        
        }
    }
}