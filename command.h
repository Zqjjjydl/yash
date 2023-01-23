#ifndef COMMAND_H
#define COMMAND_H
typedef struct{
    char* cmd;//cmd name
    char** arg;//arguments,end with NULL
    char* inputFile;//input redirection file name, NULL if none
    char* outputFile;//output redirection file name, NULL if none
    char* errorOutputFile;//error msg output redirection file name, NULL if none
} command;
void deleteCmd(command* cmd);
#endif