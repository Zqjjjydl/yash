#ifndef JOB_H
#define JOB_H
#include "command.h"
typedef enum{Running, Stopped, Done} status;
typedef struct job{
    int jobId;
    status s;
    char* jobString;
    int pgid;
    int isMostRecentJob;
    struct job* next;
} job;
void deleteJob(job* j);
job* newJob(int jobId, status s, char* jobString, int pgid,int isMostRecentJob);
void printJob(job* j);
#endif