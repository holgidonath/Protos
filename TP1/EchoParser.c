#include "include/commandParser.h"

int isValid(char c)
{
    if (c == 'x')
    {
        return 0;
    }
    return 1;
    
}

int echoParser(char * buffer, long int * valread, int * wasValid, int * prev_limit, int * commandParsed, struct buffer * buf )
{   
    int i = 0;
    long int counter = 0;
    int limit= *prev_limit;
    int notFinished = 1;
    char auxbuf[BUFFSIZE+1] = {0};
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
                
            }
            
           
        }
        else 
        {
            while (*(buffer + i) != '\r' && *(buffer + i + 1) != '\n' )
            {
                i++;
            }
            auxbuf[counter++] = *(buffer+i);
            auxbuf[counter++] = *(buffer + i + 1);
            *commandParsed = BEGIN;
            // *(buffer + counter) = *(buffer + i);
            // counter++;
            // *(buffer + counter) = *(buffer + i + 1);
            // counter++;
            // limit = 0;
            // i++;
            *wasValid = 1;
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

