//////////////////////////////////////////////////////////////////////
// File Name : proxy_cache.c 					     //
// Date : 2021/03/30   					 	     //
// Os : Ubuntu 16.04 LTS 64bits  				     //
// Author : Hyun Yerim  					     //
// Student ID : 2019202078 					     //
// ----------------------------------------------------------------- //
// Title : System Programming Assignment #1-1 (proxy server) 	     //
// Description : For input url, make hash url and create directory   // 
//		and file of it at ~/cache directory  		     //
///////////////////////////////////////////////////////////////////////

#include <stdio.h>		
#include <string.h>		
#include <openssl/sha.h>	
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>

#define BUFFSIZE	1024*1024
#define PORTNO		40000

FILE *fp;
time_t start, end;
struct tm* START, * END;
int startSec = 0, endSec = 0;
struct tm* PCSSTART, * PCSEND;
int pcsStartSec = 0, pcsEndSec = 0;
int subprocess = 0;
int semid, i;
union semun{
	int val;
	struct semid_ds *buf;
	unsigned short int* array;
} arg;

typedef struct logFileInfo {
	int semid;
	int index;
	char* url;
	char* dir_name;
	char* file_name;
	struct tm* tp;
} LOG;

///////////////////////////////////////////////////////////////////////
// p								     //
// ================================================================= //
// Input: 				 		     	     //
// 			   					     //
// Output:				 			     //
// 						 		     //
// Purpose:	make semaphore to enter critical section 	     //
///////////////////////////////////////////////////////////////////////
void p(int semid)
{	
	struct sembuf pbuf;
	pbuf.sem_num = 0;
	pbuf.sem_op = -1;
	pbuf.sem_flg = SEM_UNDO;   // make semaphore to enter critical section
	if((semop(semid, &pbuf, 1)) == -1)
	{
		perror("p : semop failed");
		exit(1);
	}
}

///////////////////////////////////////////////////////////////////////
// v								     //
// ================================================================= //
// Input: 				 		     	     //
// 			   					     //
// Output:				 			     //
// 						 		     //
// Purpose:	return semaphore after using critical section 	     //
///////////////////////////////////////////////////////////////////////
void v(int semid)
{
	struct sembuf vbuf;
	vbuf.sem_num = 0;
	vbuf.sem_op = 1;
	vbuf.sem_flg = SEM_UNDO;   // delete semaphore
	if((semop(semid, &vbuf, 1)) == -1)
	{
		perror("v : semop failed");
		exit(1);
	}
}

///////////////////////////////////////////////////////////////////////
// writeToLog							     //
// ================================================================= //
// Input: 				 		     	     //
// 			   					     //
// Output:				 			     //
// 						 		     //
// Purpose:	critical section(write logfile.txt)	 	     //
///////////////////////////////////////////////////////////////////////	
void *writeToLog(void *info)
{	
	LOG *INFO = (LOG *)info;   // change parmeter struct to LOG struct value
	printf("*PID# %d is waiting for the semaphore.\n",getpid());   // print waiting message	
	p(INFO->semid);   // make semaphore
	sleep(rand()%10);   // wait to make synchronize
	printf("*PID# %d is in the critical zone.\n",getpid());   // print enter message   
	printf("*PID# %d create the *TID# %lu.\n", getpid(), pthread_self());   // print thread ID
	if(INFO->index == 1)   // if MISS
	{
		fprintf(fp, "[MISS] ServerPID : %d | %s-[%d/%02d/%02d, %02d:%02d:%02d]\n", getpid(), INFO->url, 1900+(INFO->tp->tm_year), 1+(INFO->tp->tm_mon), INFO->tp->tm_mday, INFO->tp->tm_hour, INFO->tp->tm_min, INFO->tp->tm_sec);   // write logfile.txt MISS
	}
	else if(INFO->index == 0)   // if HIT
	{
		fprintf(fp, "[HIT] ServerPID : %d | %s/%s-[%d/%02d/%02d, %02d:%02d:%02d]\n", getpid(), INFO->dir_name, INFO->file_name, 1900+(INFO->tp->tm_year), 1+(INFO->tp->tm_mon), INFO->tp->tm_mday, INFO->tp->tm_hour, INFO->tp->tm_min, INFO->tp->tm_sec);
		fprintf(fp, "[HIT]%s\n", INFO->url);   // write logfile.txt HIT
	}	
	fclose(fp);   // write logfile.txt
	printf("*TID# %lu is exited.\n", pthread_self());   // print thread exit message
	printf("*PID# %d exied the critical zone.\n",getpid());   // print exit critical section message
	v(INFO->semid);   // delete semaphore
}

