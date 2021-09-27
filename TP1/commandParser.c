#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "include/commandParser.h"
#include "include/echoParser.h"


int parseCommand(char * buffer, int * commandParsed, int * valread, int * wasValid, int * limit){
    unsigned state = *commandParsed;
    int i = 0;
    char c = tolower(buffer[0]);
    char test[] = "hola";
    
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
                strcpy(buffer, buffer + i + 1);
				*valread -= (i+1);
                int correct = echoParser(buffer,valread, wasValid, limit, commandParsed);
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
            if (c == ' '){
                state = FINISH;
                *commandParsed = GET;
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