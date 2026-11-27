#define MAXLINE  8192  /* Max text line length */
#define MAXARGS  128
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void do_cd(char **argv);
void do_dir(char **argv);
void do_alarm(char **argv);

int main()
{
    char cmdline[MAXLINE];

    /* ^C, ^Z 를 눌러도 shell 자신은 죽거나 background가 되지 않도록 무시 */
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (1) {
        /* read */
        printf(">>");
        fflush(stdout);
        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {          /* Ctrl+D */
                printf("\n");
                exit(0);
            }
            clearerr(stdin);
            continue;
        }

        /* evaluate */
        eval(cmdline);
    }
}

/* eval */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    /* 이미 끝난 background 자식 프로세스(좀비)를 회수 */
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;

    if (!builtin_command(argv)) {
        if ((pid = fork()) < 0) {
            perror("fork");
            return;
        }
        if (pid == 0) {
            /* 자식: 포그라운드면 ^C로 종료 가능(가산점), 백그라운드면 ^C 무시.
               ^Z(SIGTSTP)는 shell에서 상속받은 "무시" 상태 그대로 둠 */
            signal(SIGINT, bg ? SIG_IGN : SIG_DFL);
            if (execvp(argv[0], argv) < 0) {      /* PATH에서 찾아 실행 */
                fprintf(stderr, "%s: Command not found.\n", argv[0]);
                _exit(127);
            }
        }

        if (!bg) {
            int status;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
                ;
        }
        else
            printf("%d %s", pid, cmdline);
    }
    return;
}

/* builtin - shell이 직접 처리해야 하는 명령 */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "exit") || !strcmp(argv[0], "logout"))
        exit(0);
    if (!strcmp(argv[0], "&"))
        return 1;
    if (!strcmp(argv[0], "cd")) {
        do_cd(argv);
        return 1;
    }
    if (!strcmp(argv[0], "DIR")) {
        do_dir(argv);
        return 1;
    }
    if (!strcmp(argv[0], "ALARM")) {
        do_alarm(argv);
        return 1;
    }
    return 0;   /* ls, pwd, date, cat, cp, rm, rmdir, mv, mkdir 등은 fork+exec로 실행 */
}

/* cd [path] : 인자가 없으면 $HOME 으로 이동 */
void do_cd(char **argv)
{
    char *path = argv[1];

    if (path == NULL)
        path = getenv("HOME");
    if (path == NULL || chdir(path) < 0)
        perror("cd");
}

/* DIR path : (cd path; ls -l) 과 비슷하지만 출력 형식이 다름
   파일타입  크기(바이트)  수정시각(년-월-일-시-분)  권한(8진수)  이름  (필드 사이 공백 2칸) */
void do_dir(char **argv)
{
    const char *path = argv[1] ? argv[1] : ".";
    struct dirent **list;
    struct stat st;
    char full[MAXLINE], tbuf[32], type;
    int n, i;

    n = scandir(path, &list, NULL, alphasort);
    if (n < 0) {
        perror(path);
        return;
    }
    for (i = 0; i < n; i++) {
        char *name = list[i]->d_name;

        if (name[0] != '.') {                    /* ls -l 처럼 숨김 파일은 제외 */
            snprintf(full, sizeof(full), "%s/%s", path, name);
            if (lstat(full, &st) < 0)
                perror(full);
            else {
                if (S_ISDIR(st.st_mode))       type = 'd';
                else if (S_ISREG(st.st_mode))  type = 'f';
                else if (S_ISLNK(st.st_mode))  type = 'l';
                else                           type = '-';
                strftime(tbuf, sizeof(tbuf), "%Y-%m-%d-%H-%M", localtime(&st.st_mtime));
                printf("%c  %ld  %s  %03o  %s\n",
                       type, (long)st.st_size, tbuf, (unsigned)(st.st_mode & 0777), name);
            }
        }
        free(list[i]);
    }
    free(list);
}

/* ALARM min:sec : 시간이 지나면 "Alarm Arrived!!" 출력. 인자가 없으면 10초 */
void do_alarm(char **argv)
{
    int min = 0, sec = 10;
    pid_t pid;

    if (argv[1] != NULL) {
        if (sscanf(argv[1], "%d:%d", &min, &sec) != 2 ||
            min < 0 || min > 59 || sec < 0 || sec > 59) {
            fprintf(stderr, "usage: ALARM min:sec (min, sec: 0 ~ 59)\n");
            return;
        }
    }

    if ((pid = fork()) < 0)
        perror("fork");
    else if (pid == 0) {          /* 자식이 기다렸다가 메시지 출력 -> shell은 계속 사용 가능 */
        sleep(min * 60 + sec);
        printf("\nAlarm Arrived!!\n");
        fflush(stdout);
        _exit(0);
    }
}

/* parseline - 입력 줄을 공백 기준으로 잘라 argv에 저장, 마지막이 &면 background(1) 반환 */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0)  /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
