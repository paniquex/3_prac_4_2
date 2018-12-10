#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdlib.h>

int server_finish = 0;

int main(int agrc, char *argv[]) {
	key_t key = ftok("resource", 'r');
	key_t key_sem_server = ftok("server", 'r');
	int semid_resourse = semget(key, 1, IPC_CREAT | 0666);
	int semid_server = semget(key_sem_server, 1, IPC_CREAT | 0666);
	int shmid = shmget(key , sizeof(int), IPC_CREAT | 0666);
	char *shmaddr = shmat(shmid, NULL, 0);
	struct sembuf sops;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	while (server_finish != 1) {
		semop(semid_resourse, &sops, 1);
	}
}