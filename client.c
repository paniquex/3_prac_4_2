#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wait.h>

const int SHARED_MEMORY_SIZE = 500;

int main(int agrc, char *argv[]) { //0 - resource // 1 - server
	key_t key = ftok("resource", 'r');
	int semid_resource = semget(key, 3, 0);
	if (semid_resource == -1) {
		printf("error\n");
		return 1;
	}
//	semctl(semid_resource, 0, SETVAL, (int) 1);
//	semctl(semid_resource, 1, SETVAL, (int) 0);

	int shmid = shmget(key , SHARED_MEMORY_SIZE, 0);
	char *shmaddr = shmat(shmid, NULL, 0);


	struct sembuf sops;
	sops.sem_op = -1;
	sops.sem_num = 2;
	printf("User waiting for server\n");
	semop(semid_resource, &sops, 1);
	printf("User waiting for resources\n");

	sops.sem_op = -1;
	sops.sem_num = 0;
	semop(semid_resource, &sops, 1);
	printf("User working with resources\n");
	if (fork() == 0) {
		int fd[2];
		pipe(fd);
		if (fork() == 0) {
			dup2(fd[1], 1);
			close(fd[1]);
			close(fd[0]);
			execvp("ls", argv);
			return 0;
		}
		close(fd[1]);
		int i = 0;
		int read_error;
		while ((read_error = read(fd[0], shmaddr + i, sizeof(char))) != 0) {
			if (read_error != -1) {
				i++;
			}
			if (i >= SHARED_MEMORY_SIZE) {
				break;
			}
		}
		char terminate_symbol = '\0';
		*(shmaddr + i) = terminate_symbol;
		close(fd[0]);
		wait(NULL);
	}
	wait(NULL);

	sops.sem_op = 1;
	semop(semid_resource, &sops, 1);
	printf("User has ended work with resources\n");
	sops.sem_num = 1;
	semop(semid_resource, &sops, 1);
	printf("User is off\n");
	return 0;
}
