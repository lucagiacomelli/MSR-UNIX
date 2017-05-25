#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#define BACKLOG 100
#define TAGLIAT 256
#define SIZE 32
#define MESSAGGIO 484
#define FLAG 4


int clientSocket;
struct sockaddr_in addrClient;
struct hostent *hp;
char new[FLAG];				//legge da stdin se l'utente è nuovo o meno
int nuovo;							//verifichiamo se l'utente è nuovo o meno
char command[FLAG];		//prendiamo in input il comando dal client
int comando;						//traduzione del comando in intero
int size;
char risposta[SIZE];
struct sigaction sa;	// la maschera di sigaction mi permette di bloccare i segnali durante l'esecuzione
								//dell'handler al fine di avere una gestione affidabile dei segnali

typedef struct{
	char mittene [SIZE];
	char destinatario [SIZE];
	char oggetto [2*SIZE];
	char testo[TAGLIAT];
}messaggio;

struct{
	char nome [SIZE];
	char cognome[SIZE];
	char username[SIZE];
	char password[SIZE];
	char verificaPass [SIZE];
}utente;

void termina(char* s){
	printf("%s", s);
	close(clientSocket);
	exit(0);
}

void gestione_Int(){
	termina("\nClient terminato\n");
}

void gestione_SIGP(){
	termina("\n----SERVER DISCONNESSO-----\n");
}

int scriviStringaSenzaNumeri(char* s, char input[], int tagliaMax){
	int eseguita =1;
	char tampone[SIZE];
	while (eseguita) {
		strcpy(input, "");
		printf("%s\n", s);
		char* line=NULL ;
		size_t count=0;
		int length;
		length=getline(&line ,&count , stdin);
		if(strcmp(line,"\n")==0){ printf("-----Inserire almeno un carattere-----\n");}
		else{
			if (length<tagliaMax && length>0) {
				strcpy(tampone, line);
				tampone[length]= '\0';
				eseguita = 0;
				if(tampone[0] == ' ' | tampone[0] == '	'){
					printf("-----Non può iniziare con SPACE o TAB-----\n"); eseguita =1;}
				for(int i=0; i<length; i++){
					if(	tampone[i] == '0'| tampone[i] == '1' | tampone[i] == '2' |
						tampone[i] == '3'| tampone[i] == '4' | tampone[i] == '5' |
						tampone[i] == '6'| tampone[i] == '7' | tampone[i] == '8' |
						tampone[i] == '9'| tampone[i] == ',' | tampone[i] == '.' |
						tampone[i] == ';'| tampone[i] == ':' | tampone[i] == '<' |
						tampone[i] == '_'| tampone[i] == '?' | tampone[i] == '!' |
						tampone[i] == '#'| tampone[i] == '@' | tampone[i] == '*' |
						tampone[i] == '+'| tampone[i] == '[' | tampone[i] == ']' |
						tampone[i] == '^'| tampone[i] == '(' | tampone[i] == ')' |
						tampone[i] == '{'| tampone[i] == '}' | tampone[i] == '&' |
						tampone[i] == '$'| tampone[i] == '%' | tampone[i] == '|' |
						tampone[i] == '=' | tampone[i] == '/'| tampone[i] == '-' ){
						printf("Si accettano solo caratteri\n");
						eseguita = 1;
						break;
					}
				}
			}
			else{ printf("-----Taglia massima superata-----\n");}
		}
	}
	strcpy(input, tampone);
	return 1;
}

int scriviStringa(char* s, char input[], int tagliaMax){

	while (1) {
		strcpy(input,"");
		printf("%s\n", s);
		char* line=NULL ;
		size_t count=0;
		int length;
		length=getline(&line ,&count , stdin);
		if(strcmp(line,"\n")==0){ printf("----inserire almeno un carattere----\n");}
		else{
			if (length<tagliaMax && length>0) {
				strcpy(input, line);
				input[length]= '\0';
				return 1;
			}
			else{ printf("----Taglia massima superata----\n");}
		}
	}
	return -1;
}


