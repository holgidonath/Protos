#include "include/commandParser.h"
#include <ctype.h>
#include <string.h>

int isValid(char c)
{
    if (!isascii(c))
    {
        return 0;
    }
    return 1;
    
}

int echoParser(char * buffer, long int * valread, int * wasValid, int * prev_limit, int * commandParsed, struct buffer * buf, int * correct_lines, int * incorrect_lines )
{   
    int i = 0;
    long int counter = 0;
    int limit= *prev_limit;
    int notFinished = 1;
    char auxbuf[BUFFSIZE+1] = {0};
    int flag = 0;
    while (notFinished)
    {
        
        if((isValid(*(buffer + i)) && limit < 100 )&& *wasValid)
        {
            auxbuf[counter++] = *(buffer + i);
            limit++;
            if(*(buffer + i) == '\r' && *(buffer + i + 1) == '\n')
            {   
                auxbuf[counter++] = *(buffer + i + 1);
                limit = 0;
                i++;
                *commandParsed = BEGIN;
                notFinished = 0;
                *wasValid = 1;
                *correct_lines += 1;
                
            }
            
           
        }
        else 
        {
            if(*wasValid){
                auxbuf[counter++] = '\r';
                auxbuf[counter++] = '\n';
            }
            while (*(buffer + i) != '\r' && *(buffer + i + 1) != '\n' )
            {
                i++;
                if (i+5 >= BUFFSIZE){
                    *wasValid = 0;
                    flag = 1;
                    break;
                }
            }
            if(flag != 1){
                *wasValid = 1;
                *incorrect_lines += 1;
            }
            
            *commandParsed = BEGIN;
            // *(buffer + counter) = *(buffer + i);
            // counter++;
            // *(buffer + counter) = *(buffer + i + 1);
            // counter++;
            // i++;
            limit = 0;
            // *wasValid = 1;
            notFinished = 0;
           
        }
        i++;
        
        
    }
    *prev_limit = limit;
    buf->buffer = realloc(buf->buffer, buf->len + counter);
    memcpy(buf->buffer + buf->len, auxbuf, counter);
    buf->len += counter;
    return counter;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         

