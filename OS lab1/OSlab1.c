#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

#define OPEN_ERROR 1
#define MEMORY_ERROR 2
#define RW_ERROR 3
#define FOLDER_ERROR 4
#define ARG_ERROR 5

void ChildStaff(const char *PATH, int id, int parent_id)
{
    printf("Child process created: PID = %d\n", id);

    int filesize = strlen(PATH) + 21; // 11 = #output_.txt, 11 + 8 + 1(/) + 1(\0)
    char *file = (char *)malloc(filesize);
    char textsize = strlen("Child PID: ") + strlen(", Parent PID: ") + 17; // 8 + 8 + 1
    char *text = (char *)malloc(textsize + 1);

    if (file == NULL)
    {
        perror("Not enough memory for file!");
        exit(MEMORY_ERROR);
    }
    if (text == NULL)
    {
        perror("Not enough memory for text!");
        free(file);
        exit(MEMORY_ERROR);
    }

    snprintf(file, filesize, "%s/output_%d.txt", PATH, id);
    snprintf(text, textsize, "Child PID: %d, Parent PID: %d", id, parent_id);

    int fd = open(file, O_CREAT | O_WRONLY | O_TRUNC, 0644); // O_APPEND
    if (fd == -1)
    {
        perror("Could not open the file! Child is terminating...");
        exit(OPEN_ERROR);
    }

    if (write(fd, text, strlen(text)) < strlen(text))
    {
        perror("Could not write in the file!");
        exit(RW_ERROR);
    }

    close(fd);
    free(file);
    free(text);
}

void ReadFiles(const char *PATH)
{
    printf("\n\nParent reading file contents:\n");
    DIR *dir = opendir(PATH);
    if (dir == NULL)
    {
        perror("Folder Not Found!");
        exit(FOLDER_ERROR);
    }

    struct dirent *entry;
    char readBuffer[43];
    int filesize = strlen(PATH) + 20;
    char *file = (char *)malloc(filesize);

    if (file == NULL)
    {
        perror("Not enough memory for file!");
        exit(MEMORY_ERROR);
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if(entry->d_name[0] == '.') continue;
        snprintf(file, filesize, "%s/%s", PATH, entry->d_name);

        int fd = open(file, O_RDONLY);
        if (fd == -1)
        {
            perror("Could not open a file!");
            continue;
        }

        ssize_t bytesRead = read(fd, readBuffer, 42);
        if (bytesRead < 0)
        {
            perror("Could not read in the file!");
            printf("%s\n", file);
            close(fd);
            continue;
        }
        readBuffer[bytesRead] = '\0';
        printf("%s\n", readBuffer);
        close(fd);
    }
    closedir(dir);  
    free(file);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        perror("Not enough Arguments");
        exit(ARG_ERROR);
    }
    int status, N;
    pid_t me;
    char *end;
    N = strtol(argv[1], &end, 10);

    if (*end != '\0')
    {
        perror("Invalid Argument!");
        exit(ARG_ERROR);
    }

    me = getpid();
    
    //Creating a Parent Folder
    char PATH[15];
    snprintf(PATH,15,"./%d",me);
    if(mkdir(PATH, 0777) == -1){
        perror("Folder Couldn't be Created!");
        exit(FOLDER_ERROR);
    }

    // Starting Child Processes
    pid_t child;
    for (int i = 0; i < N; ++i)
    {
        child = fork();
        if (child < 0)
        {
            perror("Ooops! I missed a child!");
        }
        else if (child == 0)
        {
            ChildStaff(PATH, getpid(), getppid());
            exit(0);
        }
    }

    // waiting for them to end
    for (int i = 0; i < N; ++i)
        wait(&status);

    // reading the files
    ReadFiles(PATH);

    return 0;
}