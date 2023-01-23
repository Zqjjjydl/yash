#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "parse.h"
#include "command.h"
#include <signal.h>

#define CMDFAIL continue

void doNothing(int sig){
    printf("Do nothing after receiving signal %d\n",sig);
}

void doSomething(int sig){
    printf("Do something after receiving signal %d\n",sig);
}

int fileRedirect(command* cmd){
    if(cmd->inputFile!=NULL){
        int fd=open(cmd->inputFile,O_RDONLY);
        if(fd==-1){
            return -1;
        }
        
        dup2(fd,0);
    }
    if(cmd->outputFile!=NULL){
        int fd=open(cmd->outputFile,O_WRONLY|O_CREAT,S_IRWXU);
        if(fd==-1){
            return -1;
        }
        dup2(fd,1);
    }
    if(cmd->errorOutputFile!=NULL){
        int fd=open(cmd->errorOutputFile,O_WRONLY|O_CREAT,S_IRWXU);
        if(fd==-1){
            return -1;
        }
        dup2(fd,2);
    }
    return 0;
}

int main(){
    char* buffer=NULL;
    char* tokenList[2010];
    int tokenCnt=0;
    //yash should ignore sigtstp
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    // The signal disposition is a per-process attribute: in a
    // multithreaded application, the disposition of a particular signal
    // is the same for all threads.

    // A child created via fork(2) inherits a copy of its parent's
    // signal dispositions.  During an execve(2), the dispositions of
    // handled signals are reset to the default; the dispositions of
    // ignored signals are left unchanged.

    // signal(SIGCHLD, doNothing);
    while(1){
        buffer=readline("# ");
        char* nextToken=strtok(buffer," ");
        if(nextToken==NULL){
            continue;
        }
        do{
            tokenList[tokenCnt++]=nextToken;
        } while (nextToken=strtok(NULL," "));
        //parse command
        //look for pipe, exec two seperate command
        int pipeIdx=-1;
        for(int i=0;i<tokenCnt;i++){
            if(strcmp(tokenList[i],"|")==0){
                pipeIdx=i;
                break;
            }
        }
        if(pipeIdx==-1){
            //for single command, fork, file redirection, exec
            command* cmd=parse(tokenList,0,tokenCnt);
            int cpid=fork();
            if(cpid==0){
                //child
                //do file redirection
                if(fileRedirect(cmd)==-1){
                    CMDFAIL;
                }
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                // setpgid(0,0); //stop signal is sent to everyone in a process group
                // signal(SIGTSTP, doSomething);
                // while(1){
                //     printf("Oh yeah!\n");
                //     sleep(1);
                // }
                int ret=execvp(cmd->cmd,cmd->arg);
                if(ret==-1){
                    CMDFAIL;
                }
            }
            waitpid(cpid,NULL,WUNTRACED);//should wait for signal

            // kill(cpid,SIGCONT);
            // printf("child continues");
            deleteCmd(cmd);
        }
        else{
            //for two commands, build pipe before exec two single command
            command* cmd1=parse(tokenList,0,pipeIdx);
            command* cmd2=parse(tokenList,pipeIdx+1,tokenCnt);
            int pfd[2];
            pipe(pfd);
            int cpid1=fork();
            if(cpid1==0){//left child, write to pipe
                if(fileRedirect(cmd1)==-1){
                    CMDFAIL;
                }
                dup2(pfd[1],1);
                close(pfd[0]);
                int ret=execvp(cmd1->cmd,cmd1->arg);
                if(ret==-1){
                    CMDFAIL;
                }
            }
            int cpid2=fork();
            if(cpid2==0){//right child, read from pipe
                if(fileRedirect(cmd2)==-1){
                    CMDFAIL;
                }
                dup2(pfd[0],0);
                close(pfd[1]);
                int ret=execvp(cmd2->cmd,cmd2->arg);
                if(ret==-1){
                    CMDFAIL;
                }
            }
            close(pfd[0]);
            close(pfd[1]);
            wait(NULL);
            wait(NULL);
            deleteCmd(cmd1);
            deleteCmd(cmd2);
        }
        
        free(buffer);
        buffer=NULL;
        tokenCnt=0;
    }
}