
int echoParser(char * buffer, int * valread)
{   
    int i = 0;
    int counter = 0;
    int notFinished = 1;
    int isCorrect = 1;
    while (notFinished)
    {

        if(isValid(buffer + i) && counter <= 100)
        {
            *(buffer + counter) = *(buffer + i);
            counter++;
        }
        else 
        {
            isCorrect = 0;
            *(buffer + counter) = '/r';
            counter++;
            *(buffer + counter) = '/n';
            counter++;
            *valread = counter;
            notFinished = 0;
        }

        if( *(buffer + i) == '/r' && *(buffer + i + 1) == '/n')
        {
             *(buffer + counter + 1) == *(buffer + i + 1);
             counter++;
             *valread = counter;
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