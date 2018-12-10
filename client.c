#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

int server_finish = 0;

int main(int agrc, char *argv[]) { //0 - resource // 1 - server
	key_t key = ftok("resource", 'r');
	int semid_resource = semget(key, 2, 0);
	if (semid_resource == -1) {
		printf("error\n");
	}
	semctl(semid_resource, 0, SETVAL, (int) 1);
	semctl(semid_resource, 1, SETVAL, (int) 0);

	int shmid = shmget(key , sizeof(int), 0);
	char *shmaddr = shmat(shmid, NULL, 0);


	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	semop(semid_resource, &sops, 1);

	if (fork() == 0) {
		execvp("ls", argv);
	}
	wait(NULL);
	sops.sem_op = 1;
	semop(semid_resource, &sops, 1);
	sops.sem_num = 1;
	semop(semid_resource, &sops, 1);
	return 0;
}
