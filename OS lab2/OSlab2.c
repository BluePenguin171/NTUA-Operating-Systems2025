#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>



#define OPEN_ERROR 1
#define MEMORY_ERROR 2
#define GEN_ERROR 3
#define ARG_ERROR 4

#define ALLOWED_ERRORS 50

//Global var
int N; 
pid_t *children;

void generate_children(pid_t children[],int N){
    int i = 0,errors = 0; 
    while(i < N){
        pid_t child = fork();
        if(child == 0){
            char *child_args[] = {"./child",NULL};
            execv(child_args[0],child_args);
            perror("Something Went Wrong! Killing the child...");
            exit(OPEN_ERROR);
        }
        else if (child < 0){
            ++errors; 
            perror("Lost a child!");
        }
        else{
            children[i++] = child;
        }

        if(errors > ALLOWED_ERRORS){
            perror("Lost too many childs!");
            free(children);
            exit(GEN_ERROR);
        }
    }
}

//user handler
void command_children(int sigtype){
    for(int i =0; i< N; ++i) {
        if(kill(children[i],sigtype) != 0) 
            perror("Missed an execution!");
    }
}

//child handler
void child_interrupt(int sigtype){
    int status;
    pid_t changed_child;
    time_t end,start;
    time(&start);
    char buffer[100];
    //find the one who sent the SIGCHLD
    while((changed_child =  waitpid(-1, &status, WUNTRACED | WNOHANG))==0){
        time(&end);
        if((double) (end - start) > 0.5){
            perror("False Alarm (SIGCHLD)");
            return;
        }
    }
    
    if(WIFEXITED(status) || WIFSIGNALED(status)){
        int len = snprintf(buffer, sizeof(buffer), "A child has been terminated, creating a new one.\n");
        write(STDOUT_FILENO,buffer,len);
        pid_t new_child[1];
        generate_children(new_child,1); 
        
        //search in children to replace the child 
        for(int i =0; i< N; ++i){
            if(children[i] == changed_child){
                children[i] = new_child[0];
                len = snprintf(buffer, sizeof(buffer), "new child %d : %d\n", i + 1 , children[i]);
                write(STDOUT_FILENO,buffer,len);
            }
        }
        
    }

    else if(WIFSTOPPED(status)){
        int len = snprintf(buffer, sizeof(buffer), "A child has stop, trying to motivate it...\n");
        write(STDOUT_FILENO, buffer, len);
        if(kill(changed_child,SIGCONT) != 0) 
            perror("Missed a SIGCONT signal");
    }

} 


//Term handler
void execution(int sigtype){
    int status;
    //ending sequence
    command_children(SIGTERM); 
    // waiting for them to end
    for (int i = 0; i < N; ++i) wait(&status);
    char msg[] = {"My work here is done!\n"};
    write(STDERR_FILENO,msg,23);
    free(children);
    exit(0);
}

int main(int argc, char *argv[])
{

    //Setting Up
    if (argc < 2)
    {
        perror("Not enough Arguments");
        exit(ARG_ERROR);
    }
    

    pid_t me;
    char *end;
    
    N = strtol(argv[1], &end, 10); 

    if (*end != '\0')
    {
        perror("Invalid Argument!");
        exit(ARG_ERROR);
    }


    children = (pid_t *)malloc(N*sizeof(pid_t));
    
    if(children == NULL){
        perror("Not enough Memory! Cant support so many children...");
        exit(MEMORY_ERROR);
    }

    me = getpid();
    
    //Phase 1 : Generate N Children
    generate_children(children,N);

    //Phase 2 : printing the tree
    printf("Father: %d\n",me);
    for(int i =0; i< N; ++i){
        printf("  child %d : %d\n",i+1,children[i]);
    }
    

    //User Handler
    struct sigaction UserHandler;
    UserHandler.sa_handler = command_children;
    UserHandler.sa_flags = 0;
    sigfillset(&UserHandler.sa_mask); 
    sigaction(SIGUSR1, &UserHandler, NULL);
    sigaction(SIGUSR2, &UserHandler, NULL);

    //Child Handler
    struct sigaction ChildHandler;
    ChildHandler.sa_handler = child_interrupt;
    ChildHandler.sa_flags = 0;
    sigemptyset(&ChildHandler.sa_mask); //fixes the not responsive child bug
    sigaction(SIGCHLD, &ChildHandler, NULL);

    //Termination Handler
    struct sigaction TermHandler;
    TermHandler.sa_handler = execution;
    TermHandler.sa_flags = 0;
    sigfillset(&TermHandler.sa_mask); 
    sigaction(SIGTERM, &TermHandler, NULL);
    sigaction(SIGINT,  &TermHandler, NULL);


    //Phase 3 : The main loop 
    while(1){
        pause();
    }



}