#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>				//libreria per i flag del file
#include <sys/ipc.h>
#include <sys/msg.h>			//per la msqid_ds del msgctl
#include <errno.h>
#include <signal.h>
#include <sys/sem.h>

#define BACKLOG 1000			//connessioni pendenti
#define TAGLIAM 256			//taglia del testo del messaggio
#define SIZE 32						//taglia base
#define CHIAVE 4					//taglia intero
#define TAGLIAU 133			//taglia utente
#define MAX 1000					//massimo numero di utenti per il server
#define MESSAGGIO 484		//taglia totale dei messaggi

#define NOMEFILE "DatiUtenti"		//MODIFICABILE

int serverSocket, socketAccept;
struct sockaddr_in my_addr;
struct sockaddr_in addrChiamante;
int addr = sizeof(addrChiamante);
int array_code[MAX];				//memorizza cd per ciascun utente
int array_connessi[MAX];		//memorizza gli utenti attualmente connessi
int array_messaggi[MAX];		//memorizza il numero di messaggi nella coda
int key; 									//chiave della coda dei messaggi associata a ciascun utente
struct msqid_ds structure;		//struttura per leggere il numero di msg in ciascuna coda
struct sigaction sa;
long id_sem;

typedef struct {
	char mittente[SIZE];
	char destinatario[SIZE];
	char oggetto[2 * SIZE];
	char testo[TAGLIAM];
} messaggio;

void termina(char * stringa) {
	printf("%s", stringa);
	exit(0);
}

void gestione_Int() {
	/*for (int i = 1; i < key; i++) {
		if (eliminazioneCode(array_code[i]) == -1) {
			printf("Errore nell'eliminazione delle code\n");
		}
	}
	printf("code eliminate\n");*/
	if (semctl(id_sem, 0, IPC_RMID, 1) == -1) {
		printf("Errore nell'eliminazione del semaforo");
	}
	termina("\nIl server è stato interrotto\n");
}

void gestione_Ill() {
	printf(
			"\nSi è tentato di eseguire un'operazione non consentita"
			" o di accedere a locazioni di memoria non disponibili all'applicazione.\n");
	raise(SIGINT);
}

void gestisci_SGP(int sock_c, int chiave) {
	array_connessi[chiave] = 0;
	int status;
	close(sock_c);
	pthread_exit((void*) &status);
}

void* function_id() {
	int ds_socket = socketAccept;
	int nuovo, command, status;
	int chiave_utente = 0;
	messaggio msg;

	if (key == MAX) {
		if (write(ds_socket, "si", 2) == -1) {
			printf("Errore nella comunicazione della disponibilità\n");
			close(ds_socket);
			pthread_exit((void*) &status);
		}
	}
	if (write(ds_socket, "no", 2) == -1) {
		printf("Errore nella comunicazione della disponibilità\n");
		close(ds_socket);
		pthread_exit((void*) &status);
	}

	if (read(ds_socket, &nuovo, 4) == -1) {
		if (errno != EINTR) {
			printf("Errore nella lettura utente\n");
			close(ds_socket);
			pthread_exit((void*) &status);
		}
	}

	if (nuovo == 1) {
		chiave_utente = functionNuovo(ds_socket);
		if (chiave_utente == -1) {
			printf("Errore nel lancio della function_Nuovo\n");
			gestisci_SGP(ds_socket, chiave_utente);
		}
	} else {
		chiave_utente = function_Registrato(ds_socket);
		if (chiave_utente == -1) {
			if (write(ds_socket, "no", CHIAVE) == -1) {
				printf("Errore nella notifica di errata registrazione\n");
			}
			printf("Errore nel lancio della function_Registrato\n");
			gestisci_SGP(ds_socket, chiave_utente);
		}
	}

	while (1) {
		if (read(ds_socket, &command, 4) == -1) {
			if (errno == EPIPE) {
				printf("Errore nella scrittura dei messaggi\n");
				gestisci_SGP(ds_socket, chiave_utente);
			}
		}

		if (write(ds_socket, "ok", 4) == -1) {
			if (errno == EPIPE) {
				printf("Errore nel riscontro\n");
				gestisci_SGP(ds_socket, chiave_utente);
			}
		}

		switch (command) {
		case 1:
			if (readMsg(ds_socket, chiave_utente, msg) == -1) {
				if (errno == EPIPE) {
					printf("Errore nella scrittura dei messaggi\n");
					gestisci_SGP(ds_socket, chiave_utente);
				}
			}
			break;

		case 2:
			if (sendMsg(ds_socket, msg) == -1) {
				if (errno == EPIPE) {
					printf("Errore nella scrittura dei messaggi\n");
					gestisci_SGP(ds_socket, chiave_utente);
				}
			}
			break;

		case 3:
			if (deleteMsg(ds_socket, chiave_utente, msg) == -1) {
				if (errno == EPIPE) {
					printf("Errore nell'eliminazione dei messaggi\n");
					gestisci_SGP(ds_socket, chiave_utente);
				}
			}
			break;
		default:
			break;
		}

	}
	return NULL;
}

