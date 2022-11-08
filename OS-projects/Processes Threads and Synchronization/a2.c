#include <stdlib.h>
#include <stdio.h>
#include "a2_helper.h"
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>

sem_t thread3_started, thread1_ended;
sem_t th10_can_end, th10_done;
sem_t sem_for_6_threads;
bool th10_still_running, th10_finished;
int threads_running_in_p7;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bool_var = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_th_counter = PTHREAD_MUTEX_INITIALIZER;
bool th10_running;
//pthread_cond_t freeze = PTHREAD_COND_INITIALIZER;
int sem_id;

int total_threads_p7;

void down(int semId, int semNr, int val) {
	struct sembuf op = {semNr, val < 0 ? val : -val, 0};
	semop(semId, &op, 1);
}

void up(int semId, int semNr, int val) {
	struct sembuf op = {semNr, val > 0 ? val : -val, 0};
	semop(semId, &op, 1);
}

void* process2_thread_function(void* id_void) {
    long id = (long)id_void + 1;
    
    if(id == 1) {
        sem_wait(&thread3_started);
    }
    else if(id == 4) {
        down(sem_id, 0, 1);
    }

    info(BEGIN, 2, id);

    if(id == 3) {
        sem_post(&thread3_started);
        sem_wait(&thread1_ended);
    }

    info(END, 2, id);
    if(id == 1) {
        sem_post(&thread1_ended);
    }
    else if(id == 4)
        up(sem_id, 1, 1);

    pthread_exit(NULL);
}

void* process7_thread_function(void* id_void) {
    sem_wait(&sem_for_6_threads);
    long id = (long)id_void + 1;

    info(BEGIN, 7, id);

    pthread_mutex_lock(&mutex);
    threads_running_in_p7++;
    pthread_mutex_unlock(&mutex);

    if(id == 10) {

        pthread_mutex_lock(&mutex_bool_var);
        th10_running = true;
        pthread_mutex_unlock(&mutex_bool_var);

        sem_wait(&th10_can_end);

        info(END, 7, 10);
        sem_post(&th10_done);

        pthread_mutex_lock(&mutex_bool_var);
        th10_running = false;
        pthread_mutex_unlock(&mutex_bool_var);

        pthread_mutex_lock(&mutex_th_counter);
        th10_finished = true;
        pthread_mutex_unlock(&mutex_th_counter);
        
        pthread_exit(NULL);
    }

    pthread_mutex_lock(&mutex_th_counter);
    if(total_threads_p7 == 6 && !th10_finished) {
        pthread_mutex_unlock(&mutex_th_counter);
        pthread_mutex_lock(&mutex_bool_var);
        if(!th10_running) {

            pthread_mutex_unlock(&mutex_bool_var);
            
            pthread_mutex_lock(&mutex);
            if(threads_running_in_p7 == 5) {
                pthread_mutex_unlock(&mutex);
                sem_post(&th10_can_end);
            }
            else
                pthread_mutex_unlock(&mutex);
            sem_wait(&th10_done);
            sem_post(&th10_done);
        }
        else
            pthread_mutex_unlock(&mutex_bool_var);
    }    
    else
        pthread_mutex_unlock(&mutex_th_counter);

    pthread_mutex_lock(&mutex_bool_var);
    if(th10_running == true) {
        pthread_mutex_unlock(&mutex_bool_var);
        pthread_mutex_lock(&mutex);
        if(threads_running_in_p7 == 6)
            sem_post(&th10_can_end);
        pthread_mutex_unlock(&mutex);
        sem_wait(&th10_done);
        sem_post(&th10_done);
    }
    else 
        pthread_mutex_unlock(&mutex_bool_var);
    
    pthread_mutex_lock(&mutex_th_counter);
    total_threads_p7--;
    pthread_mutex_unlock(&mutex_th_counter);


    pthread_mutex_lock(&mutex);
    threads_running_in_p7--;
    pthread_mutex_unlock(&mutex);

    info(END, 7, id);
    
    sem_post(&sem_for_6_threads);
    pthread_exit(NULL);
}

