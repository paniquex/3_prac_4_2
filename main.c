#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>


const int SHARED_MEMORY_SIZE = 500;
int server_finish = 0;
int semid_resource;
int shmid_online_serv_count;
char *shmaddr_online_serv_count;
int shmid_ls_output;
char *shmaddr_ls_output;

void
sigterm_handler(int sig);

int main(int agrc, char *argv[]) { //0 - resource // 1 - server_sleep // 2 - server_count // 3 - client_count
	if (signal(SIGINT, sigterm_handler) == SIG_ERR) {
		perror("Signal error:");
		return(0); // to safe end of server
	}

	key_t key_ls_output = ftok("ls_output", 'r');
	if (key_ls_output == -1) {
		perror(0);
		return 1;
	}
	if ((semid_resource = semget(key_ls_output, 4, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
		if ((semid_resource = semget(key_ls_output, 4, 0)) == -1) {
			perror(0);
			exit(0);
		};
	} else {
		semctl(semid_resource, 0, SETVAL, (int) 1);
		semctl(semid_resource, 1, SETVAL, (int) 0);
		semctl(semid_resource, 2, SETVAL, (int) 1);
		semctl(semid_resource, 3, SETVAL, (int) 1);
	}
	//creating shared memory to save amount of online servers
	key_t key_online_serv_count = ftok("online_serv_count", 'r');
	if ((shmid_online_serv_count = shmget(key_online_serv_count, sizeof(int), IPC_CREAT | IPC_EXCL | 0666)) == -1) {
		if ((shmid_online_serv_count = shmget(key_online_serv_count, sizeof(int), 0)) == -1) {
			perror(0);
			exit(0);
		};
		if ((shmaddr_online_serv_count = shmat(shmid_online_serv_count, NULL, 0)) == -1) {
			perror(0);
			exit(0);
		};
	} else {
		if ((shmaddr_online_serv_count = shmat(shmid_online_serv_count, NULL, 0)) == -1) {
			perror(0);
			exit(0);
		};
		(*shmaddr_online_serv_count) = 0;
	}
	(*shmaddr_online_serv_count)++;
	
	if ((shmid_ls_output = shmget(key_ls_output, SHARED_MEMORY_SIZE, IPC_CREAT | 0666)) == -1) {
		perror(0);
		exit(0);
	}
	if ((shmaddr_ls_output = shmat(shmid_ls_output, NULL, 0)) == -1) {
		perror(0);
		exit(0);
	};

	struct sembuf sops;
	sops.sem_flg = 0;
	while (server_finish != 1) {
		sops.sem_op = -1;
		sops.sem_num = 2;
		printf("-----Server is waiting end of another SERVER work...-----\n");
		if (semop(semid_resource, &sops, 1) == -1) {
			perror(0);
			raise(SIGINT);
		};
		sops.sem_num = 1;
		printf("-----Server is waiting end of USER work-----\n");
		if (semop(semid_resource, &sops, 1) == -1) {
			perror(0);
			raise(SIGINT);
		};
		sops.sem_num = 0;
		printf("-----Server is waiting for release of shared resources-----\n");
		if (semop(semid_resource, &sops, 1) == -1) {
			perror(0);
			raise(SIGINT);
		};
		printf("-----Server starts working with shared resources-----\n");
		//work with shared memory
		int i = 0;
		char symbol;
		while ((symbol = *(shmaddr_ls_output + i)) != '\0') {
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
		sops.sem_op = 1;
		sops.sem_num = 0;
		if (semop(semid_resource, &sops, 1) == -1) {
			perror(0);
			raise(SIGINT);
		};
		printf("-----Server has ended work with shared resources-----\n");
		sops.sem_num = 2;
		if (semop(semid_resource, &sops, 1) == -1) {
			perror(0);
			raise(SIGINT);
		};
		printf("-----Server has ended full work\n\n");
	}
}


void
sigterm_handler(int sig) {
	(*shmaddr_online_serv_count)--;
	printf("%d", (int) *shmaddr_online_serv_count);
	if (((int) *shmaddr_online_serv_count == 0)) {
		semctl(semid_resource, 0, IPC_RMID, NULL);
		semctl(semid_resource, 1, IPC_RMID, NULL);
		semctl(semid_resource, 2, IPC_RMID, NULL);
		semctl(semid_resource, 3, IPC_RMID, NULL);
		shmctl(shmid_online_serv_count, IPC_RMID, NULL);
		shmctl(shmid_ls_output, IPC_RMID, NULL);
		shmdt(shmaddr_online_serv_count);
		shmdt(shmaddr_ls_output);
		exit(0);
	}
	semctl(semid_resource, 0, SETVAL, (int) 1);
	semctl(semid_resource, 1, SETVAL, (int) 0);
	semctl(semid_resource, 2, SETVAL, (int) 1);
	semctl(semid_resource, 3, SETVAL, (int) 1);
	exit(0);
}