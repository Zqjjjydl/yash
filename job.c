#include "job.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void deleteJob(job* j){
    free(j->jobString);
    free(j);
}
job* newJob(int jobId, status s, char* jobString, int pgid,int isMostRecentJob){
    job* ret=(job*)malloc(sizeof(job));
    ret->jobId=jobId;
    ret->s=s;
    ret->jobString=(char*)malloc(2010*sizeof(char));
    strncpy(ret->jobString,jobString,strlen(jobString)+1);
    ret->pgid=pgid;
    ret->isMostRecentJob=isMostRecentJob;
    ret->next=NULL;
    return ret;
}
void printJob(job* j){
    char* statusStr=NULL;
    char* plusOrMinus=NULL;
    switch (j->s)
    {
    case Running:
        statusStr="Running";
        break;
    case Stopped:
        statusStr="Stopped";
        break;
    case Done:
        statusStr="Done";
        break;
    default:
        break;
    }
    if(j->isMostRecentJob){
        plusOrMinus="+";
    }
    else{
        plusOrMinus="-";
    }
    printf("[%d]%s %s %s\n",j->jobId,plusOrMinus,statusStr,j->jobString);
}