#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
 
#define NUM_CHILDREN 3
#define MSG_SIZE 256
 
volatile sig_atomic_t got_sigint = 0;
 
/* Ctrl+C handler: just set a flag, main loop prints the message */
void handle_sigint(int sig) {
    got_sigint = 1;
}
 
int main(void) {
    int fd[NUM_CHILDREN][2];      /* one pipe per child */
    char message[MSG_SIZE];
    int i, j;
 
    /* Set up Ctrl+C handler (no SA_RESTART so fgets gets interrupted) */
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
 
    /* 1. Create all pipes first */
    for (i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(fd[i]) == -1) {
            perror("pipe");
            exit(1);
        }
    }
 
    /* 2. Fork the children */
    for (i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
 
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
 
        if (pid == 0) {
            /* ---- CHILD ---- */
            signal(SIGINT, SIG_IGN);          /* only the parent handles Ctrl+C */
 
            for (j = 0; j < NUM_CHILDREN; j++)
                close(fd[j][1]);              /* child only reads */
 
            char buf[MSG_SIZE];
            ssize_t n;
            while ((n = read(fd[i][0], buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                printf("  Child %d (PID %d) received: %s", i + 1, getpid(), buf);
                fflush(stdout);
            }
 
            printf("  Child %d (PID %d) exiting.\n", i + 1, getpid());
            exit(0);
        }
    }
 
    /* ---- PARENT ---- */
    for (i = 0; i < NUM_CHILDREN; i++)
        close(fd[i][0]);                      /* parent only writes */
 
    printf("Parent PID %d created %d children.\n", getpid(), NUM_CHILDREN);
 
    /* 3. Read message from keyboard, send to every child via pipe */
    while (!got_sigint) {
        printf("Enter a message (type 'exit' to quit): ");
        fflush(stdout);
 
        if (fgets(message, sizeof(message), stdin) == NULL)
            break;                            /* Ctrl+D or interrupted */
 
        if (strcmp(message, "exit\n") == 0) {
            printf("\n[EXIT] You typed 'exit'. Shutting down normally...\n");
            break;
        }
 
        for (i = 0; i < NUM_CHILDREN; i++)
            write(fd[i][1], message, strlen(message));
 
        usleep(100000);                       /* let children print first */
    }
 
    /* 4. Interrupt message */
    if (got_sigint)
        printf("\n[INTERRUPT] Ctrl+C (SIGINT) detected! Program was interrupted by the user.\n");
 
    for (i = 0; i < NUM_CHILDREN; i++)
        close(fd[i][1]);                      /* children see EOF and exit */
 
    for (i = 0; i < NUM_CHILDREN; i++)
        wait(NULL);
 
    if (got_sigint)
        printf("Parent: children stopped. Terminated by interrupt.\n");
    else
        printf("Parent: all children finished. Goodbye!\n");
    return 0;
}