int functionNuovo(int sock_c) {
	int eseguita = 1, confirm;
	int des_coda;
	char confermaReg[CHIAVE];
	char key_s[CHIAVE];
	struct sembuf oper;  //struttura per il semaforo
	struct {
		char nome[SIZE];
		char cognome[SIZE];
		char username[SIZE];
		char password[SIZE];
		int chiave;
	} utente;

	while (eseguita) {
		//non garantisco le lettura di tutti i byte--> ciclo while--> vedi Client login()
		if (read(sock_c, &utente.nome, SIZE) == -1){ printf("Errore read nome\n"); return -1;}
		if (read(sock_c, &utente.cognome, SIZE) == -1){ printf("Errore read cognome\n");return -1;}
		if (read(sock_c, &utente.username, SIZE) == -1){ printf("Errore read username\n");return -1;}

		if (verificaUsername(utente.username) == -1) {
			if (write(sock_c, "NonPresente", SIZE) == -1) { printf("Errore conferma\n"); return -1; }
			if (read(sock_c, &utente.password, SIZE) == -1){ printf("Errore read pswd\n");return -1;}
			if (read(sock_c, confermaReg, CHIAVE) == -1){
				printf("Errore conferma di registrazione\n");return -1;
			}
			confirm = atoi(confermaReg);

			if (confirm == 1) {
				utente.chiave = key;
				sprintf(key_s, "%d\n", key);

				oper.sem_flg = IPC_NOWAIT;		//semaforo per la scrittura su file
				oper.sem_num = 0;							//un unico elemento nell'array semaforico
				oper.sem_op = -1;							//decido di prelevare il token
				if (semop(id_sem, &oper, 1) == -1) {
					printf("Errore nel prelievo del token semaforico\n");
					return -1;
				}

				des_coda = msgget(utente.chiave, IPC_CREAT | 0660);
				if (des_coda == -1) {
					printf("Errore nella chiamata msgget\n");
					return -1;
				}

				/*struct msqid_ds str;
				 str.msg_qbytes = 100*MESSAGGIO;
				 if(msgctl(des_coda, IPC_SET, &str) == -1){
				 printf("Errore nel SET della coda appena creata\n");
				 if(msgctl(des_coda, IPC_RMID, NULL) == -1){
				 printf("Errore nell'eliminazione della coda appena creata\n");
				 }
				 printf("Coda eliminata\n");
				 return -1;
				 }*/
				array_code[key] = des_coda;

				int fd = open(NOMEFILE, O_RDWR | O_APPEND);
				if (fd == -1) {
					printf("Errore nell'apertura\n");
					return -1;
				}

				int result;
				result = write(fd, utente.nome, SIZE);
				result = write(fd, utente.cognome, SIZE);
				result = write(fd, utente.username, SIZE);
				result = write(fd, utente.password, SIZE);
				result = write(fd, key_s, CHIAVE);
				result = write(fd, "\n", 1);
				if (result == -1){ printf("Errore scrittura su file\n"); return -1;}

				close(fd);
				oper.sem_op = 1;				//indico che rilasco il token
				if (semop(id_sem, &oper, 1) == -1) {
					printf("Errore nel rilascio del token semaforico\n");
					return -1;
				}
				//tengo traccia della registrazione (si può omettere)
				printf("Nome utente: %s", utente.nome);
				printf("Cognome utente: %s", utente.cognome);
				printf("Username utente: %s", utente.username);
				printf("Password utente: %s", utente.password);
				printf("Chiave utente: %d\n", utente.chiave);

				array_connessi[utente.chiave] = 1;

				if (write(sock_c, "ok", CHIAVE) == -1) {
					printf("Errore nella notifica di avvenuta registrazione\n");
					return -1;
				}

				eseguita = 0;
				key++;
			} else {
				if (write(sock_c, "no", CHIAVE) == -1) {
					printf("Errore nella notifica di mancata registrazione\n");
					return -1;
				}
			}
		} else {
			if(write(sock_c, "Presente", SIZE) == -1) { return -1;}
		}
	}
	return utente.chiave;
}

