#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "include/udpCommandParser.h"

int udpParseCommand(char * buffer){
    
    unsigned state = UDP_BEGIN;
    int i = 0;
    char c = tolower(buffer[0]);

    while(state != UDP_FINISH && state != UDP_INVALID && state != STATS_OK && state != LOCALE_EN_OK && state != LOCALE_ES_OK){
        switch (state){
            case UDP_BEGIN:
                if(c == 's'){
                    state = S;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case S:
                if (c == 'e')
                {
                    state = SE;
                }else if (c == 't'){
                    state = ST;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case SE:
                if (c == 't'){
                    state = SET;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case SET:
                if (c == ' '){
                    state = SETSPACE;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case SETSPACE:
            	if (c == 'l'){
                    state = L;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case L:
                if (c == 'o'){
                    state = LO;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case LO:
                if (c == 'c'){
                    state = LOC;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case LOC:
                if (c == 'a'){
                    state = LOCA;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case LOCA:
                if (c == 'l'){
                    state = LOCAL;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case LOCAL:
                if (c == 'e'){
                    state = LOCALE;
                } else { 
                    state = UDP_INVALID;
                }
                break;
            case LOCALE:
                if (c == ' '){
                    state = LOCALESPACE;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case LOCALESPACE:
            	if (c == 'e'){
                    state = UDP_E;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case UDP_E:
                if (c == 'n'){
                    state = EN;
                } else if (c == 's'){
                    state = ES;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case EN:
                // printf("pediste locale\n"); 
            	if(c == '\0'){
                	state = LOCALE_EN_OK;
                	// printf("pediste locale\n");
                } else {
                	state = UDP_INVALID;
                }
                break;
            case ES:
                state = LOCALE_ES_OK;
            	if(c == '\0'){
                	state = LOCALE_ES_OK;
                } else {
                	state = UDP_INVALID;
                }
                break;
            case ST:
                if (c == 'a'){
                    state = STA;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case STA:
                if (c == 't'){
                    state = STAT;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case STAT:
                if (c == 's'){
                    state = STATS;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case STATS:
            	if(c == '\0'){
                	state = STATS_OK;
                } else {
                    state = UDP_INVALID;
                }
                break;
            case RETURN:
                if (c == 'n'){
                    state = UDP_FINISH;
                } else {
                    state = UDP_INVALID;
                }
            case UDP_FINISH:
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
