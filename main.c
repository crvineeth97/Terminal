#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <grp.h>
#include <termios.h>
#include <assert.h>
#include <errno.h>

#define MAX_BUF_LEN 1024
#define LINEBUFFERSIZE 2
#define TOKENBUFFERSIZE 2
#define TOKENDELIMITERS " \t\r\n\a"
#define CHILDPROCESSESLEN 128

void spaceAllocationError()
{
        fprintf(stderr, "Unable to allocate space\n");
        exit(EXIT_FAILURE);
}

pid_t childProcesses[CHILDPROCESSESLEN] = {};
char childNames[CHILDPROCESSESLEN][256] = {};

char *builtInStrings[] = {
    (char *)"cd",
    (char *)"help",
    (char *)"ls",
    (char *)"nightswatch",
    (char *)"pinfo",
    (char *)"pwd",
};

int arrayLen(char **arr)
{
        int i;
        for (i = 0; arr[i] != NULL; i++)
                ;
        return i;
}

int getch(void)
{
        int c = 0;

        struct termios org_opts, new_opts;
        int res = 0;
        //-----  store old settings -----------
        res = tcgetattr(STDIN_FILENO, &org_opts);
        assert(res == 0);
        //---- set new terminal parms --------
        memcpy(&new_opts, &org_opts, sizeof(new_opts));
        new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
        c = getchar();
        //------  restore old settings ---------
        res = tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
        assert(res == 0);
        return (c);
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
                        buffer[pos++] = '\0';
                        return buffer;
                }
                else
                        buffer[pos++] = ch;
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
                tokens[pos++] = token;
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

int cd(char **args)
{
        const struct passwd *pw = getpwuid(getuid());
        const char *HOME = pw->pw_dir;
        char tmp[9056] = "";
        if (args[1] == NULL)
        {
                if (chdir(HOME) != 0)
                        perror("Error");
        }
        else if (args[1][0] == '~')
        {
                const struct passwd *pw = getpwuid(getuid());
                const char *HOME = pw->pw_dir;
                strcat(tmp, HOME);
                strcat(tmp, &args[1][1]);
                if (chdir(tmp) != 0)
                        perror("Error");
        }
        else
        {
                if (chdir(args[1]) != 0)
                        perror("Error");
        }
        return 1;
}

int echo(char *line)
{
        int i;
        bool flg1 = 0;
        bool flg2 = 0;
        int bufsize = LINEBUFFERSIZE, pos = 0, ch;
        char *buffer = (char *)malloc(sizeof(char) * bufsize);
        line = &line[5];
        do
        {
                int len = strlen(line);
                for (i = 0; i < len; i++)
                {
                        ch = line[i];
                        if (ch == '\'')
                        {
                                if (flg2)
                                        buffer[pos++] = ch;
                                else
                                        flg1 = !flg1;
                        }
                        else if (ch == '\"')
                        {
                                if (flg1)
                                        buffer[pos++] = ch;
                                else
                                        flg2 = !flg2;
                        }
                        else
                                buffer[pos++] = ch;
                        if (pos >= bufsize)
                        {
                                bufsize *= LINEBUFFERSIZE;
                                buffer = (char *)realloc(buffer, bufsize);
                        }
                }
                if (flg1 || flg2)
                {
                        printf("> ");
                        line = readLine();
                }
                buffer[pos++] = '\n';
                // strcat(friend, "\n");
        } while (flg1 || flg2);
        buffer[pos] = 0;
        printf("%s", buffer);
        free(buffer);
        buffer = NULL;
        return 1;
}

int help(char **args)
{
        int i, n = sizeof(builtInStrings) / sizeof(char *);
        printf("Type program names and arguments, and hit enter.\n");
        printf("The following are built in:\n");
        for (i = 0; i < n; i++)
                printf("  %s\n", builtInStrings[i]);
        printf("  echo\n");
        printf("Use the man command for information on other programs.\n");
        return 1;
}