int function_Registrato(int ds_socket) {
	int eseguita = 1;
	int chiave;
	struct {
		char username[SIZE];
		char password[SIZE];
	} utente;

	while (eseguita) {

		if (read(ds_socket, &utente.username, SIZE) == -1) {
			printf("Errore read username Login\n");
			return -1;}
		if (read(ds_socket, &utente.password, SIZE) == -1) {
			printf("Errore read pswd Login\n");
			return -1;}

		chiave = verificaLogin(utente.username, utente.password);
		if (chiave == -1){
			if (write(ds_socket, "NonPresente", SIZE) == -1){ printf("Errore riscontro nonPres\n");
				return -1;}
		}else {
			if (write(ds_socket, "ok", SIZE)== -1){ printf("Errore riscontro Login\n"); return -1;};
			if (verificaConn(chiave) == -1) {
				array_connessi[chiave] = 1;
				if(write(ds_socket, "ok", SIZE)==-1){ printf("Errore riscontro nonConn\n"); }
				eseguita = 0;
			} else {
				write(ds_socket, "GIACONNESSO", SIZE);
			}
		}
	}
	array_connessi[chiave] = 1;
	return chiave;
}

int verificaLogin(char* username, char* pass) {
	int fd, chiave;
	char usernames[SIZE], password[SIZE], key_s[CHIAVE];

	fd = open(NOMEFILE, O_RDONLY);
	if (fd == -1){ printf("Errore nell'apertura del file\n"); return -1;}

	lseek(fd, 2 * SIZE, 0);
	if (read(fd, usernames, SIZE) == -1){printf("Errore nella read username\n");return -1;}
	if (read(fd, password, SIZE) == -1){ printf("Errore nella read pswd\n"); return -1;}

	if (strcmp(usernames, username) == 0 && (strcmp(password, pass) == 0)) {
		if (read(fd, key_s, CHIAVE) == -1) { printf("Errore nella lettura della chiave\n"); return -1;}
		else { chiave = atoi(key_s); }
		close(fd);
		return chiave;
	}

	for (int i = 0; i < MAX; i++) {
		if (usernames == NULL || password == NULL){ break;}

		//133-64 INSERIRE COSTANTI
		lseek(fd, 69, 1);
		if (read(fd, usernames, SIZE) == -1) { printf("Errore nella read usrn\n"); return -1; }
		if (read(fd, password, SIZE) == -1) { printf("Errore nella read pswd\n"); return -1; }

		if (strcmp(usernames, username) == 0 && (strcmp(password, pass) == 0)) {
			if (read(fd, key_s, CHIAVE) == -1) { printf("Errore nella lettura della chiave\n"); return -1;}
			else { chiave = atoi(key_s); }
			close(fd);
			return chiave;
		}
	}
	close(fd);
	return -1;
}

int verificaUsername(char* username) {
	int fd, chiave;
	char usernames[SIZE], key_s[CHIAVE];

	fd = open(NOMEFILE, O_RDONLY);
	if (fd == -1) { printf("Errore nell'apertura del file\n"); return -1;}

	lseek(fd, 2 * SIZE, 0);
	if (read(fd, usernames, SIZE) == -1) { printf("Errore nella read\n"); return -1;}

	//se trovo l'username, prelevo la chiave associata
	if ((strcmp(usernames, username)) == 0) {
		lseek(fd, SIZE, 1);
		if (read(fd, key_s, CHIAVE) == -1) { printf("Errore nella lettura della chiave\n"); return -1;}
		else { chiave = atoi(key_s);}
		close(fd);
		return chiave;
	}

	for (int i = 0; i < MAX; i++) {
		if (usernames == NULL) { break;}

		//mi sposto all'interno del file di 133-32 bytes
		lseek(fd, 101, 1);
		if (read(fd, usernames, SIZE) == -1) { printf("Errore nella read\n"); return -1;}

		if (strcmp(usernames, username) == 0) {
			lseek(fd, SIZE, 1);
			if (read(fd, key_s, CHIAVE) == -1) { printf("Errore nella lettura della chiave\n"); return -1; }
			else { chiave = atoi(key_s); }
			close(fd);
			return chiave;
		}
	}
	close(fd);
	return -1;
}

