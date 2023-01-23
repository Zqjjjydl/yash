#include "command.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
command* parse(char** tokenList, int begin, int end){
    //parse argument into a command
    command* ret=(command*)malloc(sizeof(command));
    ret->inputFile=NULL;
    ret->outputFile=NULL;
    ret->errorOutputFile=NULL;
    ret->cmd=tokenList[begin];
    ret->arg=(char**)malloc(2010*sizeof(char*));
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