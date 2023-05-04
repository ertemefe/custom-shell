#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#define BUFFER_SIZE 128

int main(int argc, char **argv)
{
    printf("Welcome to the room  %s!! \n\n", argv[1]);
    char *filePath = malloc(sizeof(char) * (strlen(argv[1] + strlen(argv[2]) + BUFFER_SIZE*2)));

    strcat(filePath, "/tmp/");
    strcat(filePath, argv[1]);
    char *room = strdup(filePath);

    struct stat exist = {0};
    if (stat(room, &exist) == -1)
    {
        mkdir(room, 0700);
    }

    strcat(filePath, "/");
    strcat(filePath, argv[2]);
    char *userName = strdup(filePath);

    if (access(userName, F_OK) != 0)
    {
        mkfifo(userName, 0777);
    }

    char *inputMessage = (char *)malloc(BUFFER_SIZE*3 * sizeof(char));
    ssize_t len = BUFFER_SIZE*3;
    ssize_t numberOfLines = 0;
    pid_t pid, pid1, pid2;
    char *readInput;
    
    pid = fork();
    if (pid == 0)
    {
        while (1)
        {
            // Child that constantly reads from the pipe
            readInput = (char *)malloc(BUFFER_SIZE * sizeof(char));

            int fdRead;
            fdRead = open(userName, O_RDONLY);
            read(fdRead, readInput, BUFFER_SIZE);

            close(fdRead);
            printf("[%s] %s", argv[1], readInput);
            fflush(stdout);
            free(readInput);
        }
    }

    pid1 = fork();
    if (pid1 != 0)
    {
        waitpid(pid, NULL, 0);
    }
    else
    {
        while (1)
        {
            numberOfLines = getline(&inputMessage, &len, stdin);

            if (numberOfLines > 1)
            {

                DIR *directory;
                struct dirent *user;
                directory = opendir(room);
                if (directory)
                {
                    while ((user = readdir(directory)) != NULL)
                    {
                        pid2 = fork();
                        if (pid2 == 0)
                        {
                            // do not write to . and .. files which are hidden
                            if (strcmp(".", user->d_name) == 0 || strcmp("..", user->d_name) == 0)
                            {
                                return 0;
                            }

                            else if (strcmp(argv[2], user->d_name) == 0)
                            {
                                // Current user's pipe so no writing no action
                                return 0;
                            }
                            else
                            {
                                // Write if the file is others files
                                char *currentPath = malloc(sizeof(char) * BUFFER_SIZE);
                                strcat(currentPath, room);
                                strcat(currentPath, "/");
                                strcat(currentPath, user->d_name);


                                char *message = malloc(sizeof(char) * BUFFER_SIZE*3);
                                strcat(message, argv[2]);
                                strcat(message, ": ");
                                strcat(message, inputMessage);

                                int fdWrite;
                                fdWrite = open(currentPath, O_WRONLY);

                                write(fdWrite, message, BUFFER_SIZE);
                                close(fdWrite);

                                printf("[%s] %s: %s", argv[1], argv[2], inputMessage);
                                fflush(stdout);
                                free(currentPath);
                                return 0;
                            }
                        }
                    }
                }
                closedir(directory);
            }
            free(inputMessage);
            inputMessage = (char *)malloc(BUFFER_SIZE*3 * sizeof(char));
        }
    }
    return 0;
}