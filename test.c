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

pthread_mutex_t fl_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

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
            if (curr_data->next != 0x0) printf(",");
            printf("\n");
        }
        curr_data = curr_data->next;
    }
    printf("]\n");
}

void freeData(Data** head) {
    Data * curr_data = *head;
    while (curr_data != 0x0) {
        Data * temp_data = curr_data;
        curr_data = curr_data->next;
        free(temp_data->path);
        free(temp_data);
    }
    *head = 0x0;
}

void* scanAndCompare(void* arg) {
    char* directoryPath = (char*)arg;
    DIR* dir;
    struct dirent* entry;

    dir = opendir(directoryPath);
    if (dir == NULL) {
        fprintf(stderr, "Cannot open directory '%s'\n", directoryPath);
        pthread_exit(NULL);
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[BUFFER_SIZE];
        struct stat statbuf;

        snprintf(path, BUFFER_SIZE, "%s/%s", directoryPath, entry->d_name);

        if (stat(path, &statbuf) == -1) {
            fprintf(stderr, "Cannot stat '%s'\n", path);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            pthread_mutex_lock(&data_mutex);
            appendData(&data_head.next, "<>");
            appendData(&data_head.next, path);
            pthread_mutex_unlock(&data_mutex);
            pthread_mutex_lock(&fl_mutex);
            appendFL(&fl_head.next, statbuf.st_size, path);
            pthread_mutex_unlock(&fl_mutex);
        }
        else if (S_ISREG(statbuf.st_mode)) {
            pthread_mutex_lock(&fl_mutex);
            appendFL(&fl_head.next, statbuf.st_size, path);
            pthread_mutex_unlock(&fl_mutex);
        }
    }

    closedir(dir);
    pthread_exit(NULL);
}

void* compareFiles(void* arg) {
    while (1) {
        pthread_mutex_lock(&fl_mutex);
        fileList* curr_file = fl_head.next;
        if (curr_file == 0x0) {
            pthread_mutex_unlock(&fl_mutex);
            break;
        }
        fl_head.next = curr_file->next;
        pthread_mutex_unlock(&fl_mutex);

        FILE* file = fopen(curr_file->path, "rb");
        if (file == NULL) {
            fprintf(stderr, "Cannot open file '%s'\n", curr_file->path);
            freeFL(&curr_file);
            continue;
        }

        char buffer[BUFFER_SIZE];
        int readBytes;
        long totalBytes = 0;
        while ((readBytes = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
            totalBytes += readBytes;
        }

        fclose(file);

        pthread_mutex_lock(&data_mutex);
        appendData(&data_head.next, curr_file->path);
        appendData(&data_head.next, "<>");
        appendData(&data_head.next, "Size: ");
        char sizeString[20];
        snprintf(sizeString, 20, "%ld", curr_file->size);
        appendData(&data_head.next, sizeString);
        appendData(&data_head.next, "Total bytes read: ");
        char totalBytesString[20];
        snprintf(totalBytesString, 20, "%ld", totalBytes);
        appendData(&data_head.next, totalBytesString);
        pthread_mutex_unlock(&data_mutex);

        freeFL(&curr_file);
    }

    pthread_exit(NULL);
}

int main() {
    signal(SIGINT, handleSIGINT);

    pthread_t scanThread, compareThread;

    char directoryPath[BUFFER_SIZE];
    printf("Enter directory path: ");
    fgets(directoryPath, BUFFER_SIZE, stdin);
    directoryPath[strcspn(directoryPath, "\n")] = '\0';

    int ret = pthread_create(&scanThread, NULL, scanAndCompare, directoryPath);
    if (ret) {
        fprintf(stderr, "Error creating scan thread: %d\n", ret);
        return 1;
    }

    ret = pthread_create(&compareThread, NULL, compareFiles, NULL);
    if (ret) {
        fprintf(stderr, "Error creating compare thread: %d\n", ret);
        return 1;
    }

    pthread_join(scanThread, NULL);
    pthread_join(compareThread, NULL);

    printf("\nScanning and comparison finished. Results:\n");
    printData(data_head.next);

    freeFL(&fl_head.next);
    freeData(&data_head.next);

    return 0;
}