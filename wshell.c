#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_CMDS 100
#define MY_PATH_MAX 4096
#define HISTORY_SIZE 10
#define AND "&&"
#define OR "||"
#define BACKGROUND "&"

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

void parseShellOperator(char **tokenizedInput, char *, int, history *myHistory, int pidArray[], char *bgCmdArr[]);

void halfinator(char *leftDest[], char *rightDest[], char *src[], int nCmds, int indexOfOp, int *destArgs);

void background(int pidArray[], char *line, char *cmds[], int nCmds, history *myHistory, char *bgCmdArr[]);

int currPidCount = 0;

int main() {
    history *myHistory = malloc(sizeof(history));
    int pidArray[255] = {0};
    char *bgCmdArr[255] = {NULL};
    while (1) {
        char *line = "";
        prompt(&line);

        char *arguments[MAX_CMDS];
        int nArgs = 0;
        tokenize(line, arguments, &nArgs);
        parseShellOperator(arguments, line, nArgs, myHistory, pidArray, bgCmdArr);
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
        case 0:
            exit(0);
        case 1: //cd, will fail
            if (nArgs == 1) {
                if (chdir(getenv("HOME")) != 0) {
                    puts("wshell: cd: home: No such file or directory");
                    returnNumber = 1;
                }
                break;
            } else if (nArgs == 2) { //correct
                if (chdir(tokenizedInput[1]) != 0) {
                    printf("wshell: no such directory: %s\n", tokenizedInput[1]);
                    returnNumber = 1;
                }
            } else if (nArgs > 2) {
                puts("wshell: cd: too many arguments");
                returnNumber = 1;
                break;
            }
            break;
        case 2: { //pwd: will never fail*
            char cwd[MY_PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            } else {
                perror("getcwd() error");
                //program exits, return number has no effect
            }
            break;
        }
        case 3: //echo: will never fail
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
            printHistory(myHistory);
            break;
        case 5:
            //kill
//            printf("wshell: no such background job: %d", job);
        case 6:
            //job
        default: { //extern: will fail
            int pid = fork();
            if (pid == 0) {
                int i;
                char *cmdArgs[MAX_CMDS];
                for (i = 0; i < nArgs; i++) {
                    cmdArgs[i] = tokenizedInput[i];
                }
                cmdArgs[nArgs] = NULL;
                if (execvp(cmd, cmdArgs) == -1) {
                    printf("wshell: could not execute command: %s\n", cmd);
                    exit(1);
                }
            } else {
                int status;
                waitpid(pid, &status, 0);
//                printf("pid: %d\n", pid);
                returnNumber = WEXITSTATUS(status);// think its this one
            }
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
void parseShellOperator(char *tokenizedInput[], char *line, int nCmds, history *myHistory, int pidArray[],
                        char *bgCmdArr[]) {
    fileCheck(line);
    //first pointer saved will be the op
    //doesnt check if silly person appends command to the end for || and &&.
    //will be null unless op is found
    int opType = -1;
    int opIndex = -1;
    for (int i = 0; i < nCmds; i++) {
        if ((strcmp(AND, tokenizedInput[i]) == 0)) {
            opType = 0;
            opIndex = i;
            break;
        } else if (strcmp(OR, tokenizedInput[i]) == 0) {
            opType = 1;
            opIndex = i;
            break;
        } else if (strcmp(BACKGROUND, tokenizedInput[i]) == 0) {
            opType = 2;
            opIndex = i;
            break;
        }
    }

    if (opType == 0) {
        //and = only if first one succeeds, call second.
        char *leftDest[MAX_CMDS] = {NULL};
        char *rightDest[MAX_CMDS] = {NULL};
        int rightArgs = 0;
        halfinator(leftDest, rightDest, tokenizedInput, nCmds, opIndex, &rightArgs);
        int leftArgs = nCmds - rightArgs - 1;
        if (handleCmds(leftDest, line, leftArgs, myHistory) != 1) {
            //so it doesnt double
            history *temp = malloc(sizeof(history));
            handleCmds(rightDest, line, rightArgs, temp);
        }
    } else if (opType == 1) {
        //or = only if first one fails, call second
        char *leftDest[MAX_CMDS] = {NULL};
        char *rightDest[MAX_CMDS] = {NULL};
        int rightArgs = 0;

        halfinator(leftDest, rightDest, tokenizedInput, nCmds, opIndex, &rightArgs);
        int leftArgs = nCmds - rightArgs - 1;
        if (handleCmds(leftDest, line, leftArgs, myHistory) == 1) {
            history *temp = malloc(sizeof(history));
            handleCmds(rightDest, line, rightArgs, temp);
        }
    } else if (opType == 2) {
        //pidArray holds all running and have run pids. (index + 1) is job number. backCmds is the string asscociated with
        //the corresponding pidArray entry.
        char *backgroundCmds[nCmds];
        unsigned long backgroundLineLen = strlen(line) - 2; //space and &
        char *backgroundLine = strdup(line);
        backgroundLine[backgroundLineLen] = '\0';
        for (int i = 0; i < nCmds - 1; i++) {
            backgroundCmds[i] = tokenizedInput[i];
        }
//        printf("line: %s\n", backgroundLine);
//        for(int i = 0; i < nCmds-1; i++){
//            printf("%s ",backgroundLine[i]);
//        }
//        puts("");

        background(pidArray, backgroundLine, backgroundCmds, nCmds - 1, myHistory, bgCmdArr);
    } else {
        handleCmds(tokenizedInput, line, nCmds, myHistory);
    }
}

void halfinator(char *leftDest[], char *rightDest[], char *src[], int totalArgs, int indexOfOp, int *destArgs) {
    *destArgs = totalArgs - indexOfOp - 1;
    int destCount = 0;
    for (int i = 0; i < indexOfOp; i++) {
        leftDest[i] = src[i];
    }
    for (int i = indexOfOp + 1; i < totalArgs; i++) {
        rightDest[destCount++] = src[i];
    }
}

void background(int pidArray[], char *line, char *cmds[], int nCmds, history *myHistory, char *bgCmdArr[]) {
    //if you want this to work with && and || have a function call before parsecmds that sets a boolean
    //and strips the and off, and then calls parse args.
    int childPid = fork();
    printf("[%d]\n", currPidCount +1);
    if (childPid == 0) {
        handleCmds(cmds, line, nCmds, myHistory);
        printf("[%d] Done: %s\n", currPidCount);
        //remove from job list
    } else if (childPid != -1) {
        pidArray[currPidCount] = childPid + 1;
        bgCmdArr[currPidCount] = line;
        currPidCount = (currPidCount + 1) % 255;
    } else {
        perror("fork background"); /* fork failed */
    }
}