int ls(char **args)
{
        DIR *mydir;
        struct dirent *myfile;
        struct stat fileStat;
        int flagL, flagA, count;
        count = flagL = flagA = 0;
        int s = arrayLen(args);
        int i;
        for (i = 1; i < s; i++)
        {
                if (strcmp(args[i], "-l") == 0)
                {
                        flagL = 1;
                        count++;
                }

                if (strcmp(args[i], "-a") == 0)
                {
                        flagA = 1;
                        count++;
                }

                if (strcmp(args[i], "-la") == 0 || strcmp(args[i], "-al") == 0)
                {
                        flagA = 1;
                        flagL = 1;
                        count++;
                }
        }

        if (s - count == 1)
        {
                mydir = opendir("./");

                if (flagL == 1)
                {
                        // if(s-count>1)
                        //   printf("%s:\n",args[i]);
                        char buf[512];
                        while ((myfile = readdir(mydir)) != NULL)
                        {
                                if (flagA == 0)
                                {
                                        if (myfile->d_name[0] == '.')
                                                continue;
                                }
                                sprintf(buf, "%s/%s", "./", myfile->d_name);
                                stat(buf, &fileStat);

                                struct group *user_group = getgrgid((long)fileStat.st_gid);
                                struct passwd *user_name = getpwuid((long)fileStat.st_uid);

                                printf((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
                                printf((fileStat.st_mode & S_IRUSR) ? "r" : "-");
                                printf((fileStat.st_mode & S_IWUSR) ? "w" : "-");
                                printf((fileStat.st_mode & S_IXUSR) ? "x" : "-");
                                printf((fileStat.st_mode & S_IRGRP) ? "r" : "-");
                                printf((fileStat.st_mode & S_IWGRP) ? "w" : "-");
                                printf((fileStat.st_mode & S_IXGRP) ? "x" : "-");
                                printf((fileStat.st_mode & S_IROTH) ? "r" : "-");
                                printf((fileStat.st_mode & S_IWOTH) ? "w" : "-");
                                printf((fileStat.st_mode & S_IXOTH) ? "x" : "-");

                                printf("  %lu", fileStat.st_nlink);

                                printf(" %4s %4s ", user_name->pw_name, user_group->gr_name);

                                printf("%lu", fileStat.st_size);

                                char *foo = ctime(&fileStat.st_ctime);
                                foo[strlen(foo) - 1] = 0;

                                printf(" %s", foo);

                                printf(" %s \n", myfile->d_name);

                                // free(foo);
                                // free(user_group);
                                // free(user_name);
                        }
                }

                else
                {
                        // if(s-count>1)
                        // printf("%s:\n",args[i]);
                        //char buf[512];
                        while ((myfile = readdir(mydir)) != NULL)
                        {
                                if (flagA == 0)
                                {
                                        if (myfile->d_name[0] == '.')
                                                continue;
                                }
                                //  sprintf(buf, "%s/%s", argv[1], myfile->d_name);
                                //  stat(buf, &fileStat);
                                printf(" %s \n", myfile->d_name);
                        }
                }
        }

        else
        {
                int i;
                for (i = 1; i < s; i++)
                {
                        //  printf("else condition");
                        if (args[i][0] == '-')
                        {
                                //printf("%s\n",args[i]);
                                continue;
                        }
                        //  printf("not continue %s \n",args[i]);
                        char tmp[512] = "";
                        if (args[i][0] == '~')
                        {
                                const struct passwd *pw = getpwuid(getuid());
                                const char *HOME = pw->pw_dir;
                                strcat(tmp, HOME);
                                strcat(tmp, &args[i][1]);
                                mydir = opendir(tmp);
                        }
                        else
                                mydir = opendir(args[i]);

                        //  printf("seg ?");

                        if (flagL == 1)
                        {
                                if (s - count > 2)
                                        printf("%s:\n", args[i]);
                                char buf[512];
                                while ((myfile = readdir(mydir)) != NULL)
                                {
                                        if (flagA == 0)
                                        {
                                                if (myfile->d_name[0] == '.')
                                                        continue;
                                        }
                                        if (args[i][0] == '~')
                                                sprintf(buf, "%s/%s", tmp, myfile->d_name);
                                        else
                                                sprintf(buf, "%s/%s", args[i], myfile->d_name);
                                        stat(buf, &fileStat);

                                        struct group *user_group = getgrgid((long)fileStat.st_gid);
                                        struct passwd *user_name = getpwuid((long)fileStat.st_uid);

                                        printf((S_ISDIR(fileStat.st_mode)) ? "d" : "-");
                                        printf((fileStat.st_mode & S_IRUSR) ? "r" : "-");
                                        printf((fileStat.st_mode & S_IWUSR) ? "w" : "-");
                                        printf((fileStat.st_mode & S_IXUSR) ? "x" : "-");
                                        printf((fileStat.st_mode & S_IRGRP) ? "r" : "-");
                                        printf((fileStat.st_mode & S_IWGRP) ? "w" : "-");
                                        printf((fileStat.st_mode & S_IXGRP) ? "x" : "-");
                                        printf((fileStat.st_mode & S_IROTH) ? "r" : "-");
                                        printf((fileStat.st_mode & S_IWOTH) ? "w" : "-");
                                        printf((fileStat.st_mode & S_IXOTH) ? "x" : "-");

                                        printf("  %lu", fileStat.st_nlink);

                                        printf(" %4s %4s ", user_name->pw_name, user_group->gr_name);

                                        printf("%lu", fileStat.st_size);

                                        char *foo = ctime(&fileStat.st_ctime);
                                        foo[strlen(foo) - 1] = 0;

                                        printf(" %s", foo);

                                        printf(" %s \n", myfile->d_name);

                                        // free(foo);
                                        // free(user_group);
                                        // free(user_name);
                                }
                        }

                        else
                        {
                                if (s - count > 2)
                                        printf("%s:\n", args[i]);
                                //char buf[512];
                                while ((myfile = readdir(mydir)) != NULL)
                                {
                                        if (flagA == 0)
                                        {
                                                if (myfile->d_name[0] == '.')
                                                        continue;
                                        }
                                        //  sprintf(buf, "%s/%s", argv[1], myfile->d_name);
                                        //  stat(buf, &fileStat);
                                        printf(" %s \n", myfile->d_name);
                                }
                        }
                } //end of for
        }

        closedir(mydir);
        // free(mydir);
        // free(myfile);
        return 1;
}

int nightswatch(char **args)
{
        int slp = 1;
        char argum[128];
        if (strcmp(args[1], "-n") == 0)
        {
                strcpy(argum, args[3]);
                slp = atoi(args[2]);
        }
        else
                strcpy(argum, args[1]);
        if (strcmp(argum, "dirty") == 0)
        {
                pid_t pid = fork();
                if (pid == 0)
                {
                        char ch;
                        while (1)
                        {
                                ch = getch();
                                if (ch == 'q' || ch == 'Q')
                                        break;
                        }
                        exit(EXIT_FAILURE);
                }
                else if (pid < 0)
                {
                        perror("Error");
                        exit(EXIT_FAILURE);
                }
                else
                {
                        do
                        {
                                int cstatus = 10;
                                waitpid(pid, &cstatus, WNOHANG);
                                if (cstatus != 10)
                                        break;
                                FILE *fp = fopen("/proc/meminfo", "r");
                                if (!fp)
                                        perror("Error");
                                char line[128];
                                char **arg;
                                do
                                {
                                        fgets(line, 128, fp);
                                        arg = tokenizeLine(line);
                                } while (strcmp(arg[0], "Dirty:") != 0);
                                fclose(fp);
                                printf("%d%s\n", atoi(arg[1]), arg[2]);
                                fflush(stdout);
                                sleep(slp);
                                free(arg);
                                arg = NULL;
                        } while (1);
                }
        }
        else if (strcmp(argum, "interrupt") == 0) //NIGHTSWATCH PART 1
        {
                int flag = 0;
                pid_t pid = fork();
                if (pid == 0)
                {
                        char ch;
                        while (1)
                        {
                                ch = getch();
                                if (ch == 'q' || ch == 'Q')
                                        break;
                        }
                        exit(EXIT_FAILURE);
                }
                else if (pid < 0)
                {
                        perror("Error");
                        exit(EXIT_FAILURE);
                }
                else
                {
                        do
                        {
                                int cstatus = 10;
                                waitpid(pid, &cstatus, WNOHANG);
                                if (cstatus != 10)
                                        break;
                                int s;
                                FILE *fp = fopen("/proc/interrupts", "r");
                                if (!fp)
                                        perror("Error");
                                char line[1024];
                                char **arg;
                                do
                                {
                                        fgets(line, 1024, fp);
                                        arg = tokenizeLine(line);
                                        s = arrayLen(arg);
                                        int i;
                                        if (flag == 0)
                                        {
                                                for (i = 0; i < s; i++)
                                                        printf("%s\t", arg[i]);
                                                flag = 1;
                                                printf("\n");
                                        }
                                } while (strcmp(arg[s - 1], "i8042") != 0);
                                fclose(fp);
                                int i;
                                for (i = 1; i < s - 3; i++)
                                        printf("%s\t", arg[i]);
                                printf("\n");
                                fflush(stdout);
                                sleep(slp);
                                free(arg);
                                arg = NULL;
                        } while (1);
                }
        }
        else
        {
                printf("Wrong argument.");
        }
        return 1;
}

int pinfo(char **args)
{
        int c, L = 0;
        pid_t Id;

        //2 cases, one passable argument
        //Clear buffer space before this

        if (args[1] == NULL)
                Id = getpid();

        else
                Id = atoi(args[1]);

        char Reuse_p[MAX_BUF_LEN];
        char buffer[MAX_BUF_LEN];

        printf("pid -- %d\n", Id);

        sprintf(Reuse_p, "/proc/%d/status", Id); //pass value

        FILE *fp = fopen(Reuse_p, "r");
        if (!fp)
        {

                perror("Error: could not open proc/pid/status file\n");
        }
        else
        {
                char status;
                do
                {
                        fgets(buffer, 256, fp);
                } while (buffer[0] != 'S');
                sscanf(buffer, "State:\t%c", &status);

                printf("Process Status -- %c\n", status);
                fclose(fp);
        }

        sprintf(Reuse_p, "/proc/%d/statm", Id);
        fp = fopen(Reuse_p, "r");
        if (!fp)
        {

                perror("Error: could not open proc/pid/statm file\n");
        }
        else
        {
                while ((c = fgetc(fp)) != ' ')
                        buffer[L++] = c;

                buffer[L] = '\0';

                printf("Memory -- %s\n", buffer);
                fclose(fp);
        }

        sprintf(Reuse_p, "/proc/%d/exe", Id);
        L = -4;
        L = readlink(Reuse_p, buffer, MAX_BUF_LEN - 1);
        if (L == -4)
        {

                perror("Error: could not open proc/pid/exe file\n");
        }
        else
        {
                buffer[L] = '\0';

                //mod_cwd_rel(buffer);

                printf("Executable Path -- %s\n", buffer);
        }
        return 1;
}

int pwd(char **args)
{
        char *cwd = (char *)malloc(sizeof(char) * 9056);
        char *pwd1 = (char *)malloc(sizeof(char) * 9056);
        pwd1[0] = 0;
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
                        strcat(pwd1, TILDA);
                        strcat(pwd1, &cwd[i]);
                        printf("%s", pwd1);
                }
                else
                        printf("%s", cwd);
        }
        else
                printf("%s\n", cwd);
        free(pwd1);
        pwd1 = NULL;
        free(cwd);
        cwd = NULL;
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
        free(hostname);
        hostname = NULL;
}

