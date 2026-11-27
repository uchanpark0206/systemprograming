#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>


#define MAXLINE 8192
#define MAXARGS 128

/* 함수 프로토타입 (PPT 4쪽 shellex.c 구조) */
void eval(char *cmdline);
int parseline(char *buf, char **argv);

int builtin_command(char **argv);

/* 시그널 핸들러 프로토타입 (10장 PPT 방식) */
void sigalrm_handler(int sig);

int main(void) {
    char cmdline[MAXLINE];

    /* 쉘 자체는 Ctrl+C(SIGINT), Ctrl+Z(SIGTSTP)에 죽거나 중단되지 않도록 무시  SIG_IGN는 해당 시그널을 무시는 메크로*/


    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);


    /* ALARM 명령어용 SIGALRM 핸들러 등록 (PPT 30쪽, 47쪽 참고) */
    signal(SIGALRM, sigalrm_handler);

    while (1) {

        /* Read */
        printf("%% ");
        fflush(stdout);

        if (fgets(cmdline, MAXLINE, stdin) == NULL) {
            if (feof(stdin)) {
                printf("\n");
                exit(0);
            }
            /* 시그널(SIGALRM 등)에 의해 fgets가 중단된 경우 루프 재개 */
            clearerr(stdin);
            continue;
        }


        /* Evaluate */
        eval(cmdline);

    }

    return 0;
}

/* eval: 명령어 한 줄을 파싱하고 실행 */
void eval(char *cmdline) {
    char *argv[MAXARGS];
    char buf[MAXLINE];
    pid_t pid;

    strcpy(buf, cmdline);
    parseline(buf, argv);

    if (argv[0] == NULL) {
        return; /* 빈 줄 무시 */
    }

    /* 내장 명령어가 아니면 자식 프로세스 생성 후 실행 */
    if (!builtin_command(argv)) {
        if ((pid = fork()) < 0) {
            perror("fork error");
            return;
        }

        if (pid == 0) {
            /* [자식 프로세스] */
            /* 가산점: 포그라운드 자식은 Ctrl+C(SIGINT)를 받으면 종료되도록 기본 동작 복구 */
            signal(SIGINT, SIG_DFL);



            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(1);
            }
        } else {
            /* [부모 프로세스 (쉘)] */
            int status;

            if (waitpid(pid, &status, 0) < 0) {
                perror("waitpid error");
            }

        }
    }
}

/* builtin_command: 내장 명령어이면 실행 후 1 반환, 아니면 0 반환 */
int builtin_command(char **argv) {
    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "logout") == 0) {
        exit(0);
    }

    /* 추후 여기에 cd, DIR, ALARM 분기 추가 */

    return 0;
}

/* parseline: 입력 문자열을 공백/탭/개행 기준으로 분리해 argv 배열 구성 */
int parseline(char *buf, char **argv) {
    int argc = 0;
    char *token = strtok(buf, " \t\n");

    while (token != NULL && argc < MAXARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;

    return 0;
}

/* SIGALRM 핸들러 (ALARM 명령어 만료 시 호출) */
void sigalrm_handler(int sig) {
    printf("\nAlarm Arrived!!\n%% ");
    fflush(stdout);
=======

}