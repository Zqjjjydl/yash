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
#include "job.h"
#include <time.h>

job* dummyJobListHead;
int nextJobId=0;
void checkChild(int sig){
    // check each child
    // to remove all terminated process and print their information 
    // to change all stopped process status
    int status=0;
    job* curP=dummyJobListHead;
    while(curP&&curP->next){
        waitpid(curP->next->pgid,&status,WUNTRACED|WNOHANG);
        if(WIFSTOPPED(status)){
            curP->next->s=Stopped;
        }
        else if(WIFEXITED(status)){
            printJob(curP->next);
            //delete curP->next
            curP->next=curP->next->next;
        }
        curP=curP->next;
    }
}


int fileRedirect(command* cmd){
    if(cmd->inputFile!=NULL){
        int fd=open(cmd->inputFile,O_RDONLY);
        if(fd==-1){
            return -1;
        }
        dup2(fd,0);
        close(fd);
    }
    if(cmd->outputFile!=NULL){
        int fd=open(cmd->outputFile,O_WRONLY|O_CREAT,S_IRWXU);
        if(fd==-1){
            return -1;
        }
        dup2(fd,1);
        close(fd);
    }
    if(cmd->errorOutputFile!=NULL){
        int fd=open(cmd->errorOutputFile,O_WRONLY|O_CREAT,S_IRWXU);
        if(fd==-1){
            return -1;
        }
        dup2(fd,2);
        close(fd);
    }
    return 0;
}

int continueJob(){
    //called by yash, continue a job, return the pgid or -1 if failed
    job* cur=dummyJobListHead;
    while(cur&&cur->next){
        if((cur->next->s)==Stopped){
            kill(-cur->next->pgid,SIGCONT);
            cur->next->s=Running;
            return cur->next->pgid;
        }
        cur=cur->next;
    }
    return 1;
}

void addJob(char* jobString,int pgid,status s){
    job* temp=newJob(nextJobId++, s, jobString, pgid);
    temp->next=dummyJobListHead->next;
    dummyJobListHead->next=temp;
}

void printJobs(){
     job* cur=dummyJobListHead;
    while(cur->next!=NULL){
        printJob(cur->next);
        cur=cur->next;
    }
}

int main(){
    char* buffer=NULL;
    char* tokenList[2010];
    int tokenCnt=0;
    //yash should ignore sigtstp
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    dummyJobListHead=newJob(nextJobId++,Running,"",0);

    signal(SIGCHLD, checkChild);//when receiving child stop or ter
    while(1){
        buffer=readline("# ");
        if(buffer==NULL){//EOF
            exit(0);
        }
        if(strlen(buffer)==0){//empty
            continue;
        }

        if(strcmp(buffer,"bg")==0){
            continueJob();
            continue;
        }
        else if(strcmp(buffer,"fg")==0){
            int pgid=continueJob();
            if(pgid==-1){
                continue;//if no job found, ignore fg command
            }
            //give terminal control, wait on it
            tcsetpgrp(0,pgid);
            waitpid(-pgid,NULL,WUNTRACED);
            continue;
        }
        else if(strcmp(buffer,"jobs")==0){
            printJobs();
            continue;
        }
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
                //sleep to ensure that parent can store its information
                struct timespec ts;
                ts.tv_sec=0;
                ts.tv_nsec=1000000;
                nanosleep(&ts,NULL);
                //do file redirection
                if(fileRedirect(cmd)==-1){
                    exit(0);
                }
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                setpgid(0,0); //set current process to a new pgid
                int ret=execvp(cmd->cmd,cmd->arg);
                if(ret==-1){
                    exit(0);
                }
            }
            int status=0;
            if(cmd->isBackground==0){//if frontground cmd, wait for it
                waitpid(cpid,&status,WUNTRACED);//should wait for signal
            }
            else{//else add it as a background job
                addJob(buffer,cpid,Running);
            }
            if(WIFSTOPPED(status)){//if stopped by signal, put it into joblist
                addJob(buffer,cpid,Stopped);
            }
            // kill(cpid,SIGCONT);
            // printf("child continues");
            deleteCmd(cmd);
        }
        else{
            //for two commands, build pipe before exec two single command
            command* cmd1=parse(tokenList,0,pipeIdx);
            command* cmd2=parse(tokenList,pipeIdx+1,tokenCnt);
            int pfd[2];
            if(pipe(pfd)==-1){
                continue;
            }
            int cpid1=fork();
            if(cpid1==0){//left child, write to pipe
                //sleep to ensure that right sibling can find the group
                struct timespec ts;
                ts.tv_sec=0;
                ts.tv_nsec=1000000;
                nanosleep(&ts,NULL);
                if(fileRedirect(cmd1)==-1){
                    exit(0);
                }
                setpgid(0,0); //set current process to a new pgid
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                dup2(pfd[1],1);
                close(pfd[0]);
                int ret=execvp(cmd1->cmd,cmd1->arg);
                if(ret==-1){
                     exit(0);
                }
            }
            int cpid2=fork();
            if(cpid2==0){//right child, read from pipe
                if(fileRedirect(cmd2)==-1){
                    exit(0);
                }
                setpgid(0,cpid1);//set pgid to cpid1
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                dup2(pfd[0],0);
                close(pfd[1]);
                int ret=execvp(cmd2->cmd,cmd2->arg);
                if(ret==-1){
                    exit(0);
                }
            }
            close(pfd[0]);
            close(pfd[1]);
            if(cmd2->isBackground==0){
                waitpid(cpid2,NULL,WUNTRACED);//should wait for signal
            }
            deleteCmd(cmd1);
            deleteCmd(cmd2);
        }
        
        free(buffer);
        buffer=NULL;
        tokenCnt=0;
    }
}