int (*builtInFunctions[])(char **) = {
    &cd,
    &help,
    &ls,
    &nightswatch,
    &pinfo,
    &pwd,
};

int executeCommand(char **args)
{
        if (args[0] == NULL)
                return 1;
        if (strcmp(args[0], "cd") == 0)
                return cd(args);
        if (strcmp(args[0], "exit") == 0)
                return 0;
        int i, n = sizeof(builtInStrings) / sizeof(char *);
        pid_t pid;
        int status, len = arrayLen(args);
        pid = fork();
        if (pid == 0)
        {
                //Child process
                if (strcmp(args[len - 1], "&") == 0)
                        args[len - 1] = NULL;
                for (i = 0; i < n; i++)
                        if (strcmp(args[0], builtInStrings[i]) == 0)
                        {
                                (*builtInFunctions[i])(args);
                                exit(EXIT_FAILURE);
                        }
                if (execvp(args[0], args) == -1)
                        perror("Error");
                exit(EXIT_FAILURE);
        }
        else if (pid < 0)
                perror("Error forking");
        else
        {
                //Parent process
                if (strcmp(args[len - 1], "&") == 0)
                {
                        printf("Process ID of %s: %d\n", args[0], pid);
                        int i;
                        for (i = 0; i < CHILDPROCESSESLEN; i++)
                                if (childProcesses[i] == -5)
                                {
                                        childProcesses[i] = pid;
                                        strcpy(childNames[i], args[0]);
                                        break;
                                }
                        waitpid(pid, &status, WNOHANG);
                }
                else
                {
                        do
                        {
                                waitpid(pid, &status, WUNTRACED);
                        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
                }
        }
        return 1;
}

void doneProcesses()
{
        int i;
        for (i = 0; i < CHILDPROCESSESLEN; i++)
        {
                if (childProcesses[i] == -5)
                        continue;
                int cstatus = 10;
                waitpid(childProcesses[i], &cstatus, WNOHANG);
                // printf("%d %d\n", childProcesses[i], cstatus);
                if (cstatus != 10)
                {
                        fprintf(stderr, "%s with process ID %d exited normally\n", childNames[i], childProcesses[i]);
                        childProcesses[i] = -5;
                }
        }
}

void loop()
{
        char *line, *semiLine;
        int status = 1;
        char **args;
        do
        {
                printPrompt();
                line = readLine();
                /* OK so strtok cannot be used in a recursive fashion, cause it assumes the previous string to be in the buffer or something like that */
                char *saveptr;
                for (semiLine = strtok_r(line, ";", &saveptr); status && semiLine != NULL; semiLine = strtok_r(NULL, ";", &saveptr))
                {
                        args = tokenizeLine(semiLine);
                        if (strcmp(args[0], "echo") == 0)
                                status = echo(line);
                        else
                                status = executeCommand(args);
                }
                doneProcesses();
                free(line);
                free(args);
                line = NULL;
                args = NULL;

        } while (status);
}

int main()
{
        int i;
        for (i = 0; i < CHILDPROCESSESLEN; i++)
                childProcesses[i] = -5;
        loop();
        return 0;
}
