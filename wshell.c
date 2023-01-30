#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_CMDS 100
#define MY_PATH_MAX 4096
#define HISTORY_SIZE 10

//TODO history only adds two commands, but has correct number of entries. problem with print loop?

typedef struct history {
    int nEntries;
    char *lines[HISTORY_SIZE];
} history;

char *getDir();

void prompt(char **);

void addToHistory(history *myHistory, char *line);

void printHistory(history *myHistory);

void tokenize(char *, char **, int *);

int handleCmds(char **tokenizedInput, char *, int, history *myHistory);

void fileCheck(char *);

void parseShellOperator(char **tokenizedInput, char *, int, history *myHistory);

void halfinator(char *leftDest[], char *rightDest[], char *src[], int nCmds, int indexOfOp);

int main() {
    history *myHistory = malloc(sizeof(history));
    while (1) {
        char *line = "";
        prompt(&line);

        char *arguments[MAX_CMDS];
        int nArgs = 0;
        tokenize(line, arguments, &nArgs);
        parseShellOperator(arguments, line, nArgs, myHistory);
//        handleCmds(arguments, line, nArgs, myHistory);
    }
}

//make this return exit code, switch error handling to parent parsing function.
int handleCmds(char **tokenizedInput, char *line, int nArgs, history *myHistory) {
    char *cmd = tokenizedInput[0];
    int returnNumber = 0;
    int switchNum = 0;
    int nCmds = 5;
    char *listOfCmds[nCmds];
    listOfCmds[0] = "exit";
    listOfCmds[1] = "cd";
    listOfCmds[2] = "pwd";
    listOfCmds[3] = "echo";
    listOfCmds[4] = "history";
    addToHistory(myHistory, line);

    for (int i = 0; i < nCmds; i++) {
        if (strcmp(cmd, listOfCmds[i]) == 0) {
            break;
        }
        switchNum++;
    }

    switch (switchNum) {
        case 0: //exit
            fileCheck(line);
            exit(0);
        case 1: //cd, will fail
            if (nArgs == 1) {
                if (chdir(getenv("HOME")) != 0) {
                    puts("wshell: cd: home: No such file or directory");
                    returnNumber = 1;
                }
                fileCheck(line);
                break;
            } else if (nArgs == 2) { //correct
                fileCheck(line);
                if (chdir(tokenizedInput[1]) != 0) {
                    printf("wshell: no such directory: %s\n", tokenizedInput[1]);
                    returnNumber = 1;
                }
            } else if (nArgs > 2) {
                fileCheck(line);
                puts("wshell: cd: too many arguments");
                returnNumber = 1;
                break;
            }
            break;
        case 2: { //pwd: will never fail*
            char cwd[MY_PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                fileCheck(line);
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
                //program exits, return number has no effect
//                returnNumber = 1;
            }
            break;
        }
        case 3: //echo: will never fail
            fileCheck(line);
            if (nArgs > 1) {
                for (int i = 1; i < nArgs - 1; i++) {
                    printf("%s ", tokenizedInput[i]);
                }
                printf("%s\n", tokenizedInput[nArgs - 1]);
            } else {
                puts("");
            }
            break;
        case 4: //history: will never fail
            fileCheck(line);
            printHistory(myHistory);
            break;
        default: //extern: will fail
            fileCheck(line);
            int pid = fork();
            int status;
            if (pid == 0) {
                int i;
                char *cmdArgs[MAX_CMDS];
                for (i = 0; i < nArgs; i++) {
                    cmdArgs[i] = tokenizedInput[i];
                }
                cmdArgs[nArgs] = NULL;
                if (execvp(cmd, cmdArgs) == -1) {
                    printf("wshell: could not execute command1: %s\n", cmd);
                    //if this is reached exit wont work...
                    returnNumber = 1;
                }
            } else if (pid > 0) {
                pid = wait(&status);
            } else {
                //-1, error, but dont quit
                printf("wshell: could not execute command2: %s\n", cmd);
            }
    }
    return returnNumber;
}

