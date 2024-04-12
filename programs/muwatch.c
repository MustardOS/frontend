#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "options.h"

typedef struct {
    char name[64];
    int running;
} ProcessInfo;

int processExists(const char *processName) {
    DIR *dir = opendir("/proc");
    if (dir == NULL) {
        perror("opendir");
        return 0;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                char cmdlinePath[64];
                snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);
                FILE *cmdlineFile = fopen(cmdlinePath, "r");
                if (cmdlineFile != NULL) {
                    char cmdline[64];
                    if (fgets(cmdline, sizeof(cmdline), cmdlineFile) != NULL) {
                        if (strcmp(cmdline, processName) == 0) {
                            fclose(cmdlineFile);
                            closedir(dir);
                            return 1;
                        }
                    }
                    fclose(cmdlineFile);
                }
            }
        }
    }

    closedir(dir);
    return 0;
}

void startProcess(const char *processName) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    } else if (pid == 0) {
        if (setsid() < 0) {
            perror("setsid");
            exit(1);
        }
        if (execlp(processName, processName, NULL) < 0) {
            perror("execlp");
            exit(1);
        }
    }
}

void addProcess(ProcessInfo **processes, int *numProcesses, const char *processName) {
    *processes = realloc(*processes, (*numProcesses + 1) * sizeof(ProcessInfo));
    strcpy((*processes)[*numProcesses].name, processName);
    (*processes)[*numProcesses].running = 0;
    (*numProcesses)++;
}

int main() {
    ProcessInfo *processes = NULL;
    int numProcesses = 0;

    const char *processNames[] = {MUAUDIO_EXEC, MUBRIGHT_EXEC};
    int numProcessNames = sizeof(processNames) / sizeof(processNames[0]);

    for (int i = 0; i < numProcessNames; i++) {
        addProcess(&processes, &numProcesses, processNames[i]);
    }

    while (1) {
        for (int i = 0; i < numProcesses; i++) {
            ProcessInfo *currentProcess = &processes[i];
            if (!currentProcess->running) {
//                printf("Process %s is not running. Starting it...\n", currentProcess->name);
                startProcess(currentProcess->name);
                currentProcess->running = 1;
            }
            else if (!processExists(currentProcess->name)) {
//                printf("Process %s is not running. Restarting it...\n", currentProcess->name);
                startProcess(currentProcess->name);
            }
        }
        sleep(5);
    }

    free(processes);
    return 0;
}
