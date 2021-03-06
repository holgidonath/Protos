#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "include/commandParser.h"
#include "include/echoParser.h"
#include "include/getParser.h"


int parseCommand(char * buffer, int * commandParsed, long * valread, int * wasValid, int * limit, struct buffer * buf, char *locale, int * correct_lines, int * incorrect_lines){
    unsigned state = *commandParsed;
    int i = 0;
    char c = tolower(buffer[0]);

    if(*wasValid == 0){
        echoParser(buffer,valread,wasValid,limit,commandParsed,buf, correct_lines, incorrect_lines);
        return state;
    }
    
    while(state != FINISH && state != INVALID){
        switch (state)
        {
        case BEGIN:
            if(c == 'e'){
                state = E;
            } else if (c == 'g'){
                state = G;
            } else {
                state = INVALID;
            }
            break;
        case E:
            if(c == 'c'){
                state = EC;
            } else {
                state = INVALID;
            }
            break;
        case EC:
            if(c == 'h'){
                state = ECH;
            } else {
                state = INVALID;
            }
            break;
        case ECH:
            if (c == 'o'){
                state = ECHO;
            } else {
                state = INVALID;
            }
            break;
        case ECHO:
            if (c == ' ' || *commandParsed == ECHO){
                state = FINISH;
                *commandParsed = ECHO;
                // strcpy(buffer, buffer + i + 1);
				// *valread -= (i+1);
                echoParser(buffer + i + 1,valread, wasValid, limit, commandParsed, buf, correct_lines, incorrect_lines);
            } else {
                state = INVALID;
            }
            break;
        case G:
            if(c == 'e'){
                state = GE;
            } else {
                state = INVALID;
            }
            break;
        case GE:
            if(c == 't'){
                state = GET;
            } else {
                state = INVALID;
            }
            break;
        case GET:
            if (c == ' ' || *commandParsed == GET){
                state = FINISH;
                *commandParsed = GET;

                // strcpy(buffer, buffer + i + 1);
				// *valread -= (i+1);
                int correct = getParser(buffer+i+1,valread, wasValid, limit, commandParsed, buf, locale);
                if(!correct){
                    state = INVALID;
                    *incorrect_lines += 1;
                }else{
                    *correct_lines +=1;
                }
            } else {
                state = INVALID;
            }
            break;
        case INVALID:
        case FINISH:
            break;
        default:
            break;
        }
        i++;
        c = tolower(buffer[i]);

    }
    return state;
}
