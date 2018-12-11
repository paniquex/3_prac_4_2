#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>


const int SHARED_MEMORY_SIZE = 500;
int server_finish = 0;
int semid_resource;
int shmid;
char *shmaddr;

void
sigterm_handler(int sig);

int main(int agrc, char *argv[]) { //0 - resource // 1 - server_sleep // 2 - server_ready_to_process_user
	signal(SIGINT, sigterm_handler);
	key_t key = ftok("resource", 'r');
	semid_resource = semget(key, 3, IPC_CREAT | 0666);
	semctl(semid_resource, 0, SETVAL, (int) 1);
	semctl(semid_resource, 1, SETVAL, (int) 0);
	semctl(semid_resource, 2, SETVAL, (int) 0);


	shmid = shmget(key, SHARED_MEMORY_SIZE, IPC_CREAT | 0666);
	shmaddr = shmat(shmid, NULL, 0);

	struct sembuf sops;
	sops.sem_flg = 0;
	while (server_finish != 1) {
		sops.sem_op = 1;
		sops.sem_num = 2;
		semop(semid_resource, &sops, 1);
		printf("Server is ready to next client\n");
		printf("Server is sleeping...\n");
		sops.sem_op = -2;
		sops.sem_num = 1;
		semop(semid_resource, &sops, 1);
		printf("Server is on... and waiting for resources\n");

		sops.sem_op = -1;
		sops.sem_num = 0;
		semop(semid_resource, &sops, 1);
		printf("Server working with resources\n");
		sleep(4);

		//work with shared memory
		int i = 0;
		char symbol;
		while ((symbol = *(shmaddr + i)) != '\0') {
			if (symbol != '\n') {
				printf("%c", symbol);
				i++;
			} else {
				printf("\n\n");
				i++;
			}
			if (i >= SHARED_MEMORY_SIZE) {
				printf("\n");
				break;
			}
		}
		printf("server\n");

		sops.sem_op = 1;
		sops.sem_num = 0;
		semop(semid_resource, &sops, 1);
		printf("Server has ended work with resources\n\n");
	}
}


void
sigterm_handler(int sig) {
	semctl(semid_resource, 0, IPC_RMID, NULL);
	semctl(semid_resource, 1, IPC_RMID, NULL);
	semctl(semid_resource, 2, IPC_RMID, NULL);
	shmctl(shmid, IPC_RMID, NULL);
	shmdt(shmaddr);
	exit(0);
}