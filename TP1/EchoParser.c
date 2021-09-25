
int echoParser(char * buffer, int * valread)
{   
    int i = 0;
    int counter = 0;
    int limit= 0;
    int notFinished = 1;
    int isCorrect = 1;
    while (notFinished)
    {

        if(isValid(*(buffer + i)) && limit <= 100)
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
            }
           
        }
        else 
        {
            isCorrect = 0;
            while (*(buffer + i) != '\r' && *(buffer + i + 1) != '\n' )
            {
                i++;
            }
            *(buffer + counter) = *(buffer + i);
            counter++;
            *(buffer + counter) == *(buffer + i + 1);
            counter++;
            limit = 0;
            i++;
        
        }
        
        if(i == *valread)
        {
            *valread = counter;
            notFinished = 0;
        }
        i++;
    }

    return isCorrect;
}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         

int isValid(char c)
{
    if (c == 'x')
    {
        return 0;
    }
    return 1;
    
}