#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdbool.h>
#include <sys/wait.h>

#define LINEBUFFERSIZE 2
#define TOKENBUFFERSIZE 2
#define TOKENDELIMITERS " \t\r\n\a"

void spaceAllocationError()
{
        fprintf(stderr, "Unable to allocate space\n");
        exit(EXIT_FAILURE);
}

char *builtInStrings[] = {
    (char *)"cd",
    (char *)"echo",
    (char *)"help",
    (char *)"ls",
    (char *)"pwd",
    (char *)"texit",
};

int cd(char **args)
{
        const struct passwd *pw = getpwuid(getuid());
        const char *HOME = pw->pw_dir;
        if (args[1] == NULL)
                chdir(HOME);
        else if (chdir(args[1]) != 0)
                perror("Error");
        return 1;
}

int echo(char **args)
{
        return 1;
}

int texit(char **args)
{
        return 0;
}

int help(char **args)
{
        int i, n = sizeof(builtInStrings) / sizeof(char *);
        printf("Type program names and arguments, and hit enter.\n");
        printf("The following are built in:\n");

        for (i = 0; i < n; i++)
                printf("  %s\n", builtInStrings[i]);

        printf("Use the man command for information on other programs.\n");
        return 1;
}

int ls(char **args)
{
        return 1;
}

int pwd(char **args)
{
        char *cwd = (char *)malloc(sizeof(char) * 9056);
        char *pwd = (char *)malloc(sizeof(char) * 9056);
        getcwd(cwd, 9056);
        if (!cwd)
                cwd = (char *)"Directory";
        if (strcmp(args[0], "0") == 0)
        {
                const struct passwd *pw = getpwuid(getuid());
                const char *HOME = pw->pw_dir;
                const char *TILDA = "~";
                const size_t HOMELEN = strlen(HOME);
                size_t i;
                for (i = 0; i < HOMELEN; i++)
                        if (cwd[i] != HOME[i])
                                break;
                if (i == HOMELEN)
                {
                        strcat(pwd, TILDA);
                        strcat(pwd, &cwd[i]);
                        printf("%s", pwd);
                }
                else
                        printf("%s", cwd);
        }
        else
                printf("%s\n", cwd);
        return 1;
}

void printPrompt()
{
        char *username = getlogin();
        if (!username)
                username = (char *)"Username";
        char *hostname = (char *)malloc(sizeof(char) * 9056);
        gethostname(hostname, 9056);
        if (!hostname)
                hostname = (char *)"Hostname";
        printf("<%s@%s:", username, hostname);
        char *zero[] = {"0"};
        pwd(zero);
        printf("> ");
}

char *readLine()
{
        int bufsize = LINEBUFFERSIZE, pos = 0, ch;
        char *buffer = (char *)malloc(sizeof(char) * bufsize);

        if (!buffer)
                spaceAllocationError();

        while (1)
        {
                ch = getchar();
                if (ch == EOF || ch == '\n')
                {
                        buffer[pos] = '\0';
                        return buffer;
                }
                else
                        buffer[pos] = ch;
                pos++;
                if (pos >= bufsize)
                {
                        bufsize *= LINEBUFFERSIZE;
                        buffer = (char *)realloc(buffer, bufsize);
                        if (!buffer)
                                spaceAllocationError();
                }
        }
}

char **tokenizeLine(char *line)
{
        int bufsize = TOKENBUFFERSIZE, pos = 0;
        char **tokens = (char **)malloc(bufsize * sizeof(char *));
        char *token;

        if (!tokens)
                spaceAllocationError();

        for (token = strtok(line, TOKENDELIMITERS); token != NULL; token = strtok(NULL, TOKENDELIMITERS))
        {
                tokens[pos] = token;
                pos++;

                if (pos >= bufsize)
                {
                        bufsize *= TOKENBUFFERSIZE;
                        tokens = (char **)realloc(tokens, bufsize * sizeof(char *));
                        if (!tokens)
                                spaceAllocationError();
                }
        }
        tokens[pos] = NULL;
        return tokens;
}

int (*builtInFunctions[])(char **) = {
    &cd,
    &echo,
    &help,
    &ls,
    &pwd,
    &texit,
};

int launchCommand(char **args)
{
        pid_t pid, wpid;
        int status;

        pid = fork();
        if (pid == 0)
        {
                if (execvp(args[0], args) == -1)
                        perror("Error");
                exit(EXIT_FAILURE);
        }
        else if (pid < 0)
                perror("Error forking");
        else
                do
                {
                        wpid = waitpid(pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        return wpid;
}

int executeCommand(char **args)
{
        int i, n = sizeof(builtInStrings) / sizeof(char *);
        if (args[0] == NULL)
                return 1;
        for (i = 0; i < n; i++)
                if (strcmp(args[0], builtInStrings[i]) == 0)
                        return (*builtInFunctions[i])(args);
        return launchCommand(args);
}

void loop()
{
        char *line;
        int status = 1;
        char **args;

        do
        {
                printPrompt();
                line = readLine();
                args = tokenizeLine(line);
                status = executeCommand(args);

                free(line);
                free(args);
        } while (status);
}

int main()
{
        loop();
        return EXIT_SUCCESS;
}