char* login(){
	printf("--------------LOGIN--------------\n");
	do{
		if(scriviStringa("Inserire username\n", utente.username, SIZE) != 1){return NULL;}

		do{
			size = write(clientSocket, &utente.username, SIZE);
			if(size == -1){ printf("Errore nella scrittura del nome\n"); return NULL;}
		}while(size < SIZE);

		if(scriviStringa("Inserire password\n", utente.password, SIZE) != 1){ return NULL;}

		size = write(clientSocket, &utente.password, SIZE);
		if(size == -1){ printf("Errore nella scrittura della pswd\n"); return NULL;}
		if(read(clientSocket, &risposta, SIZE)== -1){
			printf("Errore lettura della risposta\n");
			return NULL;}

		if(strcmp(risposta, "ok") != 0){
			printf("Username o password scorretti\n");
		}
		else{
			if(read(clientSocket, &risposta, SIZE)==-1){ printf("Errore nella verifica\n"); return NULL;}
			if(strcmp(risposta, "ok") != 0){
				printf("Sei già connesso con un altro dispositivo\n");
			}else{
				printf("\n-------Utente connesso-------\n");
				printf("Username: %s\n", utente.username);
			}
		}
	}while(strcmp(risposta,"ok") != 0);
	return utente.username;
}

char* nuovoUtente(){
	int ok = 1, confirm;
	char conferma[FLAG];
	printf("----------REGISTRAZIONE NUOVO UTENTE----------\n");
	do{
		if(scriviStringaSenzaNumeri("Inserire NOME massimo 32 caratteri\n",
				utente.nome, SIZE) != 1){
			return NULL;}
		if(scriviStringaSenzaNumeri("Inserire COGNOME massimo 32 caratteri\n",
				utente.cognome, SIZE) != 1){
			return NULL;}
		if(scriviStringa("Inserire USERNAME massimo 32 caratteri\n",
				utente.username, SIZE) != 1){
			return NULL;}

		do{
			size = write(clientSocket, &utente.nome, SIZE);
			if(size == -1){	printf("Errore nella scrittura del nome\n"); return NULL; }
		}while(size < SIZE);

		do{
			size = write(clientSocket, &utente.cognome, SIZE);
			if(size == -1){	printf("Errore nella scrittura del cognome\n"); return NULL;}
		}while(size < SIZE);

		size = write(clientSocket, &utente.username, SIZE);
		if(size == -1){ printf("Errore nella scrittura dell'username\n"); return NULL; }

		if(read(clientSocket, &risposta, SIZE)== -1){
			printf("Errore nella lettura della risposta\n");
			return NULL;
		}

		if(strcmp(risposta, "Presente") == 0){
			printf("Username già presente in archivio!\n");
			confirm = 0;
		}
		else{
			do{
				if(scriviStringa("Inserire PASSWORD massimo 32 caratteri\n",
						utente.password, SIZE) != 1){ return NULL;}
				if(scriviStringa("Confermare PASSWORD massimo 32 caratteri\n",
						utente.verificaPass, SIZE) != 1){ return NULL;}
				if(strcmp(utente.verificaPass, utente.password) == 0){
					do{
						size = write(clientSocket, &utente.password, SIZE);
						if(size == -1){	printf("Errore nella scrittura della password\n");}
					}while(size < SIZE);
					ok = 0;

					printf("---------RIEPILOGO REGISTRAZIONE---------\n");
					printf("NOME: %s\n", utente.nome);
					printf("COGNOME: %s\n", utente.cognome);
					printf("USERNAME: %s\n", utente.username);
					printf("PASSWORD: %s\n", utente.password);
					if(scriviStringa("Confermare la registrazione?\n"
									"[1: Sì, Altro: no]\n", conferma, FLAG) != 1){ return NULL;}
					confirm = atoi(conferma);

					if(write(clientSocket,conferma,FLAG)== -1){
						printf("Errore nell'invio della conferma registrazione");
						return NULL;
					}
					if(read(clientSocket,conferma,FLAG) == -1){
						printf("Errore nella ricezione dell'avvenuta registrazione\n");
					}
					if(strcmp(conferma,"ok") != 0){
						return NULL;
					}
				}
				else{
					printf("Hai inserito due password diverse!\n");
				}
			}while(ok);
		}
	}while(confirm != 1);
	printf("\n--------UTENTE REGISTRATO--------\n");
	printf("Username: %s\n", utente.username);

	return utente.username;
}

