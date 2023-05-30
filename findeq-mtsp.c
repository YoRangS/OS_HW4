#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>

#define BUFFER_SIZE 1024

typedef struct _fileList {
    long size;
    char* path;
    struct _fileList * next;
} fileList;

fileList fl_head = {0, 0x0, 0x0};
// fileList * fl_last = 0x0;

typedef struct Data {
    char* path;
    struct Data* next;
} Data;

Data data_head = {0x0, 0x0};

pthread_mutex_t lock ;

typedef struct _subtask {
    fileList * fl_head;
    Data * d_head;
} subtask;

pthread_mutex_t lock_n_threads ;

subtask * subtasks[64] ;
int head = 0 ;
int tail = 0 ;

sem_t unused ;
sem_t inused ;
pthread_mutex_t subtasks_lock ;

int thread_num;
int minimum_size;
char* file_name;
char* dir_path;

void put_subtask (subtask * s) 
{
    printf("------put_subtask: %p\n", s);
	sem_wait(&unused) ;
	pthread_mutex_lock(&subtasks_lock) ;
		subtasks[tail] = s ;
		tail = (tail + 1) % thread_num ;
	pthread_mutex_unlock(&subtasks_lock) ;
	sem_post(&inused) ;
    printf("------put_subtask END\n");
}

subtask * get_subtask () 
{
	subtask * s ;
	sem_wait(&inused) ;
	pthread_mutex_lock(&subtasks_lock) ;
		s = subtasks[head] ;
		head = (head + 1) % thread_num ;
    printf("    get_subtask : %p\n", s);
	pthread_mutex_unlock(&subtasks_lock) ;
	sem_post(&unused) ;

	return s ;
}

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

void freeFL(fileList** head, fileList** first) {
    fileList * itr = 0x0;
    printf("%p vs %p\n", *head, (*first)->next);
    // printf("%s vs %s\n", (*head)->path, (*first)->next->path);
    if (*head == (*first)->next) {
        itr = *first;
    }
    else {
        itr = (*first)->next;
        while (itr->next != *head) {
            itr = itr->next;
        }
    }
    fileList * fl = (*head)->next;
    free(*head);
    *head = NULL;
    itr->next = fl;
}

