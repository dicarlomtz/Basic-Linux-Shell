#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <ctype.h>

#define MAXLENGTH 100
#define READ_END 0
#define WRITE_END 1

int flag = 1;
int fd[2];

int savedCommands = 0;
char *commandsLog[MAXLENGTH];

struct parsedCommand
{
    char **parsedArg;
    char **parsedArgPipe;
    bool background;
    bool piped;
    bool ownCmd;
};

char *readInput();

char **parseSpace(char *command);
char **parsePipe(char *command);
int parseBackground(char *command);
char **parseOwn(char *command);
struct parsedCommand *parseCommand(char *command);

void redirectOutput();
void redirectInput();
void closePipeReadWrite();

void saveCommand(char *command);
void historyOwnHandler();
void nthOwnHandler(int position);
void exitOwnHandler();

void useExec(char **command);
void executeOwn(char **command);
void doProcess();
void systemCall(char *command);
void loopSystemCall();

int main()
{
    loopSystemCall();
    return 0;
}

char *readInput()
{
    fflush(stdin);
    fflush(stdin);
    char *command = malloc(MAXLENGTH);
    printf("\nUser>");
    gets(command);
    return command;
}

char **parseSpace(char *command)
{
    char **parsedCommand = malloc(MAXLENGTH);
    char *token = strtok(command, " ");
    int i = 0;
    while (token != NULL && i < (MAXLENGTH - 1))
    {
        parsedCommand[i++] = token;
        token = strtok(NULL, " ");
    }
    parsedCommand[i] = NULL;
    return parsedCommand;
}

char **parsePipe(char *command)
{
    char **parsedCommand = malloc(2);
    char *token = strtok(command, "|");
    int i = 0;
    while (token != NULL && i < 2)
    {
        parsedCommand[i++] = token;
        token = strtok(NULL, "|");
    }
    if (i == 1)
    {
        parsedCommand[i] = NULL;
    }
    return parsedCommand;
}

int parseBackground(char *command)
{
    char backgroundSimbol[1] = "&";
    int len = strlen(command);
    if (command[len - 1] == backgroundSimbol[0])
    {
        return len - 1;
    }

    return -10;
}

char **parseOwn(char *command)
{
    char **parsedOwn = malloc(1);
    parsedOwn[0] = NULL;
    if (strcmp(command, "history") == 0)
    {
        parsedOwn[0] = "history";
    }
    else if (strcmp(command, "exit") == 0)
    {
        parsedOwn[0] = "exit";
    }
    else if (command[0] == '!' && strlen(command) >= 2)
    {
        int len = strlen(command);
        for (int i = 1; i < len; i++)
        {
            if (!isdigit(command[i]))
            {
                free(command);
                fprintf(stderr, "Solo se admiten números después de '!'\n");
                exit(-1);
            }
        }
        command++;
        char numbers[50];
        sprintf(numbers, "%d", atoi(command));
        parsedOwn[0] = numbers;
    }

    return parsedOwn;
}

struct parsedCommand *parseCommand(char *cmd)
{
    char command[MAXLENGTH];
    struct parsedCommand *pc = malloc(sizeof(struct parsedCommand));
    strcpy(command, cmd);

    int aux = 0;
    aux = parseBackground(command);
    if (aux != -10)
    {
        pc->background = true;
        command[aux] = '\0';
    }
    else
    {
        pc->background = false;
    }

    char **ownCmd = parseOwn(cmd);
    if (ownCmd[0] != NULL)
    {
        pc->ownCmd = true;
        pc->piped = false;
        memcpy(&pc->parsedArg, &ownCmd, sizeof(ownCmd));
        pc->parsedArg = ownCmd;
    }
    else
    {
        pc->ownCmd = false;
        free(ownCmd);
        char **pipedCommand = parsePipe(command);
        if (pipedCommand[1] == NULL)
        {
            char **arg = parseSpace(command);
            memcpy(&pc->parsedArg, &arg, sizeof(arg));
            pc->piped = false;
        }
        else
        {
            char **firstArg = parseSpace(pipedCommand[0]);
            char **secondArg = parseSpace(pipedCommand[1]);
            memcpy(&pc->parsedArg, &firstArg, sizeof(firstArg));
            memcpy(&pc->parsedArgPipe, &secondArg, sizeof(secondArg));
            pc->piped = true;
        }

        free(pipedCommand);
    }

