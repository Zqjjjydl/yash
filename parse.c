#include "command.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "job.h"

command* parse(char** tokenList, int begin, int end){
    //parse argument into a command
    command* ret=newCmd();
    ret->cmd=tokenList[begin];
    int argCnt=0;
    for(int i=begin;i<end;i++){
        if(strcmp(tokenList[i],"<")==0){
            ret->inputFile=tokenList[++i];
        }
        else if(strcmp(tokenList[i],">")==0){
            ret->outputFile=tokenList[++i];
        }
        else if(strcmp(tokenList[i],"2>")==0){
            ret->errorOutputFile=tokenList[++i];
        }
        else if((i==(end-1))&&(strcmp(tokenList[i],"&")==0)){
            ret->isBackground=1;
        }
        else{
            ret->arg[argCnt++]=tokenList[i];
        }
    }
    ret->arg[argCnt]=NULL;
    return ret;
}

void deleteCmd(command* cmd){
    free(cmd->arg);
    free(cmd);
}
command* newCmd(){
    command* ret=(command*)malloc(sizeof(command));
    ret->cmd=NULL;
    ret->arg=(char**)malloc(2010*sizeof(char*));
    ret->inputFile=NULL;
    ret->outputFile=NULL;
    ret->errorOutputFile=NULL;
    ret->isBackground=0;
}