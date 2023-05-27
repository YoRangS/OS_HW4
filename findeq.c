#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 1024

typedef struct _fileList {
    long size;
    char* path;
    struct _fileList * next;
} fileList;

fileList fl_head = {0, 0x0, 0x0};
fileList * fl_last = 0x0;

typedef struct Data {
    char* path;
    struct Data* next;
} Data;

Data data_head = {0x0, 0x0};

// SIGINT Signal (when user presses CTRL+C)
void handleSIGINT(int sig)
{
    printf("\nSIGINT signal received. Finishing program...\n");
    // Perform any necessary cleanup operations here
    exit(0);
}

void appendFL(fileList** head, long num, char* str) {
    fileList* new_file = (fileList *)malloc(sizeof(fileList));
    new_file->path = (char *)malloc((strlen(str)+1) * sizeof(char));
    new_file->size = num;
    strcpy(new_file->path, str);
    new_file->next = 0x0;

    if(*head == 0x0) {
        *head = new_file;
        return;
    }

    fileList* curr_file = *head;
    while (curr_file->next != NULL) {
        curr_file = curr_file->next;
    }
    curr_file->next = new_file;
}

void freeFL(fileList** head) {
    fileList * itr = 0x0;
    if (*head == fl_head.next) {
        itr = &fl_head;
    }
    else {
        itr = fl_head.next;
        while (itr->next != *head) {
            itr = itr->next;
        }
    }
    fileList * fl = (*head)->next;
    free((*head)->path);
    free(*head);
    itr->next = fl;
}

void appendData(Data** head, char* str) {
    Data* new_data = (Data *)malloc(sizeof(Data));
    new_data->path = (char *)malloc((strlen(str)+1) * sizeof(char));
    strcpy(new_data->path, str);
    new_data->next = 0x0;

    if(*head == 0x0) {
        *head = new_data;
        return;
    }

    Data* curr_data = *head;
    while (curr_data->next != NULL) {
        curr_data = curr_data->next;
    }
    curr_data->next = new_data;
}

void printData(Data* head) {
    printf("[\n");
    if (head != 0x0) printf("  [\n");
    Data* curr_data = head;
    while (curr_data != 0x0) {
        if (strcmp(curr_data->path, "<>") == 0) {
            printf("  ]");
            if (curr_data->next != 0x0) {
                printf(",\n  [\n");
            } 
        }
        else {
            printf("    %s", curr_data->path);
            if (!(curr_data->next == 0x0 || strcmp(curr_data->next->path, "<>") == 0))
                printf(",");
            printf("\n");
        }
        curr_data = curr_data->next;
    }
    if (head != 0x0) printf("  ]\n");
    printf("]\n");

    // Data* curr_data = head;
    // while (curr_data != 0x0) {
    //     printf("%s\n", curr_data->path);
    //     curr_data = curr_data->next;
    // }
}

void data2File(Data* head, char* name) {
    FILE * newFile;
    newFile = fopen(name, "w");
    if (newFile == NULL) {
        perror("write");
        return;
    }

    fprintf(newFile, "[\n");
    if (head != 0x0) fprintf(newFile, "  [\n");
    Data* curr_data = head;
    while (curr_data != 0x0) {
        if (strcmp(curr_data->path, "<>") == 0) {
            fprintf(newFile, "  ]");
            if (curr_data->next != 0x0) {
                fprintf(newFile, ",\n  [\n");
            } 
        }
        else {
            fprintf(newFile, "    %s", curr_data->path);
            if (!(curr_data->next == 0x0 || strcmp(curr_data->next->path, "<>") == 0))
                fprintf(newFile, ",");
            fprintf(newFile, "\n");
        }
        curr_data = curr_data->next;
    }
    if (head != 0x0) fprintf(newFile, "  ]\n");
    fprintf(newFile, "]");

    fclose(newFile);
}

void freeData(Data* head) {
    Data* curr_data = head;
    while (curr_data != 0x0) {
        Data* next = curr_data->next;
        free(curr_data->path);
        free(curr_data);
        curr_data = next;
    }
}

void scanDir(const char *path, int minimum_size) {
    // printf("Enter scanDir\n");
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
                appendFL(&fl_head.next, fileSize, filePath);
            }

            // printf("fileSize: %ld / filePath: %s\n", fileSize, filePath);
            // printf("%p\t\t%p\n", fl_head.next, fl_last);
        }
    }

    closedir(dir);
}

void compareFile() {
    FILE *std, *file;
    while (fl_head.next != 0x0) {
        // printf("debug: %s\n", fl_head.next->path);
        std = fopen(fl_head.next->path, "rb");
        if(std == NULL) {
            perror("std");
        }

        int isSame = -1;
        int wasSame = 0;
        fileList * curr_file = fl_head.next->next;
        while (curr_file != 0x0) {
            // printf("debug 2: %s\n", curr_file->path);
            fileList* next = curr_file->next;

            file = fopen(curr_file->path, "rb");
            if(file == NULL) {
                perror("file");
            }

            fseek(std, 0, SEEK_END);
            long file1Size = ftell(std);
            fseek(file, 0, SEEK_END);
            long file2Size = ftell(file);
            if (file1Size != file2Size) {
                fclose(file);
                curr_file = next;
                continue;
            }
            rewind(std);
            rewind(file);

            char buffer1[BUFFER_SIZE];
            char buffer2[BUFFER_SIZE];
            size_t bytesRead1, bytesRead2;

            while ((bytesRead1 = fread(buffer1, sizeof(char), BUFFER_SIZE, std)) > 0 &&
                (bytesRead2 = fread(buffer2, sizeof(char), BUFFER_SIZE, file)) > 0) {
                if (memcmp(buffer1, buffer2, bytesRead1) != 0) {
                    isSame = 0;
                    break;
                }
            }

            if (isSame != 0) {
                wasSame = 1;
                // printf("Same!!\n");
                if (isSame == -1) {
                    appendData(&data_head.next, fl_head.next->path);
                }
                appendData(&data_head.next, curr_file->path);
                freeFL(&curr_file);
            }

            isSame = 1;
            curr_file = next;

            fclose(file);
        }

        if (wasSame) {
            appendData(&data_head.next, "<>");
        }

        freeFL(&fl_head.next);
        fclose(std);
    }

    Data* curr_data = data_head.next;
    if (curr_data != 0x0) {
        while (curr_data->next->next != 0x0) {
            curr_data = curr_data->next;
        }
        free(curr_data->next->path);
        free(curr_data->next);
        curr_data->next = 0x0;
    }
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
                // printf("---------------------------------\n");
                // printf("thread_num : %d\n", thread_num);
                // printf("minimum_size : %d\n", minimum_size);
                // printf("file_name : %s\n", file_name);
                // printf("dir_path : %s\n", dir_path);
                // printf("---------------------------------\n\n");
                /*---------------------------------------*/

    // Initiate thread and etc
    

    // Run find function
    const char *directoryPath = dir_path;
    scanDir(directoryPath, minimum_size);

                /*----------------debug------------------*/
                // printf("---------------------------------\n");
                // fileList * itr;
                // int i;
                // for (itr = fl_head.next, i=0; itr != 0x0; itr = itr->next, i++) {
                //     printf("[%d] %s (Size: %ld)\n", i, itr->path, itr->size);
                // }
                // printf("---------------------------------\n\n");
                /*---------------------------------------*/

    compareFile();

    if (file_name == 0x0) {
        printData(data_head.next);
    }
    else {
        data2File(data_head.next, file_name);
    }
    // Finish program (free, ...)

    return 0;
}