void fileCheck(char *line) {
    if ((!isatty(fileno(stdin)))) {
        printf("%s\n", line);
        fflush(stdout);
    }
}

void tokenize(char *line, char *tokens[], int *nArgs) {
    char *buffer, *aPtr;
    int count = 0;
    buffer = strdup(line);

    while ((aPtr = strsep(&buffer, " ")) && count < MAX_CMDS)
        tokens[count++] = aPtr;

    *nArgs = count;
    free(buffer);
    free(aPtr);
}

void prompt(char **lineBuf) {
    printf("%s$ ", getDir());
    char *line = NULL;
    size_t len = 0;
    ssize_t chars = getline(&line, &len, stdin);
    if (line[chars - 1] == '\n') {
        line[chars - 1] = '\0';
    }
    *lineBuf = line;
}

char *getDir() {
    char cwd[MY_PATH_MAX];
    char *baseName = "";
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        baseName = basename(cwd);
    } else {
        perror("getcwd() error");
    }
    return baseName;
}

void addToHistory(history *myHistory, char *line) {
    for (int i = HISTORY_SIZE - 2; i >= 0; i--) {
        myHistory->lines[i + 1] = myHistory->lines[i];
    }
    myHistory->lines[0] = line;
    if (myHistory->nEntries != 10) {
        myHistory->nEntries += 1;
    }
}

void printHistory(history *myHistory) {
    for (int i = myHistory->nEntries - 1; i >= 0; i--) {
        printf("%s\n", myHistory->lines[i]);
    }
}

//doesn't need to support >1 op. This function handles dispatching to handleCmds.
//char **tokenizedInput, char *, int, history *myHistory
void parseShellOperator(char *tokenizedInput[], char *line, int nCmds, history *myHistory) {
    //first pointer saved will be the op
    //doesnt check if silly person appends command to the end for || and &&.
    char *and = "&&";
    char *or = "||";
    char *background = "&";
    //will be null unless op is found
    int opType = -1;
    int opIndex = -1;
    for (int i = 0; i < nCmds; i++) {
        if ((strcmp(and, tokenizedInput[i]) == 0)) {
            opType = 0;
            opIndex = i;
            break;
        } else if (strcmp(or, tokenizedInput[i]) == 0) {
            opType = 1;
            opIndex = i;
            break;
        } else if (strcmp(background, tokenizedInput[i]) == 0) {
            opType = 2;
            opIndex = i;
            break;
        }
    }

    if (opType == 0) {
        char *leftDest[MAX_CMDS] = {NULL};
        char *rightDest[MAX_CMDS] = {NULL};

        halfinator(leftDest, rightDest, tokenizedInput, nCmds, opIndex);
        //and = only if first one succeeds, call second.
        if (handleCmds(leftDest, line, nCmds, myHistory) != 1) {
            handleCmds(rightDest, line, nCmds, myHistory);
        }
    } else if (opType == 1) {
        //or = only if first one fails, call second
        char *leftDest[MAX_CMDS] = {NULL};
        char *rightDest[MAX_CMDS] = {NULL};

        halfinator(leftDest, rightDest, tokenizedInput, nCmds, opIndex);

        if (handleCmds(leftDest, line, nCmds, myHistory) == 1) {
            //call wshell command not found only for second input. first one just gets ignored
            handleCmds(rightDest, line, nCmds, myHistory);
        }
    } else if (opType == 2) {
//        background
//        handleCmds(tokenizedInput, line, nCmds, myHistory)
    } else {
        handleCmds(tokenizedInput, line, nCmds, myHistory);
    }

}

void halfinator(char *leftDest[], char *rightDest[], char *src[], int nCmds, int indexOfOp) {
    int destArgs = nCmds - indexOfOp - 1;
    for (int i = 0; i < indexOfOp; i++) {
        leftDest[i] = src[i];
    }
    for (int i = indexOfOp + 1; i < nCmds; i++) {
        rightDest[i - destArgs - 1] = src[i];
    }
}