///////////////////////////////////////////////////////////////////////
// handler3							     //
// ================================================================= //
// Input: 				 		     	     //
// 			   					     //
// Output:				 			     //
// 						 		     //
// Purpose:	wait server terminate signal and write logfile 	     //
///////////////////////////////////////////////////////////////////////
void handler3()
{	
	time(&end);   // call time function
	PCSEND = localtime(&end);
	pcsEndSec = mktime(PCSEND);   // store end time
	fprintf(fp, "**Server** [Terminated] run time: %d sec. #sub process : %d\n", (pcsEndSec-pcsStartSec), subprocess);   // write server terminate to logfile
	fclose(fp);
	exit(0);
}

///////////////////////////////////////////////////////////////////////
// getIPAddr							     //
// ================================================================= //
// Input: 	addr -> url name	 		     	     //
// 			   					     //
// Output:	haddr -> url host's IP address 			     //
// 						 		     //
// Purpose: 	judge url host name can changed to IP Address with   //
//		network connection				     //
///////////////////////////////////////////////////////////////////////
char *getIPAddr(char *addr)
{
    	struct hostent* hent;
	char * haddr;
	int len = strlen(addr);
	if ( (hent = (struct hostent*)gethostbyname(addr)) != NULL)
		haddr=inet_ntoa(*((struct in_addr*)hent->h_addr_list[0]));   // get IP Address of host
	return haddr;
}

///////////////////////////////////////////////////////////////////////
// handler2							     //
// ================================================================= //
// Input: 	 		     				     //
// 			   					     //
// Output: 							     //
// 						 		     //
// Purpose: 	wait alarm signal				     //
///////////////////////////////////////////////////////////////////////
static void handler2()
{
	write(STDOUT_FILENO, "===============NO RESPONSE==============\n", sizeof("===============no response==============\n"));   // print no response error message
	exit(0);   // exit child process
}

///////////////////////////////////////////////////////////////////////
// handler							     //
// ================================================================= //
// Input: 	 		     				     //
// 			   					     //
// Output: 	pid 						     //
// 						 		     //
// Purpose: 	wait any child process's termination		     //
///////////////////////////////////////////////////////////////////////
static void handler()
{
	pid_t pid;
	int status;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0);
}

///////////////////////////////////////////////////////////////////////
// MKDIR 							     //
// ================================================================= //
// Input: 	char* dir -> new directory name 		     //
// 			   					     //
// Output: 	void 						     //
// 						 		     //
// Purpose: 	Make new directory				     //
///////////////////////////////////////////////////////////////////////
void MKDIR(char* dir)
{
	umask(000);   // permission initialize
	mkdir(dir, S_IRWXU | S_IRWXG | S_IRWXO);
}

///////////////////////////////////////////////////////////////////////
// sha1_hash							     //
// ================================================================= //
// Input: 	char* input_url -> initial input url 		     //
// 		char* hashed_url -> string to store hashed url 	     //
// 								     //
// Output: 	char* hashed_url -> hashed url			     //
//		unsigned char hashed_160bits			     //
//		-> hashed 160 bits data of input url text 	     //
//		hashed_hex 					     //
//		-> transmitted hexadecimal of hashed 160 bits	     //
//								     //
// Purpose: 	Make hashed url					     //
///////////////////////////////////////////////////////////////////////
char* sha1_hash(char* input_url, char* hashed_url)
{
	unsigned char hashed_160bits[20];
	char hashed_hex[41];
	int i;

	SHA1(input_url, strlen(input_url), hashed_160bits);

	for (int i = 0; i < sizeof(hashed_160bits); i++)
		sprintf(hashed_hex + i * 2, "%02x", hashed_160bits[i]);

	strcpy(hashed_url, hashed_hex);

	return hashed_url;
}

