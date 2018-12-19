#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>


const int SHARED_MEMORY_SIZE = 500;

const char *NAME_LS_OUTPUT = "ls_output";
const char *NAME_ONLINE_SERV_COUNT = "online_serv_count";
int fd_online_serv_count;
int fd_ls_output;

struct sembuf sops_online_serv_count;

int server_finish = 0;
int server_state = 0; // 0 - if server is waiting for end of another server work
                     // 1 - if server is working or waiting for user( not waiting for another server)
int semid_resource;
int semid_online_serv_count;
int shmid_ls_output;
char *shmaddr_ls_output;

void
sigterm_handler(int sig);

int main(int agrc, char *argv[]) { //0 - resource // 1 - server_sleep // 2 - server_count // 3 - client_count
	//creating shared memory to save amount of online servers

	fd_online_serv_count = open(NAME_ONLINE_SERV_COUNT, O_CREAT, 0666);
	fd_ls_output = open(NAME_LS_OUTPUT, O_CREAT, 0666);
	key_t key_online_serv_count = ftok(NAME_ONLINE_SERV_COUNT, 'r');
	if ((semid_online_serv_count = semget(key_online_serv_count, 1, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
		if ((semid_online_serv_count = semget(key_online_serv_count, 1, 0)) == -1) { // not first server
			close(fd_ls_output);
			close(fd_online_serv_count);
			unlink(NAME_ONLINE_SERV_COUNT);
			unlink(NAME_LS_OUTPUT);
			perror(0);
			exit(0);
		}

	} else {
			if (semctl(semid_online_serv_count, 0, SETVAL, (int) 0) == -1) {
				semctl(semid_online_serv_count, 0, IPC_RMID);
				close(fd_ls_output);
				close(fd_online_serv_count);
				unlink(NAME_ONLINE_SERV_COUNT);
				unlink(NAME_LS_OUTPUT);
				perror(0);
				exit(0);
			}
		}

	sops_online_serv_count.sem_op = 1;
	sops_online_serv_count.sem_num = 0;
	sops_online_serv_count.sem_flg = 0;
	semop(semid_online_serv_count, &sops_online_serv_count, 1);
	printf("%d\n", semctl(semid_online_serv_count, 0, GETVAL));

	if (signal(SIGINT, sigterm_handler) == SIG_ERR) {
		if (semctl(semid_online_serv_count, 0, GETVAL) == 1) {
			semctl(semid_online_serv_count, 0, IPC_RMID);
		} else {
			sops_online_serv_count.sem_op = -1;
			semop(semid_online_serv_count, &sops_online_serv_count, 1);
		}
		close(fd_ls_output);
		close(fd_online_serv_count);
		unlink(NAME_ONLINE_SERV_COUNT);
		unlink(NAME_LS_OUTPUT);
		perror("Signal error:");
		return(1);
	}

	key_t key_ls_output = ftok(NAME_LS_OUTPUT, 'r');
	if (key_ls_output == -1) {
		semctl(semid_online_serv_count, 0, IPC_RMID);
		close(fd_ls_output);
		close(fd_online_serv_count);
		unlink(NAME_ONLINE_SERV_COUNT);
		unlink(NAME_LS_OUTPUT);
		perror(0);
		return 1;
	}
	if ((semid_resource = semget(key_ls_output, 4, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
		if ((semid_resource = semget(key_ls_output, 4, 0)) == -1) {
			if (semctl(semid_online_serv_count, 0, GETVAL) == 1) {
				semctl(semid_online_serv_count, 0, IPC_RMID);
			} else {
				sops_online_serv_count.sem_op = -1;
				semop(semid_online_serv_count, &sops_online_serv_count, 1);
			}
			close(fd_ls_output);
			close(fd_online_serv_count);
			unlink(NAME_ONLINE_SERV_COUNT);
			unlink(NAME_LS_OUTPUT);
			perror(0);
			exit(1);
		};
	} else {
		semctl(semid_resource, 0, SETVAL, (int) 1);
		semctl(semid_resource, 1, SETVAL, (int) 0);
		semctl(semid_resource, 2, SETVAL, (int) 1);
		semctl(semid_resource, 3, SETVAL, (int) 1);
	}
	
	if ((shmid_ls_output = shmget(key_ls_output, SHARED_MEMORY_SIZE, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
		if ((shmid_ls_output = shmget(key_ls_output, SHARED_MEMORY_SIZE, 0)) == -1) {
			if (semctl(semid_online_serv_count, 0, GETVAL) == 1) {
				semctl(semid_online_serv_count, 0, IPC_RMID);
				semctl(semid_resource, 0, IPC_RMID, NULL);
				semctl(semid_resource, 1, IPC_RMID, NULL);
				semctl(semid_resource, 2, IPC_RMID, NULL);
				semctl(semid_resource, 3, IPC_RMID, NULL);
			} else {
				sops_online_serv_count.sem_op = -1;
				semop(semid_online_serv_count, &sops_online_serv_count, 1);
			}
			close(fd_ls_output);
			close(fd_online_serv_count);
			unlink(NAME_ONLINE_SERV_COUNT);
			unlink(NAME_LS_OUTPUT);
			perror(0);
			exit(0);
		}
	}


	struct sembuf sops;
	sops.sem_flg = 0;
	while (server_finish != 1) {
		sops.sem_op = -1;
		sops.sem_num = 2;
		printf("-----Server is waiting end of another SERVER work...-----\n");
		semop(semid_resource, &sops, 1);
		server_state = 1;
		sops.sem_num = 1;
		printf("-----Server is waiting end of USER work-----\n");
		semop(semid_resource, &sops, 1);
		//connecting to memory
		if ((shmaddr_ls_output = shmat(shmid_ls_output, NULL, 0)) == -1) {
			if ((shmid_ls_output = shmget(key_ls_output, SHARED_MEMORY_SIZE, 0)) == -1) {
				if (semctl(semid_online_serv_count, 0, GETVAL) == 1) {
					semctl(semid_online_serv_count, 0, IPC_RMID);
					shmctl(shmid_ls_output, IPC_RMID, NULL);
					semctl(semid_resource, 0, IPC_RMID, NULL);
					semctl(semid_resource, 1, IPC_RMID, NULL);
					semctl(semid_resource, 2, IPC_RMID, NULL);
					semctl(semid_resource, 3, IPC_RMID, NULL);
				} else {
					sops_online_serv_count.sem_op = -1;
					semop(semid_online_serv_count, &sops_online_serv_count, 1);
				}
				close(fd_ls_output);
				close(fd_online_serv_count);
				unlink(NAME_ONLINE_SERV_COUNT);
				unlink(NAME_LS_OUTPUT);
				perror(0);
				exit(0);
			}
		}
		sops.sem_num = 0;
		printf("-----Server is waiting for release of shared resources-----\n");
		semop(semid_resource, &sops, 1);
		printf("-----Server starts working with shared resources-----\n");
		//work with shared memory
		sleep(4); // for debug
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
		semop(semid_resource, &sops, 1);
		printf("-----Server has ended work with shared resources-----\n");
		sops.sem_num = 2;
		semop(semid_resource, &sops, 1);
		shmdt(shmaddr_ls_output);
		printf("-----Server has ended full work\n\n");
		server_state = 0;
	}
}


void
sigterm_handler(int sig) {
	sops_online_serv_count.sem_op = -1;
	semop(semid_online_serv_count, &sops_online_serv_count, 1);
	printf("%d", semctl(semid_online_serv_count, 0, GETVAL));
	if (semctl(semid_online_serv_count, 0, GETVAL) == 0) {
		semctl(semid_resource, 0, IPC_RMID, NULL);
		semctl(semid_resource, 1, IPC_RMID, NULL);
		semctl(semid_resource, 2, IPC_RMID, NULL);
		semctl(semid_resource, 3, IPC_RMID, NULL);
		semctl(semid_online_serv_count, 0, IPC_RMID);
		shmctl(shmid_ls_output, IPC_RMID, NULL);
		shmdt(shmaddr_ls_output);
		close(fd_ls_output);
		close(fd_online_serv_count);
		unlink(NAME_ONLINE_SERV_COUNT);
		unlink(NAME_LS_OUTPUT);
		exit(0);
	}
	semctl(semid_resource, 0, SETVAL, (int) 1);
	semctl(semid_resource, 1, SETVAL, (int) 0);
	if (server_state == 0) {
		semctl(semid_resource, 2, SETVAL, (int) 0);
	} else {
		semctl(semid_resource, 2, SETVAL, (int) 1);
	}
	semctl(semid_resource, 3, SETVAL, (int) 1);
	exit(0);
}