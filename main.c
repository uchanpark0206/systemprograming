#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

int main(void) {
    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        // 1. 프롬프트 출력 (버퍼를 비워 즉시 화면에 표시)
        printf("%% ");
        fflush(stdout);

        // 2. 한 줄 입력 받기 (Ctrl+D 입력 시 종료)
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        // 3. 공백/탭/개행 기준으로 분리 -> args 배열 구성
        int argc = 0;
        char *token = strtok(line, " \t\n");
        while (token != NULL && argc < MAX_ARGS - 1) {
            args[argc] = token;
            argc++;
            token = strtok(NULL, " \t\n");
        }
        args[argc] = NULL; // execvp() 인자 배열은 반드시 NULL로 종료

        // 엔터만 입력시 다시 프롬프트로
        if (argc == 0) {
            continue;
        }

        // 4. 내장 명령어 처리: exit, logout (부모 프로세스에서 직접 종료)
        if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "logout") == 0) {
            exit(0);
        }

        // 5. 외부 명령어 실행
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
        } else if (pid == 0) {
            // [자식 프로세스]
            if (execvp(args[0], args) < 0) {
                perror("execvp failed");
                exit(1); // exec 실패 시 자식이 쉘 루프를 돌지 않도록 즉시 종료
            }
        } else {
            // [부모 프로세스 (쉘)]
            int status;
            waitpid(pid, &status, 0); // 자식 종료 대기 및 회수(reaping)
        }
    }

    return 0;
}