void appendData(Data** head, char* str) {
    printf("********%p\n", *head);
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
        printf("BBBBBBBBBBBBBBB\n");
        printf("%s\n", curr_data->path);
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

void compareFile(fileList * fl, Data * data) {
    FILE *std, *file;
    // fileList * f_head;
    // Data * d_head;
    // memcpy(f_head, fl, sizeof(fileList *));
    // memcpy(d_head, data, sizeof(Data *));
    printf("    cmpfile (%p) (%p) (%s)\n", fl, fl->next, fl->next->path);
    printf("    cmpfile (%p) (%p) (%s)\n", data, data->next, data->path);
    if(fl == NULL) {
        printf("        NULL!!\n");
        return;
    }
    
    while (fl->next != 0x0) {
        printf("debug: %p\n", fl->next);
        printf("debug: %s\n", fl->next->path);
        std = fopen(fl->next->path, "rb");
        if(std == NULL) {
            perror("std");
        }

        int isSame = -1;
        int wasSame = 0;
        fileList * curr_file = fl->next->next;
        while (curr_file != 0x0) {
            printf("debug 2: %s\n", curr_file->path);
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
            printf("************isSame = %d\n", isSame);
            if (isSame != 0) {
                wasSame = 1;
                printf("************Same!!\n");
                pthread_mutex_lock(&lock) ;
                if (isSame == -1) {
                    printf("%s\n", fl->next->path);
                    printf("%p %p\n", data, data->next);
                    printf("%s\n", data->path);
                    appendData(&data->next, fl->next->path);
                    printf("************fl->next->path: %s\n", fl->next->path);
                }
                appendData(&data->next, curr_file->path);
                printf("************curr_file->path: %s\n", curr_file->path);
                pthread_mutex_unlock(&lock) ;
                freeFL(&curr_file, &fl);
            }

            isSame = 1;
            curr_file = next;

            fclose(file);
        }
        
        pthread_mutex_lock(&lock) ;
        if (wasSame) {
            appendData(&data->next, "<>");
        }
        printf("((%p))\n", fl->next);
        freeFL(&fl->next, &fl);
        printf("((%p))\n", fl->next);
        printf("    CCCCCCCCCCCCCCC\n");
        pthread_mutex_unlock(&lock) ;
        fclose(std);
    }
    printf("after cmpfile\n");

    Data* curr_data = data->next;
    pthread_mutex_lock(&lock) ;
    if (curr_data != 0x0) {
        while (curr_data->next->next != 0x0) {
            curr_data = curr_data->next;
        }
        free(curr_data->next->path);
        free(curr_data->next);
        curr_data->next = 0x0;
    }
    pthread_mutex_unlock(&lock) ;
}

void * travel (void * arg) {
    subtask * s = (subtask *) arg ;

    printf("start travel, before cmpfile %p\n", s->fl_head);
    // compareFile(&f_head, &d_head);
    
    compareFile(s->fl_head, s->d_head);

    // s --> global
    fl_head.next = s->fl_head;
    data_head.next = s->d_head;

    free(arg);
    return NULL;
}

void * worker (void * arg)
{
    subtask * s;
    while((s = get_subtask())) {
        printf("worker while : (%p)\n", s->fl_head);
        travel(s);
        printf("still in while\n");
    }
    printf("end while\n");
    return NULL;
}

void init_subtask() {
    for(int i = 0; i < thread_num; i++) {
        printf("check: (%p)\n", &fl_head);
        subtask * s = (subtask *)malloc(sizeof(subtask)) ;
        s->fl_head = (fileList*)malloc(sizeof(fileList)) ;
        s->d_head = (Data*)malloc(sizeof(Data));
        s->d_head->path = "";
        s->d_head->next = 0x0;
        printf("%p\n", s->fl_head);
        fileList * itr = fl_head.next;
        while(itr != 0x0) {
            appendFL(&(s->fl_head), itr->size, itr->path);
            itr = itr->next;
        }
        
        printf("init: (%p) (s:%p) (%p)\n", s->fl_head, s, s->fl_head->next);
        printf("init: (%s)\n", s->fl_head->next->path);
        printf("init: (%p) (%p)\n", s->d_head, s->d_head->next);
        put_subtask(s);
    }
}

int main(int argc, char* argv[])
{
    printf("START: fl_head (%p)\tdata_head (%p)\n", &fl_head, &data_head);
    int i;
    thread_num = 16;
    minimum_size = 1024;
    file_name = NULL;
    dir_path = NULL;

    // Register signal handler for SIGINT
    signal(SIGINT, handleSIGINT);

    // Check invalid inputs
    for (i = 1; i < argc - 1; i++) {
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

    // Initiate thread and etc

    pthread_t * threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_num) ;

	pthread_mutex_init(&lock, NULL) ;
	pthread_mutex_init(&lock_n_threads, NULL) ;
	pthread_mutex_init(&subtasks_lock, NULL) ;

	sem_init(&inused, 0, 0) ;
	sem_init(&unused, 0, thread_num) ;

    // Run find function
    const char *directoryPath = dir_path;
    scanDir(directoryPath, minimum_size);

    for (i = 0 ; i < thread_num ; i++) {
		pthread_create(&(threads[i]), NULL, worker, NULL) ;
	}

    init_subtask();

    for (i = 0 ; i < thread_num ; i++) {
		put_subtask(NULL) ;
        pthread_mutex_lock(&lock_n_threads) ;
        printf("    before join\n");
		pthread_join(threads[i], NULL) ;
        printf("    after join\n");
	    pthread_mutex_unlock(&lock_n_threads) ;
    }

    if (file_name == 0x0) {
        printData(data_head.next);
    }
    else {
        data2File(data_head.next, file_name);
    }

    // Finish program (free, ...)
    free(threads);
    fileList * tmp = (fileList*)malloc(sizeof(fileList));
    tmp->next = NULL;
    while(fl_head.next != 0x0) {
        // printf("freeeeeee\n");
        tmp->next = fl_head.next;
        freeFL(&fl_head.next, &tmp);
    }
    // printf("freeeeeee\n");
    // freeData(data_head.next);

    return 0;
}