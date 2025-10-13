#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

const char DELIMITERS[] = ",";
const unsigned short BUFFERSIZE = 256;
const unsigned short MAXPROCESSES = 100;

struct Process {
    int Id;
    int bursttime;
    struct Process *next;
};

int threadscount = 0;
int processdonecount = 0;
int processcount = 0;
struct Process *waitqueuehead = NULL;

struct Process* createProcess(int id, int bursttime){
    struct Process *newProcess = (struct Process *)malloc(sizeof(struct Process));
    newProcess->Id = id;
    newProcess->bursttime = bursttime;
    newProcess->next = NULL;
    return newProcess;
}

void insert_waitqueue_sjf(int id, int bursttime){
    struct Process *newProcess = createProcess(id, bursttime);
    if(waitqueuehead == NULL || waitqueuehead->bursttime > bursttime){
        newProcess->next = waitqueuehead;
        waitqueuehead = newProcess;
        return;
    }
    struct Process *curr = waitqueuehead;
    while(curr->next != NULL && curr->next->bursttime <= bursttime){
        curr = curr->next;
    }
    newProcess->next = curr->next;
    curr->next = newProcess;
    return;
}

void remove_top_waitqueue(){
    struct Process *toRemove = waitqueuehead;
    waitqueuehead = waitqueuehead->next;
    free(toRemove);
    return;
}

void print_llist(){
    if(waitqueuehead == NULL){
        printf("Empty list\n");
    }
    struct Process *curr = waitqueuehead;
    while(curr != NULL){
        printf("Process Id: %d\tBurst Time: %d\n", curr->Id, curr->bursttime);
        curr = curr->next;
    }
    return;
}

void *threadProcess(void *milliseconds){
    pthread_t thread_id = pthread_self();
    long pauseTime;
    clock_t time1, time2;
    pauseTime = *(int*)milliseconds * (CLOCKS_PER_SEC / 1000);
    time1 = clock();
    do{
        time2 = clock();
    } while ((time2 - time1) < pauseTime);
    printf("Thread %ld waited for: %f seconds\n", thread_id, (double)*(int*)milliseconds/1000);
    free(milliseconds);
    processdonecount++;
    threadscount--;
    return NULL;
}

int main(){
    char buffer[BUFFERSIZE];
    int id;
    int bursttime;
    FILE *fptr = fopen("testfile.csv", "r");
    if(fptr == NULL){
        printf("Error opening file\n");
        return 1;
    }
    while(fgets(buffer, BUFFERSIZE, fptr)){
        buffer[strcspn(buffer, "\n")] = 0;
        id = atoi(strtok(buffer, DELIMITERS));
        bursttime = atoi(strtok(NULL, DELIMITERS));
        insert_waitqueue_sjf(id, bursttime);
    }
    pthread_t thread_id[2];
    while(waitqueuehead != NULL){
        if(threadscount == 2){continue;}
        int *bursttimearg = malloc(sizeof(int));
        *bursttimearg = waitqueuehead->bursttime;
        pthread_create(&thread_id[threadscount], NULL, threadProcess, (void *)bursttimearg);
        threadscount++;
        remove_top_waitqueue();
    }
    for(int i = 0; i < threadscount; i++){
        pthread_join(thread_id[i], NULL);
    }
    return 0;
}

// int main() {
//     int processes[MAXPROCESSES];
//     clock_t start_time, end_time;
//     double cpu_time_used;
//     start_time = clock();
//     int id;
//     FILE *fptr = fopen("testfile.csv", "r");
//     if(fptr == NULL){
//         printf("Error opening file\n");
//         return 1;
//     }
//     char buffer[BUFFERSIZE];
//     int totaltime = 0;
//     int bursttime;
//     while (fgets(buffer, BUFFERSIZE, fptr) != NULL){
//         buffer[strcspn(buffer, "\n")] = 0;
//         id = atoi(strtok(buffer, DELIMITERS));
//         bursttime = atoi(strtok(NULL, DELIMITERS));
//         totaltime = totaltime + bursttime;
//         processes[id] = bursttime;
//         processcount++;
//     }
//     pthread_t thread_id[2];
//     while(processcount != processdonecount){
//         if(threadscount == 2){continue;}
//         if(processcount - 1 == processdonecount){break;}
//         int *bursttimearg = malloc(sizeof(int));
//         *bursttimearg = processes[processdonecount];
//         pthread_create(&thread_id[threadscount], NULL, threadProcess, (void *)bursttimearg);
//         threadscount++;
//     }
//     pthread_join(thread_id[threadscount], NULL);
//     end_time = clock();
//     cpu_time_used = ((double)(end_time - start_time) / CLOCKS_PER_SEC);
//     printf("Execution time: %f seconds\n", cpu_time_used);
//     printf("Expected time (no threads): %f seconds\n", (double)totaltime/1000);
//     return 0;
// }
