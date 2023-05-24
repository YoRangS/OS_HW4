#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

int main(int argc, char* argv[])
{
    // Check invalid inputs
    int thread_num = 16;
    int minimum_size = 1024;
    char* file_name = NULL;
    char* dir_path = NULL;

    for (int i = 1; i < argc - 1; i++) {
        char* val = NULL;
        val = strdup(argv[i]+3);
        if (argv[i][1] == 't' || argv[i][1] == 'm') {
            char* endPtr;
            long int tmp = strtol(val, &endPtr, 10);
            if (*endPtr != '\0') {
                printf("Invalid inputs (-%c=%s)\n", argv[i][1], val);
                return 0;
            } else {
                if    (argv[i][1] == 't')  thread_num = tmp;
                else /*argv[i][1] == 'm'*/ minimum_size = tmp;
            }
        } else if (argv[i][1] == 'o') {
            file_name = strdup(val);
        } else {
            printf("Invalid inputs: There is no such option\n");
            return 0;
        }
    }
    dir_path = strdup(argv[argc - 1]);

    /*----------------debug------------------*/
    printf("thread_num : %d\n", thread_num);
    printf("minimum_size : %d\n", minimum_size);
    printf("file_name : %s\n", file_name);
    printf("dir_path : %s\n", dir_path);
    /*---------------------------------------*/

    // Initiate thread and etc


    // Run find function


    // Finish program (free, ...)

    return 0;
}