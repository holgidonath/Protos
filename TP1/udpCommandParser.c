#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "include/udpCommandParser.h"

int udpParseCommand(char * buffer){
    printf("entro\n");
    printf("%s", buffer);
    
    unsigned state = BEGIN;
    int i = 0;
    char c = tolower(buffer[0]);

    while(state != FINISH && state != INVALID && state != STATS_OK && state != LOCALE_EN_OK && state != LOCALE_ES_OK){
        switch (state){
            case BEGIN:
                if(c == 's'){
                    state = S;
                } else {
                    state = INVALID;
                }
                break;
            case S:
                if (c == 'e')
                {
                    state = SE;
                }else if (c == 't'){
                    state = ST;
                } else {
                    state = INVALID;
                }
                break;
            case SE:
                if (c == 't'){
                    state = SET;
                } else {
                    state = INVALID;
                }
                break;
            case SET:
                if (c == ' '){
                    state = SETSPACE;
                } else {
                    state = INVALID;
                }
                break;
            case SETSPACE:
            	if (c == 'l'){
                    state = L;
                } else {
                    state = INVALID;
                }
                break;
            case L:
                if (c == 'o'){
                    state = LO;
                } else {
                    state = INVALID;
                }
                break;
            case LO:
                if (c == 'c'){
                    state = LOC;
                } else {
                    state = INVALID;
                }
                break;
            case LOC:
                if (c == 'a'){
                    state = LOCA;
                } else {
                    state = INVALID;
                }
                break;
            case LOCA:
                if (c == 'l'){
                    state = LOCAL;
                } else {
                    state = INVALID;
                }
                break;
            case LOCAL:
                if (c == 'e'){
                    state = LOCALE;
                } else { 
                    state = INVALID;
                }
                break;
            case LOCALE:
                if (c == ' '){
                    state = LOCALESPACE;
                } else {
                    state = INVALID;
                }
                break;
            case LOCALESPACE:
            	if (c == 'e'){
                    state = E;
                } else {
                    state = INVALID;
                }
                break;
            case E:
                if (c == 'n'){
                    state = EN;
                } else if (c == 's'){
                    state = ES;
                } else {
                    state = INVALID;
                }
                break;
            case EN:
            	if(c == '\r'){
                	state = LOCALE_EN_OK;
                	printf("pediste locale\n");
                } else {
                	state = INVALID;
                }
                break;
            case ES:
            	if(c == '\r'){
                	state = LOCALE_ES_OK;
                	printf("pediste locale\n");
                } else {
                	state = INVALID;
                }
                break;
            case ST:
                if (c == 'a'){
                    state = STA;
                } else {
                    state = INVALID;
                }
                break;
            case STA:
                if (c == 't'){
                    state = STAT;
                } else {
                    state = INVALID;
                }
                break;
            case STAT:
                if (c == 's'){
                    state = STATS;
                } else {
                    state = INVALID;
                }
                break;
            case STATS:
            	if(c == '\r'){
                	state = STATS_OK;
                	printf("pediste stats");
                } else {
                    state = INVALID;
                }
                break;
            case RETURN:
                if (c == 'n'){
                    state = FINISH;
                } else {
                    state = INVALID;
                }
            case FINISH:
                break;
            case STATS_OK:
                break;
            case LOCALE_ES_OK:
                break;  
            case LOCALE_EN_OK:
                break;    
        }
        i++;
        c = tolower(buffer[i]);
    }
    return state;
}

// int main(int argc, char *argv[]) {
//     char buffer[100];
//     strcat(buffer, argv[1]);
//     for (int i = 2; i < argc; i++){
//         strcat(buffer," ");
//         printf("%s", argv[i]);
//         strcat(buffer,argv[i]);
//         printf("\n");
//     }
//     strcat(buffer,"\n");
//     printf("%s", buffer);
//     parseCommand(buffer);
// }
