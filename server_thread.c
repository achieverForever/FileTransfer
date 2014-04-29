#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include <pthread.h>
#include "util.h"
using std::string;

#define MAX_BUFFER_SIZE 512
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999
#define FDSET_SIZE 10
#define BASE_DIR "data/"

const string CMD_LIST("ls");
const string CMD_GET("get");

typedef struct {
	int client_fd;
} ClientInfo;

int main(int argc, char* argv[]);
int processRequest(string& cmd, pthread_t tid, ClientInfo* client);
void* run(void* arg);


int processRequest(string& cmd, pthread_t tid, ClientInfo* client)
{
	printf("thread #%lu  Recv: %s", tid, cmd.c_str());

	char buff[MAX_BUFFER_SIZE];
	int client_fd = client->client_fd;

	// CMD_LIST 
	if ( cmd.substr(0, 2) == CMD_LIST )
	{	
		// List files in current directory and sent them back to client
		DIR* dir;
		struct dirent* dir_entry;
		dir = opendir("data");
		if (dir)
		{
			int currLen = 0;
			while ( (dir_entry = readdir(dir)) != NULL )
			{
				if (dir_entry->d_type == DT_REG)
				{
					snprintf(buff+currLen, sizeof(buff)-currLen, "%s\n", dir_entry->d_name);
					currLen += strlen(dir_entry->d_name) + 1;
				}
			}
			ssize_t sentBytes = write_n(client_fd, buff, strlen(buff));
			if (sentBytes < 0) {
				printf("write_n()\n");		
				closedir(dir);
				return -1;
			}
		}
		closedir(dir);
		return 0;
	}
	// CMD_GET
	else if ( cmd.substr(0, 3) == CMD_GET )
	{	
		string fname = string(BASE_DIR) + cmd.substr(4, cmd.length()-5);
		printf("Opening file  %s to read\n", fname.c_str());
		
		std::fstream is(fname.c_str(), std::fstream::binary | std::fstream::in);
		if (is.is_open())
		{
			int length = 0;
			is.seekg(0, is.end);
			length = is.tellg();
			is.seekg(0, is.beg);

			char* readbuff = new char[length];
			is.read(readbuff, length);

			if (!is) {
				printf("ERROR - read() file \n");
				is.close();
				delete[] readbuff;
				return -1;
			} else {
				ssize_t sentBytes;
				uint32_t fileLen = htonl(length);
				uint32_t fnameLen = htonl(fname.length());

				sentBytes = write_n(client_fd, (char*) &fileLen, sizeof(fileLen));
				if (sentBytes < 0){
					printf("write_n()\n");
					return -1;
				}
				sentBytes = write_n(client_fd, (char*) &fnameLen, sizeof(fnameLen));
				if (sentBytes < 0){
					printf("write_n()\n");
					return -1;
				}
				sentBytes = write_n(client_fd, readbuff, length);
				if (sentBytes < 0){
					printf("write_n()\n");
					return -1;
				}
				printf("Sent %d bytes to client\n", sentBytes);

				is.close();
				delete[] readbuff;
				return 0;
			}
		} else {
			printf("ERROR - open() file\n");
			return -1;			
		}

	}
	else
	{
		snprintf(buff, sizeof(buff), "ERROR - command not found\n");
		ssize_t sentBytes = write_n(client_fd, buff, strlen(buff));
		if (sentBytes < 0) {
			printf("write_n()\n");		
			return -1;					
		}
	}
	return 0;
}

void* run(void* arg)
{
	pthread_detach(pthread_self());

	ClientInfo* client = (ClientInfo*) arg;
	char buff[MAX_BUFFER_SIZE];
	int client_fd = client->client_fd;
	int recvBytes;

	while (1)
	{
		// Read client's command
		recvBytes = read(client_fd, buff, sizeof(buff));
		if (recvBytes < 0) {
			printf("read() - thread\n");
			return NULL;			
		} else if (recvBytes == 0) {
			printf("thread #%lu received EOF, quit...\n", pthread_self());
			return NULL;
		}
		else {
			buff[recvBytes] = '\0';	// NULL terminates
			string cmd(buff);
			if ( processRequest(cmd, pthread_self(), client) < 0)
				return NULL;
		}		
	}
}

int main(int argc, char* argv[])
{
	int listen_fd, conn_fd, ret;
	struct sockaddr_in serverAddr, clientAddr;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0)
		error("socket()");

	bzero(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	serverAddr.sin_port = htons(SERVER_PORT);

	ret = bind(listen_fd, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
	if (ret < 0)
		error("bind()");


	ret = listen(listen_fd, 5);
	if (ret < 0)
		error("listen()");

	while (1)
	{
		socklen_t len = sizeof(clientAddr);

		conn_fd = accept(listen_fd, (struct sockaddr*) &clientAddr, &len);
		printf("Got connection from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

		pthread_t tid;
		ClientInfo client;
		client.client_fd = conn_fd;
		if ( 0 != pthread_create(&tid, NULL, run, (void*) &client) )
			error("pthread_create()");
		printf("Created thread #%lu\n", tid);

	}

	return 0;
}