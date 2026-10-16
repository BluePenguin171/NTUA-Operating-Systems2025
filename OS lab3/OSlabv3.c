#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#define ARG_ERROR 1
#define MEMORY_ERROR 2
#define OPEN_ERROR 3
#define ALLOWED_ERRORS 4
#define GEN_ERROR 5 
#define PIPE_ERROR 6

#define READ_END 0
#define WRITE_END 1


void freePipes(int **pipes,int N){
    for (int j = 0; j < N; ++j) free(pipes[j]);
    free(pipes);
}

int **create_pipes(int N) {
    int **pipes = (int **)malloc(N * sizeof(int *));
    if (pipes == NULL) {
        perror("Memory allocation failed for pipe array");
        exit(MEMORY_ERROR);
    }

    for (int i = 0; i < N; ++i) {
        pipes[i] = (int *)malloc(2 * sizeof(int));
        if (pipes[i] == NULL) {
            perror("Not enough Memory for Pipes");
            freePipes(pipes, i);
            exit(MEMORY_ERROR);
        }
        if(pipe(pipes[i]) == -1){ 
            perror("Not enough Memory for Pipes");
            freePipes(pipes,i);
            exit(PIPE_ERROR);
        }


    }

    return pipes;
}

pid_t * generate_children(int N, int **uplink, int **downlink){
    pid_t *children;
    children = (pid_t *)malloc(N*sizeof(pid_t));
    
    if(children == NULL){
        perror("Not enough Memory! Cant support so many children...");
        exit(MEMORY_ERROR);
    }

    int i = 0,errors = 0; 
    while(i < N){
        pid_t child = fork();
        if(child == 0){
            dup2(uplink[i][READ_END],STDIN_FILENO); //Redirect ParentPipe Read End to standard input of the child
            dup2(downlink[i][WRITE_END],STDOUT_FILENO);//Redirect ChildPipe[WRITE_END] to standard output of the child

            // Child close all unused ends
            for(int k =0 ; k<N; ++k){
                close(uplink[k][READ_END]);
                close(uplink[k][WRITE_END]);
                close(downlink[k][READ_END]);
                close(downlink[k][WRITE_END]);
            }


            char *child_args[] = {"./child2",NULL};
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
    // Father close all unused ends
    for(int i =0; i<N; ++i){
        close(uplink[i][READ_END]);
        close(downlink[i][WRITE_END]);  
    }
    return children;
}

int find_max_end(int **A,int N){
    int m = 0;
    for(int i =0 ; i<N; ++i){
        m = (A[i][READ_END] > m) ? A[i][READ_END] : m;
    }
    return m;
}

void command_children(int sigtype,pid_t *children,int N){
    for(int i =0; i< N; ++i) {
        if(kill(children[i],sigtype) != 0) 
            perror("Missed an execution!");
    }
}

void execution(pid_t *children,int ** uplink,int **downlink, int N){
    int status;
    //ending sequence
    command_children(SIGKILL,children,N); 
    // waiting for them to end
    for (int i = 0; i < N; ++i) wait(&status);
    char msg[] = {"My work here is done!\n"};
    write(STDERR_FILENO,msg,23);
    free(children);
    freePipes(uplink,N);
    freePipes(downlink,N);
    exit(0);
}

int main(int argc, char *argv[])
{
    //Phase 1 : Checking the Arguments
    //Number of children check
    if (argc < 2)
    {
        perror("Not enough Arguments");
        exit(ARG_ERROR);
    }

    int N; 
    char *end;
    N = strtol(argv[1], &end, 10); 
    if (*end != '\0')
    {
        perror("Invalid Argument!"); //execution here
        exit(ARG_ERROR);
    }

    //MODE check
    //MODE : 1 for round-robin, 
    //MODE : 0 for random 
    short int MODE;
    int ticket = N;
    const char ROUND[] = "--round-robin";
    const char RANDOM[] = "--random";

    if (argc == 2){MODE = 1;} //Default case : Round-Robin
    else if (!strcmp(argv[2],ROUND)){ MODE =1;}
    else if (!strcmp(argv[2],RANDOM)){ MODE = 0;}
    else{
        printf("Usage: ask3 <nChildren> [--random] [--round-robin]");
        exit(0);
    }

    //Pipe setup
    
    int **uplink, **downlink; 
    uplink = create_pipes(N);
    downlink = create_pipes(N);

    srand(time(NULL));

    //Phase 2 : Generate N Children
    pid_t me,*children; 
    me = getpid();
    children = generate_children(N,uplink,downlink);

    
    
    
    
    //Phase 3 : Main Parent Work
    printf("Type your commands:\n");
    //Setting Up file Discriptors 
    fd_set my_fd;
    const int STDIN_FD = 0;
    const int MAX_FD = find_max_end(downlink,N);

    //Setting Up buffer, and commands 
    char buffer[100];
    char HELPCOMMAND[] ="help", EXITCOMMAND[] = "exit";

    int val;
    //main loop
    while(1){

        //INITIALIZE FILE DESCRIPTORS
        FD_ZERO(&my_fd);
        FD_SET(STDIN_FD, &my_fd);     
        for(int i =0; i<N; ++i) FD_SET(downlink[i][READ_END],&my_fd);


        int retval = select(MAX_FD + 1, &my_fd, NULL, NULL, NULL);
        if(retval == -1){
            perror("Select()");
            continue;
        }

        if(FD_ISSET(STDIN_FD, &my_fd)){

            //Read Buffer from Standard Input
            int size = read(STDIN_FD, buffer, sizeof(buffer)); 
            if(size>0){ //null terminate
                buffer[size-1] = '\0';  //clear the '\n' character
            }
            else{
                perror("Could Not Read!");
                continue;
            }

            
            if(!strcmp(buffer,HELPCOMMAND)){ //Is the command Help?
                printf("Type a number to send job to a child!\n");
                fflush(stdout);
            }
            else if(!strcmp(buffer,EXITCOMMAND)){ //Is the command Exit?
                execution(children,uplink,downlink, N);
            }
            else{
                //Is the command a number?
                char *end;
                val = strtol(buffer, &end, 10); 
                if (*end != '\0')
                {
                    printf("Type a number to send job to a child!\n");
                    continue;
                }
                
                //Decide the next worker 
                if(MODE){  
                    ticket = (ticket >= (N - 1)) ? 0 : ticket + 1;
                }
                else{ 
                    ticket = rand()%N;
                }
                
                write(uplink[ticket][WRITE_END],&val,sizeof(int));
                printf("[Parent : %d] Assigned %d to [Child : %d] [%d]\n",me,val,ticket+1, children[ticket]);
                
            }
        }
        else {
            for(int i = 0; i< N; ++i){   //Iterate to find who broke the select 
                if(FD_ISSET(downlink[i][READ_END],&my_fd)){
                    if (read(downlink[i][READ_END],&val,sizeof(int)) < 0){
                        perror("read()");
                    } 
                    else{
                        printf("[Child %d] [%d] Child Finished hard work, writing back %d\n", i + 1, children[i], val); 
                        fflush(stdout);
                    }
                    break;
                }
            }
            
        }
        

    }

    
}