int read_msg(){
	//char username[SIZE];
	//int eseguita = 1;
	messaggio msg;
	printf("Connesso: %s\n", utente.username);
	printf("-------------LETTURA MESSAGGI------------\n");

	/*while(eseguita){
		if(scriviStringaSenzaNumeri("Inserire Username di cui si vogliono conoscere i messaggi\n", username, SIZE) != 1){
			return -1;
		};
		if(write(clientSocket, username, SIZE)==-1){ printf("Errore invio username\n"); return -1;}

		if(read(clientSocket, &risposta, SIZE)==-1){
			printf("Errore ricezione risposta per correttezza username\n");
			return -1;
		}
		if(strcmp(risposta, "Presente") == 0){
			eseguita = 0;
		}
		else{
			printf("Username non presente nel sistema!\n");
		}
	}*/
	int size = 0;
	int count = 1;
	while(size != -1){
		size = read(clientSocket, (void*)&msg, sizeof(msg));
		if(size ==- 1){ printf("-----Errore in lettura del messaggio-----\n"); return -1;}
		if(strcmp(msg.oggetto, "no")== 0){
			printf("\n------NESSUN MESSAGGIO PRESENTE-----\n"); break;}
		if(strcmp(msg.oggetto, "fine") == 0){ break;}
		else{
			printf("\n----------- MESSAGGIO NUMERO %d -----------\n", count);
			printf("MITTENTE: %s\n", msg.mittene);
			printf("OGGETTO: %s\n", msg.oggetto);
			printf("TESTO: %s\n", msg.testo);
			count++;
		}
	}
	return 1;
}

int send_msg(char * mitt){
	messaggio msg;
	int eseguita = 1;
	char invio[FLAG];
	int send;
	printf("Connesso: %s\n", utente.username);
	printf("---------------SCRITTURA MESSAGGIO--------------\n");
	printf("Premere CTRL+C per terminare\n");

	while(eseguita){
		strcpy(msg.mittene, mitt);
		if(scriviStringa("Inserire DESTINATARIO\n", msg.destinatario, SIZE) != 1){
			return -1;
		}
		if(write(clientSocket, msg.destinatario, SIZE)==-1){
			printf("Errore invio username\n");
			return -1;}
		if(read(clientSocket, &risposta, SIZE)==-1){
			printf("Errore ricezione risposta per correttezza username\n");
			return -1;
		}
		if(strcmp(risposta, "Presente") == 0){
			eseguita = 0;

			if(scriviStringa("Inserire OGGETTO massimo 64 caratteri\n", msg.oggetto, 2*SIZE) != 1){
				return -1;
			}
			if(scriviStringa("Inserire TESTO massimo 256 caratteri\n", msg.testo, TAGLIAT) != 1){
				return -1;
			}
			printf("--------------RIEPILOGO MESSAGGIO-------------\n\n");
			printf("--------------------MITTENTE-------------------\n%s\n", msg.mittene);
			printf("------------------DESTINATARIO----------------\n%s\n", msg.destinatario);
			printf("--------------------OGGETTO-------------------\n%s\n", msg.oggetto);
			printf("---------------------TESTO--------------------\n%s\n\n", msg.testo);

			if(scriviStringa("		INVIARE MESSAGGIO?		\n"
							"[Immettere: 1:SI, altro: NO]\n", invio, FLAG) != 1){return -1;}
			send = atoi(invio);
			if(write(clientSocket, invio, FLAG)==-1){printf("Errore nell'invio della risposta\n");
				return -1;};
			if(send == 1){
				if(write(clientSocket, (void*)&msg, sizeof(msg))==-1){ printf("Errore invio messaggio\n");
				return -1;}
				if(read(clientSocket, risposta, SIZE)==-1){ printf("Errore nella conferma di invio\n");
				return -1;}
				if(strcmp(risposta, "inviato") != 0){
					return -1;
				}
				printf("--------MESSAGGIO INVIATO CORRETTAMENTE--------\n\n");

			}else{
				return -1;
			}
		}
		else{
			printf("Messaggio cancellato, riprovare! (il destinatario potrebbe non esistere)\n");
		}

	}
	return 1;
}

int delete_msg(){
	char confirm[FLAG];
	int conferma;
	printf("Connesso: %s\n", utente.username);
	printf("------------ELIMINAZIONE MESSAGGI------------\n");

	if(scriviStringa("Eliminare tutti i messaggi?\n[1: Sì; Altro: NO]\n", confirm, FLAG)==-1){
		return -1;}
	if(write(clientSocket, confirm, FLAG)== -1){ printf("Errore nell'invio della conferma\n");
		return -1;}
	conferma = atoi(confirm);
	if(conferma == 1){
		if(read(clientSocket, confirm, FLAG)==-1){
			printf("Errore nella lettura della conferma di eliminazione\n");
			return -1;}
		if(strcmp(confirm, "ok") == 0){
			printf("------------ELIMINAZIONE AVVENUTA CON SUCCESSO----------\n");
			return 1;
		}
		else{ return -1;}
	}
	else{
		return -1;
	}
}

