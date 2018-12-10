#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

int server_finish = 0;

void
sigterm_handler(int sig);

int main(int agrc, char *argv[]) { //0 - resource // 1 - server
	signal(SIGINT, sigterm_handler);
	key_t key = ftok("resource", 'r');
	int semid_resource = semget(key, 2, IPC_CREAT | 0666);
	semctl(semid_resource, 0, SETVAL, (int) 1);
	semctl(semid_resource, 1, SETVAL, (int) 0);

	int shmid = shmget(key , sizeof(int), IPC_CREAT | 0666);
	char *shmaddr = shmat(shmid, NULL, 0);
	


	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	while (server_finish != 1) {
		sops.sem_op = -1;
		sops.sem_num = 1;
		semop(semid_resource, &sops, 1);
		sops.sem_num = 0;
		semop(semid_resource, &sops, 1);
		printf("server\n");
		sops.sem_op = 1;
		semop(semid_resource, &sops, 1);
	}
	semctl(semid_resource, 0, IPC_RMID, NULL);
	semctl(semid_resource, 1, IPC_RMID, NULL);
	shmdt(shmaddr);
	return 0;
}


void
sigterm_handler(int sig) {
	server_finish = 1;
}