#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#define BUFFERSIZE 256
#define DELIMITERS ","
#define CORES 2

struct Process {
    int Id;
    double bursttime;
    double starttime;
    double finishtime;
    double timeelapsed;
    int core;
    struct Process *next;
};

struct Process *waitqueuehead = NULL;
int processcount = 0;

struct timeval global_start;

pthread_t thread_ids[CORES];
struct Process *process_in_core[CORES];
bool finished_core[CORES];
int active_thread_count = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static double time_since_start_ms(struct timeval *time){
    return ((double)(time->tv_sec - global_start.tv_sec) * 1000) + ((double)(time->tv_usec - global_start.tv_usec)) / 1000;
}

struct Process* createProcess(int id, double bursttime){
    struct Process *newProcess = (struct Process *)malloc(sizeof(struct Process));
    if(!newProcess){perror("mallocation failed"); exit(1);}
    newProcess->Id = id;
    newProcess->bursttime = bursttime;
    newProcess->starttime = 0.0;
    newProcess->finishtime = 0.0;
    newProcess->timeelapsed = 0.0;
    newProcess->next = NULL;
    return newProcess;
}

void insert_waitqueue_sjf(int id, double bursttime){
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
    waitqueuehead = NULL;
    return;
}

void print_final_report(){
    if(waitqueuehead == NULL){
        printf("Empty list\n");
        return;
    }
    struct Process *curr = waitqueuehead;
    double waitsum = 0.0;
    double maxtime = 0.0;
    printf("============================================================================\n");
    printf("|| %-7s || %-7s || %-9s || %-9s || %-9s ||%-9s||\n", "Process", "Core", "Burst(ms)", "Start(ms)", "End(ms)", "Elapsed(ms)");
    printf("============================================================================\n");
    while(curr != NULL){
        printf("|| %-7d || %-7d || %-9f || %-9f || %-9f || %-9f ||\n", curr->Id, curr->core, curr->bursttime, curr->starttime, curr->finishtime, curr->timeelapsed);
        waitsum = waitsum + curr->starttime; //minus arrival time but I'm assuming all the arrival times are zero
        if(curr->finishtime > maxtime){maxtime = curr->finishtime;} //this is just to get the elapsed time at the end of the processes, so the program time after doesn't interfere
        curr = curr->next;
    }
    printf("============================================================================\n");
    printf("Average wait time: %f\n", waitsum / processcount);
    printf("Total Time: %f\n", maxtime);

    return;
}

void *threadProcess(void *arg){
    struct Process *process = (struct Process*)arg;
    struct timeval time1, time2;
    gettimeofday(&time1, NULL);
    process->starttime = time_since_start_ms(&time1);

    pthread_mutex_lock(&lock);
    printf("[t=%.6f] Core %d started Process %d (burst %.6f ms)\n", process->starttime, process->core, process->Id, process->bursttime);
    pthread_mutex_unlock(&lock);

    usleep((useconds_t)process->bursttime * 1000);
    gettimeofday(&time2, NULL);
    process->finishtime = time_since_start_ms(&time2);
    process->timeelapsed = process->finishtime - process->starttime;

    pthread_mutex_lock(&lock);
    finished_core[process->core] = true;
    pthread_cond_signal(&cond);
    printf("[t=%.6f] Core %d finished Process %d (elapsed %.6f ms)\n", process->finishtime, process->core, process->Id, process->timeelapsed);
    pthread_mutex_unlock(&lock);

    return NULL;
}

void join_finished_threads(){
    for(int i = 0; i < CORES; i++){
        if(finished_core[i]){
            finished_core[i] = false;
            
            pthread_mutex_unlock(&lock);
            pthread_join(thread_ids[i], NULL);
            pthread_mutex_lock(&lock);

            process_in_core[i] = NULL;
            active_thread_count--;
            pthread_cond_signal(&cond);
        }
    }
    return;
}

int main() {
    for(int i = 0; i < CORES; i++){process_in_core[i] = NULL; finished_core[i] = false;}
    gettimeofday(&global_start, NULL);
    char buffer[BUFFERSIZE];
    FILE *fptr = fopen("input.csv", "r");
    if(fptr == NULL){
        fprintf(stderr, "Error opening input file\n");
        return 1;
    }
    while(fgets(buffer, BUFFERSIZE, fptr)){
        buffer[strcspn(buffer, "\r\n")] = 0;
        char *token = strtok(buffer, DELIMITERS);
        if(!token){continue;}
        int id = atoi(token);
        token = strtok(NULL, DELIMITERS);
        if(!token){continue;}
        double burst = strtod(token, NULL);
        insert_waitqueue_sjf(id, burst);
    }
    fclose(fptr);
    struct Process *curr = waitqueuehead;
    while(curr != NULL){
        pthread_mutex_lock(&lock);
        while(active_thread_count == CORES){
            pthread_cond_wait(&cond, &lock);
            join_finished_threads();
        }
        int slot = -1;
        for(int i = 0; i < CORES; i++){
            if(process_in_core[i] == NULL){
                slot = i;
                break;
            }
        }
        if(slot == -1){ //should literally be impossible, but threads are weird so it doesn't hurt to check.
            pthread_mutex_unlock(&lock);
            continue;
        }
        process_in_core[slot] = curr;
        curr->core = slot;
        if(pthread_create(&thread_ids[slot], NULL, threadProcess, (void*)curr) != 0){
            perror("Error creating thread");
            pthread_mutex_unlock(&lock);
            break;
        }
        active_thread_count++;
        pthread_mutex_unlock(&lock);
        curr = curr->next;
    }
    pthread_mutex_lock(&lock);
    while(active_thread_count > 0){
        pthread_cond_wait(&cond, &lock);
        join_finished_threads();
    }
    pthread_mutex_unlock(&lock);
    print_final_report();
    free_list();
    return 0;
}
