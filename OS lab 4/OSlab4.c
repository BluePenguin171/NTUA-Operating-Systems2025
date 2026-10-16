#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>  
#include <netdb.h>      
#include <poll.h>
#include <unistd.h>
#include <time.h>

#define CONNECT_ERROR -1
#define ARGUMENT_ERROR 1 
#define SOCKET_ERROR 2
#define INVALID_IP_ERROR 3
#define POLL_ERROR 4
#define READ_ERROR 5
#define WRITE_ERROR 6
#define FORMAT_ERROR 7
#define MEMORY_ERROR 8

#define DEF_HOST "147.102.75.201"
#define DEF_PORT 41312

#define BUFFER_SIZE 100


void input_validation(int argc, char *argv[], char *host, int *port, short int *DEBUG_MODE){
    const char HOST_OPTION_LONG[] = "--host";
    const char HOST_OPTION_SHORT[] = "-h";

    const char PORT_OPTION_LONG[] = "--port";
    const char PORT_OPTION_SHORT[] = "-p";

    const char DEBUG_OPTION_LONG[] = "--debug";
    const char DEBUG_OPTION_SHORT[] = "-d";

    short int debug_flag = 1;
    short int host_flag = 1;
    short int port_flag = 1;

    short int OPTION_ERROR = 0;

    for(int i =1; i<argc; ++i){
        if((!strcmp(argv[i],DEBUG_OPTION_LONG) || !strcmp(argv[i],DEBUG_OPTION_SHORT)) && debug_flag){
            *DEBUG_MODE = 1;
            debug_flag = 0;
        }
        else if ((!strcmp(argv[i],HOST_OPTION_LONG) || !strcmp(argv[i],HOST_OPTION_SHORT)) && host_flag){
            if((i+1) >= argc ){
                OPTION_ERROR = 1;
                break;
            }
            strcpy(host,argv[++i]);
            host_flag = 0;
        }

        else if ((!strcmp(argv[i],PORT_OPTION_LONG) || !strcmp(argv[i],PORT_OPTION_SHORT)) && port_flag){
            if((i+1) >= argc ){
                OPTION_ERROR = 1;
                break;
            }
            //convert argv[i] to number            
            char **end;
            *port = strtol(argv[++i],end,10);
            if (**end != '\0' || *port <= 0 || *port > 65535) {
                fprintf(stderr, "Invalid port number\n");
                 exit(ARGUMENT_ERROR);
            }
            port_flag = 0;
        }

        else{
            OPTION_ERROR = 1;
            break;
        }
    }

    if(OPTION_ERROR){
        printf("Usage: ./ask4 [--host <HOST>] [--port <PORT>] [--debug]\n");
        exit(ARGUMENT_ERROR);
    }

}

void close_everything(struct pollfd *fds,char *buffer){
    close(fds[0].fd);
    close(fds[1].fd);
    free(buffer);
}

void terminate(struct pollfd *fds,char *buffer) {
    close_everything(fds,buffer);
    printf("bye\n");
    exit(0);
}

int read_fd(struct pollfd *fds,short int fd_id,char *buffer){
    int s = read(fds[fd_id].fd,buffer,BUFFER_SIZE - 1);
    if(s>0){ //null terminate
        buffer[s-1] = '\0';  //clear the '\n' character
    }
    else{
        perror("couldn't read");
        close_everything(fds,buffer);
        exit(READ_ERROR);
    }
    return s;
}

void send_command(char *buffer, int size,struct pollfd *fds, short int MODE){
    
    if (write(fds[1].fd, buffer, size) < 0){
        perror("write error");
        close_everything(fds, buffer);
        exit(WRITE_ERROR);
    } 
    if (MODE){
        printf("[DEBUG] sent %s\n",buffer);
    }
}

void help(){
    printf("---------------------------\n");
    printf("help : Prints help message\n");
    printf("get  : request server for data\n");
    printf("exit : terminates the program\n");
}

