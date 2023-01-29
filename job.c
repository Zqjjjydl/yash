#include "job.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void deleteJob(job* j){
    free(j->jobString);
    free(j);
}
job* newJob(int jobId, status s, char* jobString, int pgid){
    job* ret=(job*)malloc(sizeof(job));
    ret->jobId=jobId;
    ret->s=s;
    ret->jobString=(char*)malloc(2010*sizeof(char));
    strncpy(ret->jobString,jobString,strlen(jobString)+1);
    ret->pgid=pgid;
    ret->next=NULL;
    return ret;
}
void printJob(job* j){
    printf("[%d]%s\n",j->jobId,j->jobString);
}