#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>


struct sth
{
    double num;
    int ref;
    char empty[112];
};

void m(int a, int b)
{
    int i;
    struct sth *p = malloc((a+b)*sizeof(struct sth));
    for (i = 0; i < a + b; i++ )
    {
        p[i].ref = rand() % 2;
        if (i % 3 == 0)
        {
            p[i].num= (double)(rand() / a + rand() / b);
        }
        else if (i % 3 == 1)
        {
            p[i].num = (double)(rand() % 31237 / 3);
        }
        else
        {
            p[i].num = (double)((rand() % a * rand() % b) / (a + b)) ;
        }
    }
    for (i = 0; i < a + b; i++)
    {
        if (p[i].ref)
        {
            p[i].num = p[i].num + a / rand();
        }
        else
        {
            if (i % 2 < 1)
            {
                p[i].num = (double)(p[i].num / p[a+b-i].num);
            }
            else
            {
                p[i].num = (double)((p[a+b-i].num + p[rand()%i].num) / p[i].num);
            }
        }
    }
    
}


int main(int argc, char ** argv) {
    /* Markers used to bound trace regions of interest */
    volatile char MARKER_START, MARKER_END;
    /* Record marker addresses */
    FILE* marker_fp = fopen("mine.marker","w");
    if(marker_fp == NULL ) {
        perror("Couldn't open marker file:");
        exit(1);
    }
    fprintf(marker_fp, "%p %p", &MARKER_START, &MARKER_END );
    fclose(marker_fp);
    
    MARKER_START = 33;
    m(66666,23333);
    MARKER_END = 34;
    
    return 0;
}
