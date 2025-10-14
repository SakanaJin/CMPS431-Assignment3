#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

const char DELIMITERS[] = ",";
const unsigned short BUFFERSIZE = 256;

struct Process {
    int Id;
    int bursttime;
    double starttime;
    double finishtime;
    double timeelapsed;
    int core;
    struct Process *next;
};

int threadscount = 0;
clock_t start_time, end_time;
struct Process *waitqueuehead = NULL;
int processcount = 0;

struct Process* createProcess(int id, int bursttime){
    struct Process *newProcess = (struct Process *)malloc(sizeof(struct Process));
    newProcess->Id = id;
    newProcess->bursttime = bursttime;
    newProcess->starttime = 0.0;
    newProcess->finishtime = 0.0;
    newProcess->timeelapsed = 0.0;
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
    processcount++;
    return;
}

void free_list(){
    while(waitqueuehead != NULL){
        struct Process *toRemove = waitqueuehead;
        waitqueuehead = waitqueuehead->next;
        free(toRemove);
    }
    return;
}

void print_llist_info(){
    if(waitqueuehead == NULL){
        printf("Empty list\n");
        return;
    }
    struct Process *curr = waitqueuehead;
    double waitsum = 0.0;
    printf("========================================================================\n");
    printf("|| %-7s || %-4s || %-5s || %-10s || %-10s || %-10s ||\n", "Process", "Core", "Burst", "Start", "End", "Elapsed");
    printf("========================================================================\n");
    while(curr != NULL){
        printf("|| %-7d || %-4d || %-5d || %-10f || %-10f || %-10f ||\n", curr->Id, curr->core, curr->bursttime, curr->starttime, curr->finishtime, curr->timeelapsed);
        waitsum = waitsum + curr->starttime; //minus arrival time but I'm assuming all the arrival times are zero
        curr = curr->next;
    }
    printf("========================================================================\n");
    printf("Average wait time: %f\n", waitsum / processcount);
    return;
}

void *threadProcess(void *arg){
    struct Process *process = (struct Process*)arg;
    pthread_t thread_id = pthread_self();
    long pausetime;
    clock_t time1, time2;
    pausetime = process->bursttime * (CLOCKS_PER_SEC / 1000);
    time1 = clock();
    process->starttime = (double)(time1 - start_time) / CLOCKS_PER_SEC;
    do{
        time2 = clock();
    }while((time2 - time1) < pausetime);
    process->timeelapsed = (double)(time2 - time1) / CLOCKS_PER_SEC;
    process->finishtime = (double)(time2 - start_time) / CLOCKS_PER_SEC;
    threadscount--;
    return NULL;
}

int main(){
    start_time = clock();
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
    struct Process *curr = waitqueuehead;
    while(curr != NULL){
        if(threadscount == 2){continue;}
        pthread_create(&thread_id[threadscount], NULL, threadProcess, (void *)curr);
        curr->core = threadscount;
        threadscount++;
        curr = curr->next;
    }
    for(int i = 0; i < threadscount; i++){
        pthread_join(thread_id[i], NULL);
    }
    print_llist_info();
    end_time = clock();
    printf("Total Time: %f\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);
    free_list();
    return 0;
}
