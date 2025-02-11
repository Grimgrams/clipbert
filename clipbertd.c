#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/_types/_pid_t.h>
#include <sys/_types/_ssize_t.h>
#include <sys/syslog.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>

#include <ApplicationServices/ApplicationServices.h>

#define BUFFER_SIZE 1024

struct clip_out{
    char *first;
    char *second;
};

int get_clipboard(void);
void clipbertd(); // actual clipbert daemon process


int compare_clips(char*);
int save_clip(char*);

CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);
int env_tap();

int main(int argc, char *argv[]){
    clipbertd();
    return EXIT_SUCCESS;
}


int get_clipboard(){
    int pipefd[2];
    pid_t pid;
    char buffer[BUFFER_SIZE] ={0};



    if (pipe(pipefd) == -1) {
        perror("Pipe failed");
        return EXIT_FAILURE;
    }

    pid = fork();

    if (pid<0) {
        perror("Fork failed");
        return EXIT_FAILURE;
    } else if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        execl("/usr/bin/pbpaste", "pbpaste", NULL);

        perror("exec failed");
        return EXIT_FAILURE;
    } else {
        close(pipefd[1]);

        ssize_t bytesRead = read(pipefd[0],buffer, BUFFER_SIZE-1);
        if (bytesRead>0) {
            buffer[bytesRead] = '\0';
            fprintf(stdout, "Copied: %s\n", buffer);
            save_clip(buffer);
        }else {
            close(pipefd[0]); // Close read end
                    wait(NULL); // Wait for child to finish
                    printf("Completed\n"); // remove later
        }
    }
    return EXIT_SUCCESS;
}

void clipbertd(){
    pid_t pid = fork();

    if (pid < 0) {exit(EXIT_FAILURE);}
    if (pid > 0) {exit(EXIT_SUCCESS);}

    // create new session and process group
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    // handle signal
    signal(SIGCHLD,  SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    chdir("/");

    int fdc; // file descriptor close
    for (fdc = sysconf(_SC_OPEN_MAX); fdc >=0; fdc--) {
        close(fdc);
    }

    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);

    openlog("Clipbert", LOG_PID, LOG_DAEMON);

    while(1){
        syslog(LOG_NOTICE, "CLIPBERT IS RUNNING EVERYONE HIDE!");
        sleep(30);
        env_tap(); // listen for key stroke
    }
}


int save_clip(char *clip){
    FILE *clip_contents;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    clip_contents = fopen("/Users/grimgram/.clipbert", "a");

    fprintf(clip_contents, "\n[%d-%02d-%02d %02d:%02d:%02d]: %s\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, clip);

    fclose(clip_contents);

    return 0;
}



CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    if (type == kCGEventKeyDown) {
        CGKeyCode keyCode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
        CGEventFlags flags = CGEventGetFlags(event);

        // Check for Cmd + C
        if ((flags & kCGEventFlagMaskCommand) && keyCode == 8 || (flags & kCGEventFlagMaskCommand) && keyCode == 7) { // Keycode 8 = 'C'
            printf("Cmd + C or Cmd + x detected!\n");
            sleep(1);
            get_clipboard();
        }
    }
    return event;
}


int env_tap(){
    CFMachPortRef eventTap = CGEventTapCreate(
            kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
            CGEventMaskBit(kCGEventKeyDown), eventCallback, NULL);

        if (!eventTap) {
            fprintf(stderr, "Failed to create event tap\n");
            return 1;
        }

        // Create a run loop source and add it to the main run loop
        CFRunLoopSourceRef runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
        CGEventTapEnable(eventTap, true);

        // Run loop to keep daemon active
        CFRunLoopRun();
        return 0;
}
