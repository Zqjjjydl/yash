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

#define CMDFAIL continue

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
    return 0;
}

int main(){
    char* buffer=NULL;
    char* tokenList[2010];
    int tokenCnt=0;
    while(1){
        buffer=readline("# ");
        char* nextToken=strtok(buffer," ");
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
                int ret=execvp(cmd->cmd,cmd->arg);
                if(ret==-1){
                    CMDFAIL;
                }
            }
            wait(NULL);//TODO should wait for signal
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