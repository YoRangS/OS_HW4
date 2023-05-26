#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>

typedef struct _fileList {
    long size;
    char* path;
    struct _fileList * next;
} fileList;

fileList fl_head = {0, 0x0, 0x0};
fileList * fl_last = 0x0;

// SIGINT Signal (when user presses CTRL+C)
void handleSIGINT(int sig)
{
    printf("\nSIGINT signal received. Finishing program...\n");
    // Perform any necessary cleanup operations here
    exit(0);
}

void scanDir(const char *path, int minimum_size) {
    printf("Enter scanDir\n");
    DIR *dir;
    struct dirent *entry;
    struct stat info;

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    } 

    while ((entry = readdir(dir)) != NULL) {
        char filePath[PATH_MAX];
        snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

        if (stat(filePath, &info) != 0) {
            perror("stat");
            continue;
        }

        if (S_ISDIR(info.st_mode)) {
          // If it is dir, do recursion
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                scanDir(filePath, minimum_size);
            }
        } else if (S_ISREG(info.st_mode)) {
            // If it is file, do a task
            //check file size vs minimum size
            FILE *file;
            file = fopen(filePath, "rb");
            if (file == NULL) printf("file open error\n");
            fseek(file, 0, SEEK_END);
            long fileSize = ftell(file);

            if (fileSize >= minimum_size) {  // Files smaller than -m, non-regular files! are filtered here
                fileList * curr_file = (fileList *)malloc(sizeof(fileList));
                if (fl_last == 0x0) {       // Also fl_head.next == 0x0
                    printf("fl_last == 0x0\n");
                    fl_head.next = curr_file;
                    fl_last->next = curr_file;
                }
                else {
                    printf("fl_last != 0x0\n");
                    fl_last->next = curr_file;
                    fl_last = curr_file;
                }
                curr_file->size = fileSize;
                strcpy(curr_file->path, filePath);
                curr_file->next = 0x0;
            }

            printf("fileSize: %ld / filePath: %s\n", fileSize, filePath);
            printf("%p\t\t%p\n", fl_head.next, fl_last);
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[])
{
    // Check invalid inputs
    int thread_num = 16;
    int minimum_size = 1024;
    char* file_name = NULL;
    char* dir_path = NULL;

    // Register signal handler for SIGINT
    signal(SIGINT, handleSIGINT);

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
    const char *directoryPath = dir_path;
    scanDir(directoryPath, minimum_size);

    fileList * itr;
    int i;
    for (itr = fl_head.next, i=0; itr != 0x0; itr = itr->next, i++) {
        printf("[%d] %s (Size: %ld)\n", i, itr->path, itr->size);
    }

    // Finish program (free, ...)

    return 0;
}