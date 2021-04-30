#define _DEFAULT_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

void child();
void parent();
void process_stage(int stage);

int semaphore_id;
int write_semid;
int read_semid;

struct sembuf lock= {0,-1,SEM_UNDO};
struct sembuf unlock= {0,1,SEM_UNDO};

int stages=0;

int semaphore_create()
{
    write_semid =semget(IPC_PRIVATE,1,IPC_CREAT|0666);
    read_semid=semget(IPC_PRIVATE,1,IPC_CREAT|0666);
    if(write_semid == -1 || read_semid == -1) {
        perror("semget");
        return -1;
    }
    return 0;
}

void c_wait_for_parent_to_read()
{
    if(semop(write_semid,&lock,1)==-1) {
        printf("\n Error:%d",errno);
    }
}

void c_signal_parent_to_read()
{
    if(semop(read_semid, &unlock, 1)==-1) {
        printf("\n Error:%d",errno);
    }
}

//Wait for the child to write data to shared memory.
void p_wait_for_child_write_data()
{
    if(semop(read_semid,&lock,1)==-1) {
        printf("\n Error:%d",errno);
    }
}

void p_signal_child_to_write_data()
{
    if(semop(write_semid, &unlock, 1)==-1) {
        printf("\n Error:%d",errno);
    }
}

int create_shared_mem(int size)
{
    int shmid;
    shmid = shmget(IPC_PRIVATE, size, 0777|IPC_CREAT);
    return shmid;
}


//Write a message to the shared memory.
void write_message(int shmid, char * message)
{
    char *shared_mem = (char *) shmat(shmid, 0, 0);
    strcpy(shared_mem,message);
    shmdt(shared_mem);
}

char *read_message(int shmid, int length)
{
    char *message = (char*)malloc(length);
    char * shared_memory = (char *) shmat(shmid, 0, 0);
    strncpy(message,shared_memory,length);
    shmdt(shared_memory);
    return message;
}

void remove_shared_mem(int shmid)
{
    shmctl(shmid, IPC_RMID, 0);
}

int get_child_exit_status()
{
    int stat;
    wait(&stat);
    return WEXITSTATUS(stat);
}

void child() {


    for(int i=1; i<=stages; i++)
    {
        c_wait_for_parent_to_read();
        char message[32];
        sprintf(message,"%s%d","STAGE",i);
        process_stage(i);
        //Stage 1 done
        write_message(semaphore_id,message);
        c_signal_parent_to_read();
    }

    exit(stages);
}
void parent()
{


    for(int i=1; i<=stages; i++)
    {
        char *message;
        printf("Waiting for the child to finish the stage:%d\n",i);
        fflush(stdout);
        p_signal_child_to_write_data();
        p_wait_for_child_write_data();
        message=read_message(semaphore_id,32);
        printf("STAGE completed:%s\n",message);
        fflush(stdout);

    }

    printf("Child exited with status:%d\n",get_child_exit_status());
    remove_shared_mem(semaphore_id);
}

void process_stage(int stage)
{
    printf("Procesing stage%d\n",stage);
    usleep(1);
    fflush(stdout);
}

int main(int argc, char* argv[])
{
    pid_t cid;
    semaphore_id = create_shared_mem(100);
    if(semaphore_id == 0 )
    {
        printf("Shared Mem creation failed\n");
    }
    int status = semaphore_create();
    if(status != 0 )
    {
        printf("Semaphorecreation failed\n");
    }
    scanf("%d",&stages);
    cid = fork();

    // Parent process
    if (cid == 0)
    {
        child();
    } else if(cid > 0 )
    {
        parent();
    }
}
