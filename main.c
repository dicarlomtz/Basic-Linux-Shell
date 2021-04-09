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
    bool own;
};

char *readInput();

int parseBackground(char *cmd);
char **parseOwn(char *cmd);
char **parseSpace(char *cmd);
char **parsePipe(char *cmd);
struct parsedCommand *parseCommand(char *cmd);

void saveCmd(char *cmd);
void historyHandler();
void nthHandler(int position);
void exitHandler();

void redirectOutput();
void redirectInput();
void closePipeReadWrite();

void useExec(char **cmd);
void executeOwn(char **cmd);
void systemCall(char *cmd);

void doProcess();
void loopProcess();

int main()
{
    loopProcess();
    return 0;
}

char *readInput()
{
    char *cmd = malloc(MAXLENGTH);

    printf("\nUser>");
    gets(cmd);

    return cmd;
}

int parseBackground(char *cmd)
{
    char backgroundSymbol = '&';
    int len = strlen(cmd);

    if (cmd[len - 1] == backgroundSymbol)
    {
        return len - 1;
    }

    return -1;
}

char **parseOwn(char *cmd)
{
    char **parsed = malloc(1);
    parsed[0] = NULL;

    if (strcmp(cmd, "history") == 0)
    {
        parsed[0] = "history";
    }
    else if (strcmp(cmd, "exit") == 0)
    {
        parsed[0] = "exit";
    }
    else if (cmd[0] == '!')
    {
        if (strlen(cmd) >= 2)
        {
            int len = strlen(cmd);
            for (int i = 1; i < len; i++)
            {
                if (!isdigit(cmd[i]))
                {
                    free(cmd);
                    free(parsed);
                    fprintf(stderr, "Solo se admiten números después de '!'\n");
                    exit(-1);
                }
            }
            cmd++;
            char numbers[50];
            sprintf(numbers, "%d", atoi(cmd));
            parsed[0] = numbers;
        }
        else
        {
            free(cmd);
            free(parsed);
            fprintf(stderr, "Debe ingresar la posición de un comando en history después de '!'\n");
            exit(-1);
        }
    }

    return parsed;
}

char **parseSpace(char *cmd)
{
    char **parsed = malloc(MAXLENGTH);
    char *token = strtok(cmd, " ");
    int i = 0;

    while (token != NULL && i < (MAXLENGTH - 1))
    {
        parsed[i++] = token;
        token = strtok(NULL, " ");
    }

    parsed[i] = NULL;
    return parsed;
}

char **parsePipe(char *cmd)
{
    char **parsed = malloc(2);
    char *token = strtok(cmd, "|");
    int i = 0;

    while (token != NULL && i < 2)
    {
        parsed[i++] = token;
        token = strtok(NULL, "|");
    }

    if (i == 1)
    {
        parsed[i] = NULL;
    }

    return parsed;
}

struct parsedCommand *parseCommand(char *cmd)
{

    if (strlen(cmd) <= 0)
    {
        free(cmd);
        fprintf(stderr, "Debe ingresar un comando válido");
        exit(-1);
    }

    char newCmd[MAXLENGTH];
    struct parsedCommand *pc = malloc(sizeof(struct parsedCommand));
    strcpy(newCmd, cmd);

    int backgroundSymbolPosition = parseBackground(newCmd);
    if (backgroundSymbolPosition != -1)
    {
        pc->background = true;
        newCmd[backgroundSymbolPosition] = '\0';
    }
    else
    {
        pc->background = false;
    }

    char **ownCmd = parseOwn(cmd);
    if (ownCmd[0] != NULL)
    {
        pc->own = true;
        pc->piped = false;
        memcpy(&pc->parsedArg, &ownCmd, sizeof(ownCmd));
        pc->parsedArg = ownCmd;
    }
    else
    {
        free(ownCmd);
        pc->own = false;
        char **pipedCmd = parsePipe(newCmd);
        if (pipedCmd[1] == NULL)
        {
            char **arg = parseSpace(newCmd);
            memcpy(&pc->parsedArg, &arg, sizeof(arg));
            pc->piped = false;
        }
        else
        {
            char **firstArg = parseSpace(pipedCmd[0]);
            char **secondArg = parseSpace(pipedCmd[1]);
            memcpy(&pc->parsedArg, &firstArg, sizeof(firstArg));
            memcpy(&pc->parsedArgPipe, &secondArg, sizeof(secondArg));
            pc->piped = true;
        }

        free(pipedCmd);
    }

    return pc;
}

void saveCmd(char *cmd)
{
    char *newCmd = malloc(strlen(cmd) + 2);
    bool saved = false;
    strcpy(newCmd, cmd);

    for (int i = 0; i < savedCommands; i++)
    {
        if (strcmp(cmd, commandsLog[i]) == 0)
        {
            saved = true;
            break;
        }
    }

    if (!saved)
    {
        if (newCmd[0] != '!')
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
    }
    else
    {
        free(newCmd);
    }

    free(cmd);
}

void historyHandler()
{
    for (int i = 0; i < savedCommands; i++)
    {
        printf("%d. %s\n", i + 1, commandsLog[i]);
    }
}

void nthHandler(int position)
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

void exitHandler()
{
    for (int i = 0; i < savedCommands; i++)
    {
        free(commandsLog[i]);
    }
    flag = 0;
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

void useExec(char **cmd)
{
    if (execvp(cmd[0], cmd) < 0)
    {
        free(cmd);
        fprintf(stderr, "Error al llamar a execvp();\n");
        exit(-1);
    }
    free(cmd);
}

void executeOwn(char **cmd)
{
    if (strcmp(cmd[0], "history") == 0)
    {
        historyHandler(cmd);
    }
    else if (strcmp(cmd[0], "exit") == 0)
    {
        exitHandler();
    }
    else
    {
        nthHandler(atoi(cmd[0]));
    }
    free(cmd);
}

void systemCall(char *cmd)
{
    pid_t f_pid, s_pid;
    struct parsedCommand *parsedCmd = parseCommand(cmd);

    if (!parsedCmd->own)
    {

        if (parsedCmd->piped)
        {
            if (pipe(fd) < 0)
            {
                fprintf(stderr, "Error al llamar a pipe();\n");
                exit(-1);
            }
        }

        f_pid = fork();

        if (f_pid < 0)
        {
            fprintf(stderr, "Error al llamar a fork();\n");
            exit(-1);
        }
        else if (f_pid == 0)
        {
            if (parsedCmd->piped)
            {
                redirectOutput();
                closePipeReadWrite();
            }
            useExec(parsedCmd->parsedArg);
        }
        else
        {
            if (parsedCmd->piped)
            {
                s_pid = fork();

                if (s_pid < 0)
                {
                    fprintf(stderr, "Error al llamar a fork();\n");
                    exit(-1);
                }
                else if (s_pid == 0)
                {
                    redirectInput();
                    closePipeReadWrite();
                    useExec(parsedCmd->parsedArgPipe);
                }
                else
                {
                    closePipeReadWrite();
                    if (!parsedCmd->background)
                    {
                        waitpid(f_pid, NULL, 0);
                        waitpid(s_pid, NULL, 0);
                    }
                }
            }
            else
            {
                if (!parsedCmd->background)
                {
                    waitpid(f_pid, NULL, 0);
                }
            }
        }
    }
    else
    {
        executeOwn(parsedCmd->parsedArg);
    }
}

void doProcess()
{
    char *cmd = readInput();
    printf("\n");
    systemCall(cmd);
    saveCmd(cmd);
}

void loopProcess()
{
    do
    {
        doProcess();
    } while (flag == 1);
}