int verificaConn(int chiave) {
	if (array_connessi[chiave] == 1) {
		return 1;
	}
	return -1;
}

int readMsg(int sock_c, int chiave, messaggio msg) {
	//char username[SIZE];
	//int eseguita = 1;
	//int chiave;
	/*while(eseguita){
	 	 if(read(sock_c, username, SIZE) == -1){ printf("Errore nell'acquisizione dell'username\n"); return -1;}
	 	 chiave = verificaUsername(username);
	 	 if(chiave == -1){
	 	 	 if(write(sock_c, "NonPresente", SIZE)==-1){ return -1;}
	 	 }
	 	 else{
	 	 	 if(write(sock_c, "Presente", SIZE)==-1){return -1;}
	 	 }
	 }
	 */
	int size = 0;
	if (array_messaggi[chiave] == 0) {
		strcpy(msg.oggetto, "no");
		if (write(sock_c, (void*) &msg, sizeof(msg)) == -1) {
			printf("Errore nel riscontro di coda vuota\n");
			return -1;
		}
	} else {
		for (int i = 0; i < array_messaggi[chiave]; i++) {

			//ricevo il messaggio in FIFO dalla coda
			size = msgrcv(array_code[chiave], &msg, MESSAGGIO, 0, IPC_NOWAIT);
			if (write(sock_c, (void*) &msg, sizeof(msg)) == -1) {
				printf("Errore invio messaggio\n");
				return -1;
			}

			//una volta prelevati posso rimetterli in coda per poterli rileggere
			if (msgsnd(array_code[chiave], &msg, MESSAGGIO, IPC_NOWAIT) == -1) {
				printf("Errore nel caricamento in coda\n");
				return -1;
			}
		}
		strcpy(msg.oggetto, "fine");
		if (write(sock_c, (void*) &msg, sizeof(msg)) == -1) {
			printf("Errore invio messaggio\n");
			return -1;
		}
	}
	return 1;
}

int sendMsg(int sock_c, messaggio msg) {
	char destinatario[SIZE];
	char sended[CHIAVE];
	int eseguita = 1;
	int inviato, chiave, size;

	while (eseguita) {
		if (read(sock_c, destinatario, SIZE) == -1) { printf("Errore nel destinatario\n"); return -1;}

		chiave = verificaUsername(destinatario);
		if (chiave == -1) {
			if (write(sock_c, "NonPresente", SIZE) == -1) { return -1;}
		}
		else {
			if (write(sock_c, "Presente", SIZE) == -1) { return -1; }
			eseguita = 0;
		}
	}

	if (read(sock_c, sended, CHIAVE) == -1) { printf("Errore nella ricezione conferma\n"); return -1;}
	inviato = atoi(sended);
	if (inviato != 1){ return -1;}

	size = read(sock_c, (void*) &msg, sizeof(msg));
	if (size == -1) { printf("Errore nella lettura del messaggio\n"); return -1;}

	if (msgsnd(array_code[chiave], &msg, MESSAGGIO, IPC_NOWAIT) == -1) {
		if (write(sock_c, "NonInviato", SIZE) == -1) { printf("Errore conferma\n"); return -1; }
		printf("Errore nel caricamento in coda (potrebbe essere piena)\n");
		return -1;
	}

	if (write(sock_c, "inviato", SIZE) == -1) { printf("Errore conferma\n"); return -1;}

	array_messaggi[chiave]++;
	return chiave;
}

int deleteMsg(int sock_c, int ch, messaggio msg) {
	char conferma[CHIAVE];
	int confirm;
	if (read(sock_c, conferma, CHIAVE) == -1) {
		printf("Errore nella lettura della conferma\n");
		return -1;
	}
	confirm = atoi(conferma);
	if (confirm == 1) {
		do {
			confirm = msgrcv(array_code[ch], &msg, MESSAGGIO, 0, IPC_NOWAIT);
		} while (confirm != -1);

		array_messaggi[ch] = 0;
		if (write(sock_c, "ok", CHIAVE) == -1) {
			printf("Errore nella notifica dell'avvenuta cancellazione");
			return -1;
		} else {
			return 1;
		}
	} else {
		return -1;
	}
}

