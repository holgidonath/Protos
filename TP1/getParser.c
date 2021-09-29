#include <stdio.h>
#include <time.h>
#include <string.h>
#include "include/getParser.h"

#define BUF_LEN 1024

int getParser(char * buffer, long int * valread, int * wasValid, int * prev_limit, int * commandParsed, int locale){
    long int counter = 0;
    int limit= *prev_limit;
    int notFinished = 1;
    int isCorrect = 1;
    unsigned state = BEGIN_GET;
    int i = 0;
    char c = tolower(buffer[0]);
    unsigned cmd = BEGIN_GET;
    while(state != FINISH_GET && state != INVALID_GET){
        switch (state)
        {
        case BEGIN_GET:
            if(c == 'd'){
                state = D;
            } else if (c == 't'){
                state = T;
            } else {
                state = INVALID_GET;
            }
            break;
        case D:
            if(c == 'a'){
                state = DA;
            } else {
                state = INVALID_GET;
            }
            break;
        case DA:
            if(c == 't'){
                state = DAT;
            } else {
                state = INVALID_GET;
            }
            break;
        case DAT:
            if(c == 'e'){
                state = DATE;
            } else {
                state = INVALID_GET;
            }
            break;   
        case DATE:
            if(c == '\r'){
                cmd = DATE;
                state = CR;
            } else {
                state = INVALID_CRLF;
            }
            break; 
        case T:
            if(c == 'i'){
                state = TI;
            } else {
                state = INVALID_GET;
            }
            break;
        case TI:
            if(c == 'm'){
                state = TIM;
            } else {
                state = INVALID_GET;
            }
            break;
        case TIM:
            if(c == 'e'){
                state = TIME;
            } else {
                state = INVALID_GET;
            }
            break;   
        case TIME:
            if(c == '\r'){
                cmd = TIME;
                state = CR;
            } else {
                state = INVALID_CRLF;
            }
            break; 
        case CR:
            if(c == '\n'){
                state = FINISH_GET;
            } else {
                state = INVALID_CRLF;
            }
            break; 
        case INVALID_GET:
            isCorrect = 0;
            break;
        case INVALID_CRLF:
            isCorrect = 0;
            break;
        case FINISH_GET:
            break;
        }
        c = tolower(buffer[++i]);
        buffer[i-1] = '\0';
    }
    *commandParsed = BEGIN_GET;
    if(state == FINISH_GET){
        time_t rawtime = time(NULL);
        struct tm *ptm = localtime(&rawtime);
        char buf [BUF_LEN] = {0};
        if(cmd == DATE && state == FINISH_GET){
        	if(locale == 'es'){
            		strftime(buf, BUF_LEN, "%d/%m/%Y\r\n", ptm);
            	} else{
            		strftime(buf, BUF_LEN, "%m/%d/%Y\r\n", ptm);
            	}
        } else if (cmd == TIME && state == FINISH_GET){
            strftime(buf, BUF_LEN, "%T\r\n", ptm);
        }
        strcpy(buffer, buf);
        *valread = strlen(buf);
    } else if (state == INVALID_CRLF || state == INVALID_GET){
        isCorrect = 0;
    }
    return isCorrect;
    
}
