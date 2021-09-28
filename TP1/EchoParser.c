#include "include/commandParser.h"

int isValid(char c)
{
    if (c == 'x')
    {
        return 0;
    }
    return 1;
    
}

int echoParser(char * buffer, long int * valread, int * wasValid, int * prev_limit, int * commandParsed)
{   
    int i = 0;
    long int counter = 0;
    int limit= *prev_limit;
    int notFinished = 1;
    int isCorrect = 1;
    while (notFinished)
    {

        if(isValid(*(buffer + i)) && limit <= 100 && *wasValid)
        {
            *(buffer + counter) = *(buffer + i);
            counter++;
            limit++;
            if(*(buffer + i) == '\r' && *(buffer + i + 1) == '\n')
            {   
                *(buffer + counter) = *(buffer + i + 1);
                counter++;
                limit = 0;
                i++;
                *commandParsed = BEGIN;
                
                
            }
           
        }
        else 
        {
            isCorrect = 0;
            int j = i + 1;
            while (*(buffer + i) != '\r' && *(buffer + j) != '\n' )
            {

                *(buffer + i) = *(buffer + (j++));
                *(buffer + (j-1)) = '\0';
                if(*(buffer + i) == '\r'){
                    i++;
                }
            }
            counter += 2;
            *commandParsed = BEGIN;
            // *(buffer + counter) = *(buffer + i);
            // counter++;
            // *(buffer + counter) = *(buffer + i + 1);
            // counter++;
            // limit = 0;
            // i++;
        
        }
        i++;
        if(i == *valread)
        {
            *valread = counter;
            *wasValid = 1;
            *prev_limit = limit;
            notFinished = 0;
        }
        
    }

    return isCorrect;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         