    return pc;
}

void redirectOutput()
{
    dup2(fd[WRITE_END], STDOUT_FILENO);
}
void redirectInput()
{
    dup2(fd[READ_END], STDIN_FILENO);
}

void closePipeReadWrite()
{
    close(fd[READ_END]);
    close(fd[WRITE_END]);
}

void saveCommand(char *command)
{
    char *newCmd = malloc(strlen(command) + 2);
    bool saved = false;
    strcpy(newCmd, command);

    for (int i = 0; i < savedCommands; i++)
    {
        if (strcmp(command, commandsLog[i]) == 0)
        {
            saved = true;
            break;
        }
    }

    if (!saved)
    {
        if (savedCommands < MAXLENGTH)
        {
            commandsLog[savedCommands++] = newCmd;
        }
        else
        {
            free(commandsLog[0]);
            commandsLog[0] = commandsLog[MAXLENGTH - 1];
            commandsLog[MAXLENGTH - 1] = newCmd;
        }
    }
    else
    {
        free(newCmd);
    }
    free(command);
}

void historyOwnHandler()
{
    printf("\n");
    for (int i = 0; i < savedCommands; i++)
    {
        printf("%d. %s\n", i + 1, commandsLog[i]);
    }
}

void nthOwnHandler(int position)
{
    if (position > savedCommands)
    {
        fprintf(stderr, "No hay un comando en la posición: %d\n", position);
        exit(-1);
    }
    else
    {
        systemCall(commandsLog[position - 1]);
    }
}

void exitOwnHandler()
{
    for (int i = 0; i < savedCommands; i++)
    {
        free(commandsLog[i]);
    }
    flag = 0;
}

void useExec(char **command)
{
    if (execvp(command[0], command) < 0)
    {
        free(command);
        fprintf(stderr, "Error al llamar a execvp();\n");
        exit(-1);
    }
    free(command);
}

void executeOwn(char **command)
{
    if (strcmp(command[0], "history") == 0)
    {
        historyOwnHandler(commandsLog);
    }
    else if (strcmp(command[0], "exit") == 0)
    {
        exitOwnHandler();
    }
    else
    {
        nthOwnHandler(atoi(command[0]));
    }
    free(command);
}

void doProcess()
{
    char *command = readInput();
    if (strlen(command) > 0)
    {
        systemCall(command);
        saveCommand(command);
    }
    else
    {
        fprintf(stderr, "Debe introducir un comando\n");
    }
}

void systemCall(char *command)
{
    pid_t pid;
    struct parsedCommand *parsedCMD = parseCommand(command);

    if (!parsedCMD->ownCmd)
    {

        if (parsedCMD->piped)
        {
            if (pipe(fd) < 0)
            {
                fprintf(stderr, "Error al llamar a pipe();\n");
                exit(-1);
            }
        }

        pid = fork();

        if (pid < 0)
        {
            fprintf(stderr, "Error al llamar a fork();\n");
            exit(-1);
        }
        else if (pid == 0)
        {
            if (parsedCMD->piped)
            {
                redirectOutput();
                closePipeReadWrite();
            }
            useExec(parsedCMD->parsedArg);
        }
        else
        {
            if (parsedCMD->piped)
            {
                pid = fork();

                if (pid < 0)
                {
                    fprintf(stderr, "Error al llamar a fork();\n");
                    exit(-1);
                }
                else if (pid == 0)
                {
                    redirectInput();
                    closePipeReadWrite();
                    useExec(parsedCMD->parsedArgPipe);
                }
                else
                {
                    closePipeReadWrite();
                    if (!parsedCMD->background)
                    {
                        int status;
                        waitpid(pid, &status, 0);
                    }
                }
            }
            else
            {
                if (!parsedCMD->background)
                {
                    wait(NULL);
                }
            }
        }
    }
    else
    {
        executeOwn(parsedCMD->parsedArg);
    }
}

void loopSystemCall()
{
    do
    {
        doProcess();
    } while (flag == 1);
}