///////////////////////////////////////////////////////////////////////
// getHomeDir 							     //
// ================================================================= //
// Input: 	char* home -> string to store home directory path    //
// 							 	     //
// Output: 	char* home -> string storing home directory path     //
// 						 		     //
// Purpose: 	Get home directory				     //
///////////////////////////////////////////////////////////////////////
char* getHomeDir(char* home)
{
	struct passwd* usr_info = getpwuid(getuid());
	strcpy(home, usr_info->pw_dir);

	return home;
}

int main()
{
	char cmd[1000];
	int status;
	struct sockaddr_in server_addr, client_addr, web_addr;
	int socket_fd, client_fd, web_fd;
	int len, len_out;
	int state;
	int hORm;
	char buf[BUFFSIZE];
	int hit = 0, miss = 0;
	char bf[BUFFSIZE];
	char hashed_url[1000];
	char dir_name[3];
	char file_name[100];
	char home[1000];
	char working[1000];
	DIR* pDir;
	struct dirent* pFile = NULL;
	int findFile = 0;
	time_t now, start, end;
	struct tm* tp;
	pid_t pid;
	int index = 0;
	FILE* missFile;
	FILE* hitFile;
	int err;
	void *tret;
	pthread_t tid;
	LOG *info = (LOG *)malloc(sizeof(LOG));

	time(&start);   // call 'time' function
	PCSSTART = localtime(&start);
	pcsStartSec = mktime(PCSSTART);   // store start time

	//////////////////////// Make cache directory //////////////////////
	getHomeDir(home);   // get home directory
	chdir(home);   // change working directory 'home' to make cache directory
	if (opendir("cache") == NULL)   // make directory 'cache' using function 'MKDIR' to store URL's cache dir, file
		MKDIR("cache");
	//////////////////////// Make logfile directory and make log.txt file //////////////////////
	if (opendir("logfile") == NULL)
		MKDIR("logfile");
	chdir("logfile");   // change woring directory 'logfile' to make 'log.txt' file	

	fp = fopen("logfile.txt", "w");   // open 'logfile.txt' to write log

	if (fp == NULL)   // if open fail, print error message and return
	{
		printf("'logfile.txt' open error\n");
		return -1;
	}

	if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)   // set socket of server
	{
		printf("Server : can't open stream socket\n");
		return 0;
	}

	bzero((char*)&server_addr, sizeof(server_addr));   // clear server addr struct
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORTNO);
	// set socket information

	if (bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)   // bind socket with socket struct
	{
		printf("Server : can't bind local address\n");
		close(socket_fd);
		return 0;
	}

	if((semid = semget((key_t)PORTNO, 1, IPC_CREAT|0666)) == -1)   // get semphore key
	{
		perror("semget failed");
		exit(1);
	}
	arg.val = 1;
	if((semctl(semid, 0, SETVAL, arg)) == -1)   // call semaphore control function
	{
		perror("semctl failed");
		exit(1);
	}

	listen(socket_fd, 10);   // wait for client connection
	signal(SIGCHLD, (void*)handler);   // wait child process's signal
	signal(SIGALRM, (void*)handler2);   // wait no response alarm
	signal(SIGINT, (void*)handler3);   // wait terminate signal

	while (1)
	{
		struct in_addr inet_client_address;
		struct hostent *host;

		char buf[BUFFSIZE];
		char resBuf[BUFFSIZE];

		char response_header[BUFFSIZE] = { 0, };
		char response_message[BUFFSIZE] = { 0, };

		char tmp[BUFFSIZE] = { 0, };
		char method[20] = { 0, };
		char url[BUFFSIZE] = { 0, };

		char* tok = NULL;
		char hitormiss[20] = { 0, };

		int len2 = 0;
		int CHECK = 0;

		len = sizeof(client_addr);
		client_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &len);   // server accept client's access

		if (client_fd < 0)
		{
			printf("Server : accept failed\n");
			close(socket_fd);
			return 0;
		}

		inet_client_address.s_addr = client_addr.sin_addr.s_addr;   // get client IP address

		//printf("[%s : %d] client was connected\n", inet_ntoa(inet_client_address), client_addr.sin_port);   // print connect success message
		pid = fork();   // make child process

		if (pid == -1)
		{
			close(client_fd);
			close(socket_fd);
			continue;
		}

		subprocess++;
		if (pid == 0)   // child process
		{
			char temp[BUFFSIZE] = {0, };

			hit = 0;
			miss = 0;   // clear hit and miss count
			bzero(buf, sizeof(buf));   // clear buf		
			read(client_fd, buf, BUFFSIZE);   // read client request
			strcpy(tmp, buf);
			tok = strtok(tmp, " ");
			strcpy(method, tok);
			if (strcmp(method, "GET") == 0)   // GET message
			{
				tok = strtok(NULL, " ");
				strcpy(url, tok);   // get url from request message
				for(int i=0; i<strlen(url)+1; i++)
				{
					temp[i] = url[7+i];
				}
				temp[strlen(temp)-1] = 0;
				if(url[strlen(url)-1] != '/')   // no used url
				{
					alarm(0);
					exit(0);
					CHECK = 1;
				}
				else
				{

					alarm(20);   // set timer
					
					host = gethostbyname(temp);
				
					char* IPAddr;

					if( host == NULL )   // call functino to get IP address and check network connected 
					{
						while(1)
						{
							if( (gethostbyname(temp)) == NULL )   // check network disconnected
							{
								sleep(2);
								continue;
							}
						}
					}

					alarm(0);

					getHomeDir(home);
					chdir(home);
					//////////////////////// make hasehd url //////////////////////
					time(&now);
					tp = localtime(&now);   // store time when receiving URL
					findFile = 0;   // to check same file when make hashed file
					sha1_hash(url, hashed_url);   // call 'sha1_hash' function to make hashed url	
					strncpy(dir_name, hashed_url, 3);   // get directory name
					for (int i = 3; i < strlen(hashed_url); i++)
						file_name[i - 3] = hashed_url[i];   // get file name and store to new string
					//////////////////////// make hashed url directory //////////////////////
					getcwd(working, 1000);
					strcat(working, "/cache");   // working directory path = ~/cache
					if (chdir(working))    // go to 'cache'
					{
						printf("'~/cache' working directory change error\n");
						return -1;
					}   // change working directory and check ~/cache directory is exist
					if (opendir(dir_name) == NULL)   // if hased url directory not exist, make new directory
						MKDIR(dir_name);
					//////////////////////// make hashed url file ////////////////////// 
					pDir = opendir(dir_name);   // open url directory
					for (pFile = readdir(pDir); pFile; pFile = readdir(pDir))
					{
						if (!strcmp(pFile->d_name, file_name))
							findFile = 1;
					}   // check hashed url directory has same hashed file already
					closedir(pDir);
					if (!findFile)   // if no same file and hashed url directory exist, go to hashed directory and create new hashed file (MISS)
					{
						//////////////////////// open logfile, cache file and write contents (MISS) //////////////////////
						if (chdir(dir_name))
						{
							printf("'~/cache/dir_name' working directory change error\n");
							return -1;
						}
						else
							creat(file_name, 0644);   // create cache file

						strcat(working, "/");
						strcat(working, dir_name);
						chdir(working);   // change working directory to ~/cache/dir_name

						missFile = fopen(file_name, "wb");   // open file to store http response
					
						if(missFile == NULL)
							printf("MISS FILE NULL!\n");
						
						index = 1;
						info->semid = semid;
						info->index = 1;
						info->url = url;
						info->dir_name = dir_name;
						info->file_name = file_name;
						info->tp = tp;   // set struct field to pass parmeters to writeToLog function with thread

						err = pthread_create(&tid, NULL, writeToLog, (void*)info);   // create thread and start function

						if(err != 0) {
							printf("pthread_create() error/\n");
							return 0;
						}

						pthread_join(tid, &tret);   // terminate thread
					}
					else   // if same file exist, print error message (HIT)
					{
						//////////////////////// open logfile and write, open cache file and read (HIT) //////////////////////
						index = 0;
						info->semid = semid;
						info->index = 0;
						info->url = url;
						info->dir_name = dir_name;
						info->file_name = file_name;
						info->tp = tp;   // set struct field to pass parmeters to writeToLog function with thread

						err = pthread_create(&tid, NULL, writeToLog, (void*)info);   // create thread and start function

						if(err != 0) {
							printf("pthread_create() error/\n");
							return 0;
						}

						pthread_join(tid, &tret);   // terminate thread

						strcat(working, "/");
						strcat(working, dir_name);
						chdir(working);   // change working directory ~/cache/dir_name

						hitFile = fopen(file_name, "rb");   // open cache file to read
						
						if(hitFile == NULL)
							printf("HIT FILE NULL!\n");
					}
				}

				if (index == -1)   // HIT or MISS failed
					return -1;
				else if (index == 0)   // HIT
				{
					while((len2 = fread(resBuf, 1, BUFFSIZE, hitFile)) > 0)   // read cache file's content
					{
						if(len2 == 0)
							break;
						send(client_fd, resBuf, len2, 0);   // send it to client browser
						memset(resBuf, BUFFSIZE, 0);   // clear
					}
					fclose(hitFile);			
				}
				else if (index == 1)   // MISS
				{
					if ((web_fd = socket(PF_INET,SOCK_STREAM, 0)) < 0)   // set socket for connection with web server
					{
						printf("Server : can't open stream socket\n");
						return 0;
					}
						memset(&web_addr, 0, sizeof(web_addr));
			                web_addr.sin_family=AF_INET;
			                web_addr.sin_port=htons(80);
			                web_addr.sin_addr.s_addr= *((unsigned long*) host->h_addr);   // set socket information
						if(connect(web_fd, (struct sockaddr*)&web_addr, sizeof(web_addr))==-1)
					{
                                       		perror("Web Server : can't connect\n");
                               		}
						
					if((len2 = send(web_fd, buf, BUFFSIZE, 0) < 0))   // send client's http request to web server
					{
                                       		perror("Error to write");
                               		}
					
					while((len2 = recv(web_fd, resBuf, BUFFSIZE, 0)) > 0)   // receive http response from web server
					{
						if(len2 == 0)
							break;
						fwrite(resBuf, 1, len2, missFile);   // write it to cache file	
						memset(resBuf, BUFFSIZE, 0);
					}https://www.wikipedia.org/
					fclose(missFile);
					missFile = fopen(file_name, "rb");   // open cache file read mode
					if(missFile == NULL)
						printf("MISS FILE NULL!\n");
					while((len2 = fread(resBuf, 1, BUFFSIZE, missFile)) > 0)   // read cache file's content
					{
						if(len2 == 0)
							break;
						send(client_fd, resBuf, len2, 0);   // send it to client browser
						memset(resBuf, BUFFSIZE, 0);
					}
					fclose(missFile);	
				}
					chdir(home);   // change working directory to initial

				close(client_fd);   // close client	
				exit(0);    // exit child process	
			}
		}
		alarm(0);   // reset alarm
		close(client_fd);   // close client		
	}
	if((semctl(semid, 0, IPC_RMID, arg)) == -1)
	{
		perror("semctl failed");
		exit(1);
	}
	close(socket_fd);   // close socket
	close(web_fd);   // close web server socket
	fclose(fp);   // close 'logfile.txt'
	return 0;
}


