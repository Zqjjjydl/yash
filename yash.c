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
#include <sys/prctl.h>
#include <errno.h>


job* dummyJobListHead;
int nextJobId=0;
void checkChild(int sig){
    // check each child
    // to remove all terminated process and print their information 
    // to change all stopped process status
    int status=0;
    job* curP=dummyJobListHead;
    while(curP&&curP->next){
        int ret=waitpid(-curP->next->pgid,&status,WNOHANG|WUNTRACED);
        if(ret==0||ret==-1){
            curP=curP->next;
            continue;
        }
        if(WIFSTOPPED(status)){
            curP->next->s=Stopped;
        }
        else if(WIFEXITED(status)){
            curP->next->s=Done;
            printJob(curP->next);
            //delete curP->next
            job* temp=curP->next;
            curP->next=curP->next->next;
            deleteJob(temp);
        }
        else if(WIFSIGNALED(status) == 0){
            //delete curP->next
            job* temp=curP->next;
            curP->next=curP->next->next;
            deleteJob(temp);
        }
        curP=curP->next;
    }
    if(dummyJobListHead->next){
        dummyJobListHead->next->isMostRecentJob=1;
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

int continueJob(int isBg){
    //called by yash, continue a job, return the pgid or -1 if failed
    job* cur=dummyJobListHead;
    while(cur&&cur->next){
        if((cur->next->s)==Stopped&&isBg==1){//bg only run stopped process
            kill(-cur->next->pgid,SIGCONT);
            cur->next->s=Running;
            return cur->next->pgid;
        }
        if(isBg==0){//fg run any process
            kill(-cur->next->pgid,SIGCONT);
            cur->next->s=Running;
            return cur->next->pgid;
        }
        cur=cur->next;
    }
    return 1;
}

void addJob(char* jobString,int pgid,status s){
    job* temp=newJob(nextJobId++, s, jobString, pgid,1);
    if(dummyJobListHead->next){
        dummyJobListHead->next->isMostRecentJob=0;
    }
    temp->next=dummyJobListHead->next;
    dummyJobListHead->next=temp;
}

void printJobs(){
    //this function print all jobs in the job list
    //note that we store job in a fifo style therefore
    //we need to reverse the order when printing
    int jobCnt=0;
    job* cur=dummyJobListHead;
    while(cur->next){
        jobCnt++;
        cur=cur->next;
    }
    for(int i=jobCnt;i>0;i--){
        cur=dummyJobListHead;
        int temp=i-1;
        while(temp--){
            cur=cur->next;
        }
        printJob(cur->next);
    }
}

void updateJob(int pgid,status s){
    job* cur=dummyJobListHead;
    while(cur&&cur->next){
        if((cur->next->pgid)==pgid){
            cur->next->s=s;
            return;
        }
        cur=cur->next;
    }
}

void removeJob(int pgid){
    job* cur=dummyJobListHead;
    while(cur&&cur->next){
        if((cur->next->pgid)==pgid){
            job* temp=cur->next;
            cur->next=cur->next->next;
            deleteJob(temp);
        }
        cur=cur->next;
    }
}

void emptyHandler(int a){
    return;
}

int main(){
    char* buffer=NULL;
    char originalBuffer[2010];
    char* tokenList[2010];
    int tokenCnt=0;
    //yash should ignore sigtstp
    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    dummyJobListHead=newJob(nextJobId++,Running,"",0,0);

    signal(SIGCHLD, emptyHandler);//when receiving child stop or ter
    while(1){
        checkChild(0);
        buffer=readline("# ");
        if(buffer){
            strncpy(originalBuffer,buffer,strlen(buffer)+1);
        }
        if(buffer==NULL){//EOF
            exit(0);
        }
        if(strlen(buffer)==0){//empty
            continue;
        }

        if(strcmp(buffer,"bg")==0){
            continueJob(1);
            continue;
        }
        else if(strcmp(buffer,"fg")==0){
            int pgid=continueJob(0);
            if(pgid==-1){
                continue;//if no job found, ignore fg command
            }
            //give terminal control, wait on it, get back terminal control
            tcsetpgrp(0,pgid);
            int status=0;
            waitpid(-pgid,&status,WUNTRACED);
            if(WIFSTOPPED(status)){
                updateJob(pgid,Stopped);
            }
            else if(WIFSIGNALED(status)){
                removeJob(pgid);
            }
            tcsetpgrp(0, getpgid(0));
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
            int ppid_before_fork = getpid();
            int cpid=fork();
            
            if(cpid==0){
                //child
                int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
                if (r == -1) { perror(0); exit(1); }
                if (getppid() != ppid_before_fork)
                    exit(1);
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
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
            
                int ret=execvp(cmd->cmd,cmd->arg);
                if(ret==-1){
                    exit(0);
                }
            }
            else{
                setpgid(cpid,cpid); //set current process to a new pgid
                tcsetpgrp(0,getpgid(cpid));
            }
            int status=0;
            if(cmd->isBackground==0){//if frontground cmd, wait for it
                waitpid(-getpgid(cpid),&status,WUNTRACED);//should wait for signal
            }
            else{//else add it as a background job
                addJob(originalBuffer,getpgid(cpid),Running);
            }
            if(WIFSTOPPED(status)){//if stopped by signal, put it into joblist
                addJob(originalBuffer,getpgid(cpid),Stopped);
            }
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
            int ppid_before_fork = getpid();
            int cpid1=fork();
            if(cpid1==0){//left child, write to pipe
                int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
                if (r == -1) { perror(0); exit(1); }
                if (getppid() != ppid_before_fork)
                    exit(1);
                //sleep to ensure that right sibling can find the group
                struct timespec ts;
                ts.tv_sec=0;
                ts.tv_nsec=1000000;
                nanosleep(&ts,NULL);
                if(fileRedirect(cmd1)==-1){
                    exit(0);
                }
                // setpgid(0,0); //set current process to a new pgid
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                dup2(pfd[1],1);
                close(pfd[0]);
                int ret=execvp(cmd1->cmd,cmd1->arg);
                if(ret==-1){
                     exit(0);
                }
            }
            int cpid2=fork();
            if(cpid2==0){//right child, read from pipe
                int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
                if (r == -1) { perror(0); exit(1); }
                if (getppid() != ppid_before_fork)
                    exit(1);
                //sleep to ensure that right sibling can find the group
                struct timespec ts;
                ts.tv_sec=0;
                ts.tv_nsec=1000000;
                nanosleep(&ts,NULL);
                if(fileRedirect(cmd2)==-1){
                    exit(0);
                }
                // setpgid(0,cpid1);//set pgid to cpid1
                signal(SIGTSTP, SIG_DFL);
                signal(SIGINT, SIG_DFL);
                signal(SIGTTIN, SIG_IGN);
                signal(SIGTTOU, SIG_IGN);
                dup2(pfd[0],0);
                close(pfd[1]);
                int ret=execvp(cmd2->cmd,cmd2->arg);
                if(ret==-1){
                    exit(0);
                }
            }
            if(cpid1&&cpid2){
                setpgid(cpid1,cpid1); //set left child to a new pgid
                setpgid(cpid2,getpgid(cpid1)); //set right child to a new pgid same as left child
                tcsetpgrp(0,getpgid(cpid1));
            }
            close(pfd[0]);
            close(pfd[1]);

            int status=0;

            if(cmd2->isBackground==0){
                waitpid(-getpgid(cpid1),&status,WUNTRACED);//should wait for signal
                waitpid(cpid2,NULL,WUNTRACED);//should wait for signal
            }
            else{//else add it as a background job
                addJob(originalBuffer,getpgid(cpid1),Running);
            }
            if(WIFSTOPPED(status)){//if stopped by signal, put it into joblist
                addJob(originalBuffer,getpgid(cpid1),Stopped);
            }
            deleteCmd(cmd1);
            deleteCmd(cmd2);
        }
        tcsetpgrp(0, getpgid(0));
        free(buffer);
        buffer=NULL;
        tokenCnt=0;
    }
}