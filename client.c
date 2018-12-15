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
	key_t key = ftok("ls_output", 'r');
	int semid_resource = semget(key, 4, 0);
	if (semid_resource == -1) {
		perror(0);
		return 1;
	}
	int shmid_ls_output = shmget(key , SHARED_MEMORY_SIZE, 0);
	if (shmid_ls_output == -1) {
		perror(0);
		return 1;
	}
	char *shmaddr_ls_output = shmat(shmid_ls_output, NULL, 0);

	struct sembuf sops;
	sops.sem_op = -1;
	sops.sem_num = 3;
	printf("-----User waiting for end of another CLIENT work-----\n");
	if (semop(semid_resource, &sops, 1) == -1) {
		perror(0);
		return 1;
	};
	printf("-----User waiting for resources-----\n");
	sops.sem_num = 0;
	if (semop(semid_resource, &sops, 1) == -1) {
		perror(0);
		semctl(semid_resource, 3, SETVAL, (int) 1); //up for new client
		return 1;
	};
	printf("-----User working with resources-----\n");
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
		sleep(4); // debug
		while ((read_error = read(fd[0], shmaddr_ls_output + i, sizeof(char))) != 0) {
			if (read_error != -1) {
				i++;
			}
			if (i >= SHARED_MEMORY_SIZE) {
				break;
			}
		}
		char terminate_symbol = '\0';
		*(shmaddr_ls_output + i) = terminate_symbol;
		close(fd[0]);
		wait(NULL);
		return 0;
	}
	wait(NULL);

	sops.sem_op = 1;
	sops.sem_num = 0;
	if (semop(semid_resource, &sops, 1) == -1) {
		perror(0);
		semctl(semid_resource, 3, SETVAL, (int) 1); //up for new client
		semctl(semid_resource, 0, SETVAL, (int) 1);
		return 1;
	}
	printf("-----User has ended work with resources\n-----");
	sops.sem_num = 1;
	if (semop(semid_resource, &sops, 1) == -1) {
		perror(0);
		semctl(semid_resource, 1, SETVAL, (int) 1); // anyway wake up server
		semctl(semid_resource, 3, SETVAL, (int) 1); //up for new client
		return 1;
	}
	printf("-----Server can wake up-----\n");
	sops.sem_num = 3;
	if (semop(semid_resource, &sops, 1) == -1) {
		perror(0);
		semctl(semid_resource, 3, SETVAL, (int) 1); //up for new client
		return 1;
	}
	printf("-----User has ended full work-----\n\n");
	return 0;
}