/* Posso aggiugere gli argomenti al main IPNumber e NumeroPorta
 * Non li metto perchè per default il client è configurato per
 * comunicare con il server in locale e con numero di porta 25000
 */
int main(){

	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.__sigaction_u.__sa_handler = gestione_Int;
	if(sigaction(SIGINT, &sa, NULL) == -1){ termina("Errore nella gestione di SIGINT\n");}
	if(sigaction(SIGHUP, &sa, NULL) == -1){ termina("Errore nella gestione di SIGHUP\n");}
	if(sigaction(SIGQUIT, &sa, NULL) == -1){ termina("Errore nella gestione di SIGQUIT\n");}
	if(sigaction(SIGTERM, &sa, NULL) == -1){ termina("Errore nella gestione di SIGTERM\n");}
	sa.__sigaction_u.__sa_handler = gestione_SIGP;
	if(sigaction(SIGPIPE, &sa, NULL) == -1){ termina("Errore nella gestione di SIGPIPE\n");}

	addrClient.sin_family = AF_INET;
	addrClient.sin_port = htons(25000);

	hp = gethostbyname("127.0.0.1");
	memcpy(&addrClient.sin_addr,hp->h_addr,4);

	clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(clientSocket == -1){ termina("Errore nella creazione del ClientSocket\n");}

	if(connect(clientSocket, &addrClient, sizeof(addrClient))==-1){ termina("Errore nella connect\n");}
	printf("Connessione stabilita\n");

	char flag[2];

	if(read(clientSocket, flag, 2) == -1){
		if(errno == EPIPE){ gestione_SIGP();}
		termina("Errore nell'ammissione, riconnettersi!\n");
	}
	if(strcmp(flag, "si") == 0){
		printf("ARCHIVIO SATURO!, Non è più possibile registrarsi\n");
	}

	if(scriviStringa("NUOVO UTENTE?\n[1:sì, altro: no]\n"
							"Premere CTRL+C per terminare\n",new,4) != 1){
		return -1;}
	nuovo = atoi(new);

	if(write(clientSocket, &nuovo, FLAG)==-1){
		if(errno == EPIPE){ gestione_SIGP();}
		termina("Errore nell'invio del flag utente, riconnettersi!\n");
	}

	char * username = NULL;
	if(nuovo == 1){
		if(strcmp(flag, "si") == 0){ termina("ARCHIVIO SATURO!, Non è più possibile registrarsi\n");}
		else{
			username = nuovoUtente();
			if(username == NULL){ termina("Errore nella registrazione, riconnettersi!\n");}
		}
	}
	else{
		username = login();
		if(username == NULL){ termina("Errore nel login, riconnettersi!\n");}

	}

	while(1){
		char ok[FLAG];
		if(scriviStringa("Cosa si desidera fare?\n"
				"Immetti cifra:\n"
				"[ 1:LEGGERE i messaggi\n"
				"2:SPEDIRE un nuovo messaggio ad uno qualunque degli utenti del sistema;\n"
				"3:CANCELLARE i messaggi]\n"
				"Premere CTRL+C per terminare\n",command,FLAG) != 1){
			termina("Errore inserimento\n");}
		comando = atoi(command);
		if(write(clientSocket, &comando, FLAG)== -1){
			termina("Errore nell'invio del comando\n");
		}
		if(read(clientSocket, ok,FLAG) == -1){
			termina("Erroe nel riscontro del comando\n");
		}

		if(strcmp(ok, "ok") == 0){
			switch(comando){
			case 1:
				if(read_msg() != 1){
					printf("\n-----------LETTURA NON ESEGUITA----------!\n");
				}
				break;
			case 2:
				if(send_msg(username) != 1){
					printf("\n---------INVIO NON ESEGUITO CORRETTAMENTE----------\n"
							"----------(La coda potrebbe essere piena)-----------\n");
				}
				break;
			case 3:
				if(delete_msg() != 1){
					printf("\n----------ELIMINAZIONE NON AVVENUTA----------\n");
				}
				break;
			default:
				printf("\nCOMANDO NON VALIDO, RIPROVARE!\n\n");
			}
		}
		else{
			printf("\nCOMANDO NON RISCONTRATO, RIPROVARE\n");
		}
	}
	close(clientSocket),
	exit(0);
}

