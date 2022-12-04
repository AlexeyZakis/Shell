#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_COMMAND_SIZE 1024
#define MAX_LINE_SIZE 512

void my_SIGINT(int sig) {}

void error(char *str) {
    printf("%s ERROR\n", str);
    exit(-1);
}
char *clearSpacesFromEnd(char *str) {
    int i, j = 0, numOfSpaces = 0;
    char *result = malloc(sizeof(str));
    for (i = 0; i < strlen(str); ++i) {
        if (str[i] == ' ') {
            ++numOfSpaces;
        }
        else {
            numOfSpaces = 0;
        }
        result[j++] = str[i];
    }
    result = realloc(result, sizeof(char) * (j - numOfSpaces + 1));
    result[j - numOfSpaces] = '\0';
    return result;
}
char *clearSpacesFromStart(char *str) {
    int i, j = 0, hasTextStarted = 0;
    char *result = malloc(sizeof(str));
    for (i = 0; i < strlen(str); ++i) {
        if (!hasTextStarted && str[i] == ' ') {
            continue;
        }
        else {
            hasTextStarted = 1;
            result[j++] = str[i];
        }
    }
    result = realloc(result, sizeof(char) * j);
    return result;
}
int isEqualStr(char *str1, char *str2) {
    int i;
    if (strlen(str1) != strlen(str2)) return 0;
    for (i = 0; i < strlen(str1); ++i) {
        if (str1[i] != str2[i]) return 0;
    }
    return 1;
}
char **getCutStartArgs(char **args, int numOfArgs) {
    int i;
    char **result = malloc(sizeof(char *) * (numOfArgs - 2));
    for (i = 0; i < numOfArgs - 2; ++i) {
        result[i] = args[i + 2];
    }
    return result;
}
char **getCutEndArgs(char **args, int numOfArgs) {
    int i;
    char **result = malloc(sizeof(char *) * (numOfArgs - 2));
    for (i = 0; i < numOfArgs - 2; ++i) {
        result[i] = args[i];
    }
    return result;
}
char *getStrUntilSepFrom(char *line, char sep, int *position) {
    int j = 0;
    if (*position > strlen(line)) return "";
    char *result = malloc(sizeof(line));
    for (; *position < strlen(line); ++(*position)) {
        if (line[*position] == sep) {
            ++(*position);
            break;
        }
        result[j++] = line[*position];
    }
    result = realloc(result, sizeof(char) * j);
    return result;
}
char **splitLineBySep(char *line, char sep, int *position) {
    int bufferSize = MAX_LINE_SIZE, i = 0;
    char **tokens = malloc(bufferSize * sizeof(char*));
    char *token;

    if (!tokens) error("malloc");

    while (i < strlen(line)) {
        token = getStrUntilSepFrom(line, sep, &i);
        token = clearSpacesFromStart(token);
        token = clearSpacesFromEnd(token);
        tokens[(*position)++] = token;
        if (*position >= bufferSize) {
            bufferSize += MAX_LINE_SIZE;
            tokens = realloc(tokens, bufferSize * sizeof(char*));
            if (!tokens) error("tokens realloc");
        }
    }
    tokens[*position] = NULL;
    return tokens;
}
int executeCommand(int numOfArgs, char **args) {
    if (numOfArgs == 0) return 1;
    pid_t pid;

    int status;
    pid = fork();
    if (pid == 0) {
        if (numOfArgs > 2 && isEqualStr(args[0], "<")) {
            freopen(args[1], "r", stdin);
            args = getCutStartArgs(args, numOfArgs);
            numOfArgs -= 2;
        }
        if (numOfArgs > 2 && isEqualStr(args[numOfArgs - 2], ">")) {
            freopen(args[numOfArgs - 1], "w", stdout);
            args = getCutEndArgs(args, numOfArgs);
            numOfArgs -= 2;
        }
        else if (numOfArgs > 2 && isEqualStr(args[numOfArgs - 2], ">>")) {
            freopen(args[numOfArgs - 1], "a", stdout);
            args = getCutEndArgs(args, numOfArgs);
            numOfArgs -= 2;
        }
        args = realloc(args, sizeof(args[0]) * (numOfArgs + 1));
        args[numOfArgs] = NULL;
        if (execvp(args[0], args) == -1) error("execvp");
        exit(0);
    }
    else if (pid < 0) error("pid");
    else {
        /* WUNTRACED - означает возврат управления и для остановленных
         * (но не отслеживаемых) дочерних процессов, о статусе которых еще не было сообщено.
         * WIFEXITED - не равно 0, если дочерний процесс успешно завершился.
         * WIFSIGNALED - возвращает 1, если дочерний процесс завершился
         * из-за необработанного сигнала. */
        do waitpid(pid, &status, WUNTRACED);
        while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}
int execute(int numOfCommands, char **commands, int background) {
    pid_t pid;
    int fd[2], input, numOfArgs = 0;
    char **args;
    for (int i = 0; i < numOfCommands; ++i) {
        pipe(fd);
        if (!(pid = fork())) {
            if (i != 0) {
                dup2(input, 0);
                close(input);
            }
            if (i != numOfCommands - 1) {
                dup2(fd[1], 1);
            }
            close(fd[0]);
            close(fd[1]);

            numOfArgs = 0;
            args = splitLineBySep(commands[i], ' ', &numOfArgs);
            executeCommand(numOfArgs, args);
            free(args);
            exit(-1);
        }
        if (i != 0) {
            close(input);
        }
        close(fd[1]);
        input = fd[0];
    }
    close(input);
    if (!background) {
        while ((pid = wait(0)) != -1);
    }
    else {
        exit(0);
    }
}
char *readLine(void) {
    int bufferSize = MAX_COMMAND_SIZE;
    int position = 0;
    int c;

    char *buffer = malloc(sizeof(char) * bufferSize);

    if (!buffer) error("malloc");

    while (1) {
        c = getchar();
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;
        if (position >= bufferSize) {
            bufferSize += MAX_COMMAND_SIZE;
            buffer = realloc(buffer, bufferSize);
            if (!buffer) error("buffer realloc");
        }
    }
}
void shell(void) {
    pid_t pid;
    char *line;
    char **commands;
    int numOfCommands, numOfArgs = 0;
    while (1) {
        line = readLine();
        numOfCommands = 0;
        commands = splitLineBySep(line, '|', &numOfCommands);
        if (isEqualStr(splitLineBySep(commands[numOfCommands - 1], ' ', &numOfArgs)[numOfArgs - 1], "&")) {
            commands[numOfCommands - 1][strlen(commands[numOfCommands - 1]) - 1] = ' ';
            commands[numOfCommands - 1] = clearSpacesFromEnd(commands[numOfCommands - 1]);
            if (!(pid = fork())) {
                setpgid(0, 0);
                signal(SIGINT, SIG_IGN);
                execute(numOfCommands, commands, 1);
            }
        }
        else {
            execute(numOfCommands, commands, 0);
        }
        free(commands);
        free(line);
    }
}
int main() {
    shell();
    return 0;
}