int InitArrayMsg(int ds_coda) {
	int numMessaggi;
	if (msgctl(ds_coda, IPC_STAT, &structure) == -1) {
		termina("Errore nell'inizializzazione dei messaggi\n");
	}
	numMessaggi = structure.msg_qnum;
	return numMessaggi;
}

int creazioneCode(int chiave) {
	int ds_coda;
	ds_coda = msgget(chiave, IPC_CREAT | 0660);
	if (ds_coda == -1) { termina("Errore nella creazione delle code degli utenti registrati\n"); }
	return ds_coda;
}

int eliminazioneCode(int ds_coda) {
	if (msgctl(ds_coda, IPC_RMID, NULL) == -1) {
		termina("Errore nella eliminazione\n");
		return -1;
	}
	return 1;
}

int Inizializzazione() {
	//int ris = remove(NOMEFILE);
	//if(ris == -1){ termina("Errore nella rimozione del file esistente\n");}
	int fd;
	char key_s[CHIAVE];
	long chiave_sem = 0;
	id_sem = semget(chiave_sem, 1, IPC_CREAT | IPC_EXCL | 0666);
	if (id_sem == -1){ termina("Errore nella chiamata semget");}

	if (semctl(id_sem, 0, SETVAL, 1) == -1){ termina("Errore nella chiamata semctl");}

	fd = open(NOMEFILE, O_CREAT, 0600);
	if (fd == -1) { termina("Errore nella creazione/apertura del file\n");}

	//leggo il valore dell'ultima chiave e incremento di uno
	lseek(fd, -5, 2);
	if (read(fd, key_s, CHIAVE) == -1) { printf("Errore nella lettura della chiave\n"); return -1; }
	else {
		key = atoi(key_s);
		key = (key + 1) % MAX;
	}

	for (int i = 1; i < key; i++) {
		array_connessi[i] = 0;
		array_code[i] = creazioneCode(i);
		array_messaggi[i] = InitArrayMsg(array_code[i]);
	}
	close(fd);
	return 1;
}

int main() {

	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.__sigaction_u.__sa_handler = gestione_Int;
	if (sigaction(SIGINT, &sa, NULL) == -1) { termina("Errore nella gestione di SIGINT\n");}
	if (sigaction(SIGHUP, &sa, NULL) == -1) { termina("Errore nella gestione di SIGHUP\n");}
	if (sigaction(SIGQUIT, &sa, NULL) == -1){ termina("Errore nella gestione di SIGQUIT\n");}
	if (sigaction(SIGTERM, &sa, NULL) == -1){ termina("Errore nella gestione di SIGTERM\n");}
	if (sigaction(SIGUSR1, &sa, NULL) == -1){ termina("Errore nella gestione di SIGUSR1\n");}
	sa.__sigaction_u.__sa_handler = gestione_Ill;
	if (sigaction(SIGILL, &sa, NULL) == -1){  termina("Errore nella gestione di SIGILL\n");}
	if (sigaction(SIGSEGV, &sa, NULL) == -1){ termina("Errore nella gestione di SIGSEGV\n");}
	sa.__sigaction_u.__sa_handler = SIG_IGN;
	if (sigaction(SIGPIPE, &sa, NULL) == -1){termina("Errore nella gestione di SIGPIPE\n");}

	if (Inizializzazione() == -1) {
		termina("Errore nella fase di inizializzazione dei dati\n");
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == -1) {
		termina("Errore nella creazione del socket");
	}

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(25000);
	my_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket, &my_addr, sizeof(my_addr)) == -1) {
		close(serverSocket);
		termina("Errore nel binding\n");
	}

	if (listen(serverSocket, BACKLOG) == -1) {
		close(serverSocket);
		termina("Errore nella trasformazione in listening\n");
	}
	while (1) {
		pthread_t tid;
		socketAccept = accept(serverSocket, &addrChiamante, &addr);
		if (socketAccept == -1) { printf("Errore nell'accettazione del client"); }
		else{
			//creiamo un thread per ogni connessione, associato ad un socketAccept
			if (pthread_create(&tid, NULL, function_id, NULL)) {
				printf("Errore nella creazione del thread per il client");
			}
		}
	}
}