void verify_message(char *buffer, int size, struct pollfd *fds,short int MODE){
    memset(buffer, 0, size);
    fds[0].events = 0; //stop watching stdin
    for(int step =1 ; step <= 2; ++step){
        int ready = poll(fds,2,5000); //timeout = 5sec;

        if (ready < 0){
            perror("poll");
            close_everything(fds,buffer);
            exit(POLL_ERROR);
        }
        else if (ready ==0){ //timeout case
            printf("Something might be wrong with the server! Try again later.\n");
            fds[0].events = POLLIN;
            return;
        }

        if (!(fds[1].revents & POLLIN)) { 
            printf("Something might be wrong with the sanity check! Find a better programmer!\n");
            fds[0].events = POLLIN;
            return;
        }
        int s = read_fd(fds,1,buffer);
        if(MODE) printf("[DEBUG]read %s\n",buffer);
        
        if(step == 1){
            printf("Send verification code: '%s'\n",buffer);
            
            fds[0].events = POLLIN;
            fds[1].events = 0;
            char code[30];
            int ready = poll(fds,2,-1);
            if (ready < 0){
                perror("poll");
                close_everything(fds,buffer);
                exit(POLL_ERROR);
            }
            int code_size = read_fd(fds,0,code);
            
            fds[0].events = 0;
            fds[1].events = POLLIN;

            send_command(code,code_size,fds,MODE);
        }
        else{
            printf("Response: %s\n",buffer);
        }
        
    }
    fds[0].events = POLLIN;
    return;


}


void handle_command(char *buffer, int size, struct pollfd *fds,short int MODE){
    static const char GET_COMMAND[] = "get";
    static const char HELP_COMMAND[] ="help";
    static const char EXIT_COMMAND[] = "exit";

    if(!strcmp(buffer,GET_COMMAND)){
        send_command(buffer,size,fds, MODE);
    }
    else if (!strcmp(buffer,HELP_COMMAND)){
        help();
    }
    else if (!strcmp(buffer,EXIT_COMMAND)){
        terminate(fds,buffer);
    }
    else{
        int N;
        char name[15],surname[15],reason[70];

        if (sscanf(buffer, "%d %15s %15s %70[^\n]", &N, name, surname, reason) != 4) {
            printf("Unknown Command! Try typing \"help\"...\n");
            return;
        }
        
        send_command(buffer,size,fds,MODE);
        verify_message(buffer,size,fds, MODE);
        
    }


}
            



void convert_data(char *buffer, int size,short int MODE){
    //print in human readable form the data format: X YYY ZZZZ WWWWWWWWWW
    if(MODE) printf("[DEBUG]read %s\n",buffer);

    
    //TODO : better solution
    char interval = buffer[0];
    
    char Y[4] = {'\0'};
    memcpy(Y, buffer + 2, 3);
    int light_levels = atoi(Y); //convert to number
    
    char Z[5] = {'\0'};
    memcpy(Z, buffer + 6, 4);
    int int_temp = atoi(Z);
    float temp = int_temp / 100.0;


    // TODO FIX THIS
    char W[10] = {'\0'};
    memcpy(W, buffer + 11, 9);
    time_t timestamp = atoi(W);
    struct tm *server_time;

    server_time = localtime(&timestamp);
    


    printf("---------------------------\n");
    printf("Latest Event:\n");
    printf("interval (%c)\n",interval);
    printf("Temperature is: %.2f\n", temp);
    printf("Light Level is: %d\n", light_levels);
    printf("Timestamp is:%s\n", asctime(server_time));
    


}



int main(int argc, char *argv[]){

    //Phase 1 : Checking the Arguments
    short int DEBUG_MODE = 0;
    char host[256];
    strcpy(host, DEF_HOST);
    int port = DEF_PORT;

    input_validation(argc,argv,host,&port,&DEBUG_MODE);
    
    
    //Phase 2 : Connection with server
    
    int sock_fd; //socket creation 
    if ((sock_fd =socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket creation failed!");
        exit(SOCKET_ERROR);
    }


    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);  

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {  // convert IP string to binary
        perror("Invalid IP address");
        exit(INVALID_IP_ERROR);
    }

    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock_fd);
        exit(CONNECT_ERROR);
    }
    
    
    printf("Connection Established!\n");



    //Phase 3: Gossip time
    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (buffer == NULL){
        perror("Not enough memory");
        close(sock_fd);
        exit(MEMORY_ERROR);
    }

    struct pollfd myfds[2];
    myfds[0].fd = STDIN_FILENO;
    myfds[0].events = POLLIN;

    myfds[1].fd = sock_fd;
    myfds[1].events = POLLIN;

   

    while(1){
        int ready = poll(myfds,2,-1); //timeout = inf;

        if (ready < 0){
            perror("poll");
            close_everything(myfds,buffer);
            exit(POLL_ERROR);
        }

        if (myfds[0].revents & POLLIN) {  //read from STDIN_FILENO
            int s = read_fd(myfds,0,buffer);

            handle_command(buffer, s, myfds,DEBUG_MODE);
            memset(buffer, 0, s); //clear the buffer
        }
        else if (myfds[1].revents & POLLIN) {
            int s = read_fd(myfds,1,buffer);

            convert_data(buffer,s,DEBUG_MODE);  //long message must be X YYY ZZZZ WWWWWWWWWWW

            memset(buffer, 0, s);
        }

    }



}