void* process6_thread_function(void* id_void) {
    long id = (long)id_void + 1;

    if(id == 3)
        down(sem_id, 1, 1);

    info(BEGIN, 6, id);
    info(END, 6, id);

    if(id  == 2) {
        up(sem_id, 0, 1);
    }
    pthread_exit(NULL);
}

int main(){
    init();

    info(BEGIN, 1, 0);
    
    int p2, p3, p4, p5, p6, p7, p8, p9;
    p5 = p6 = p7 = -1;

    sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600);

	if(sem_id < 0)
		printf("Can't create sem\n");
	if (semctl(sem_id, 0, SETVAL, 1) < 0)
		printf("err\n");
    if (semctl(sem_id, 1, SETVAL, 1) < 0)
		printf("err\n");

    down(sem_id, 0, 1);
    down(sem_id, 1, 1);

    p2 = fork();
    if(p2 > 0)
        p4 = fork();
    
    if(p2 > 0 && p4 >0)
        p7 = fork();

    if(p2 == 0) {
        info(BEGIN, 2, 0);
        p3 = fork();
        if(p3 == 0) {
            info(BEGIN, 3, 0);
            p8 = fork();

            if(p8 > 0)
                p9 = fork();

            if(p8 == 0) {
                info(BEGIN, 8, 0);

                info(END, 8, 0);
                exit(1);
            }
            else if(p9 == 0){
                info(BEGIN, 9, 0);

                info(END, 9, 0);
                exit(1);
            }
            waitpid(p5, NULL, 0);
            waitpid(p6, NULL, 0);
            info(END, 3, 0);
            exit(1);
        }
        pthread_t threads[4];

        sem_init(&thread3_started, false, 1);
        sem_init(&thread1_ended, false, 1);

        sem_wait(&thread3_started);
        sem_wait(&thread1_ended);

        for(long i = 0; i < 4; i++) 
            pthread_create(&threads[i], NULL, process2_thread_function, (void*)i);
        
        for(long i = 0; i < 4; i++)
            pthread_join(threads[i], NULL);

        waitpid(p3, NULL, 0);
        info(END, 2, 0);
        exit(1);
    }
    else if(p4 == 0) {
        info(BEGIN, 4, 0);
        p5 = fork();
            
        if(p5 > 0) {
            p6 = fork();
        }

        if(p5 == 0) {
            info(BEGIN, 5, 0);

            info(END, 5, 0);
            exit(1);
        }
        else if(p6 == 0) {
            info(BEGIN, 6, 0);
            /////////////

            pthread_t threads[4];

            for(long i = 0; i < 4; i++)
                pthread_create(&threads[i], NULL, process6_thread_function, (void*)i);

            for(long i = 0; i < 4; i++)
                pthread_join(threads[i], NULL);

            info(END, 6, 0);
            exit(1);
        }
        waitpid(p5, NULL, 0);
        waitpid(p6, NULL, 0);
        info(END, 4, 0);
        exit(1);
    }
    else if(p7 == 0) {
        info(BEGIN, 7, 0);

        pthread_t threads[46];
        total_threads_p7 = 46;
        th10_finished = false;

        sem_init(&sem_for_6_threads, false, 6);
        sem_init(&th10_can_end, false, 1);
        sem_wait(&th10_can_end);

        sem_init(&th10_done, false, 1);
        sem_wait(&th10_done);
        th10_running = false;

        for(long i = 0; i < 46; i++) 
            pthread_create(&threads[i], NULL, process7_thread_function, (void*)i);

        for(long i = 0; i < 46; i++)
            pthread_join(threads[i], NULL);


        info(END, 7, 0);
        exit(1);
    }
    
    waitpid(p2, NULL, 0);
    waitpid(p4, NULL, 0);
    waitpid(p7, NULL, 0);
    info(END, 1, 0);
    semctl(sem_id, IPC_RMID, 0);
    return 0;
} 