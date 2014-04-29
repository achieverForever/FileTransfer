#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include "util.h"
using std::string;

#define MAX_BUFFER_SIZE 512
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999
#define FDSET_SIZE 10
#define BASE_DIR "data/"

const string CMD_LIST("ls");
const string CMD_GET("get");

int processRequest(int client_fd, const char* cmd);
int main(int argc, char* argv[]);


int processRequest(int client_fd, const char* cmd)
{
	string cmdStr(cmd);
	char buff[MAX_BUFFER_SIZE];

	// CMD_LIST 
	if ( cmdStr.substr(0, 2) == CMD_LIST )
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
			if (sentBytes < 0)
				error("write_n()");		

		}
		closedir(dir);
		return 0;
	}
	// CMD_GET
	else if ( cmdStr.substr(0, 3) == CMD_GET )
	{	
		string fname = string(BASE_DIR) + cmdStr.substr(4, cmdStr.length()-5);
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

			if (is)
			{
				ssize_t sentBytes;
				uint32_t fileLen = htonl(length);
				uint32_t fnameLen = htonl(fname.length());

				sentBytes = write_n(client_fd, (char*) &fileLen, sizeof(fileLen));
				if (sentBytes < 0)
					error("write_n()");
				sentBytes = write_n(client_fd, (char*) &fnameLen, sizeof(fnameLen));
				if (sentBytes < 0)
					error("write_n()");

				sentBytes = write_n(client_fd, readbuff, length);
				if (sentBytes < 0)
					error("write_n()");		
				printf("Sent %d bytes to client\n", sentBytes);

				is.close();
				delete[] readbuff;
				return 0;			
			}
			else
			{
				printf("ERROR - read() file \n");
				is.close();
				delete[] readbuff;
				return -1;				
			}
		}
		else
		{
			printf("ERROR - open() file\n");
			return -1;			
		}

	}
	else
	{
		snprintf(buff, sizeof(buff), "ERROR - command not found\n");

		ssize_t sentBytes = write_n(client_fd, buff, strlen(buff));
		if (sentBytes < 0)
			error("write_n()");		
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int listen_fd, conn_fd;
	int i, max_idx, max_fd, ret;
	int numReady, clients[FDSET_SIZE];	// Used to store client fds
	fd_set readSet, allSet;
	struct sockaddr_in serverAddr, clientAddr;
	char buff[MAX_BUFFER_SIZE];

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

	// Initialize
	max_fd = listen_fd;
	max_idx = -1;
	for (i = 0; i < FDSET_SIZE; i++)
		clients[i] = -1;		// -1 indicates empty entry
	FD_ZERO(&allSet);
	FD_SET(listen_fd, &allSet);	// Put listen_fd into readSet

	while (1)
	{
		readSet = allSet;	// Reset readSet before each select()
		// Check listen_fd for new client connection request, select will block
		// until one fd becomes readable
		numReady = select(max_fd+1, &readSet, NULL, NULL, NULL);

		if (numReady < 0)
			error("select()");

		else if ( FD_ISSET(listen_fd, &readSet) )
		{	// New client connection 
			socklen_t len = sizeof(clientAddr);

			conn_fd = accept(listen_fd, (struct sockaddr*) &clientAddr, &len);
			printf("Got connection from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

			// Store this fd to clients array
			for (i = 0; i < FDSET_SIZE; i++)
			{
				if (clients[i] < 0)
				{
					clients[i] = conn_fd;
					break;					
				}
			}
			if (i == FDSET_SIZE)
				error("Too many clients");


			FD_SET(conn_fd, &allSet);	// Add new fd to readSet
			if (conn_fd > max_fd)
				max_fd = conn_fd;		// For select
			if (i > max_idx)
				max_idx = i;			// Max index in client array

			if (--numReady <= 0)
				continue;				// No more readable fds, select() again
		}

		// Check all clients for data
		for (i = 0; i <= max_idx; i++)
		{
			if (clients[i] < 0)
				continue;
			if( FD_ISSET(clients[i], &readSet) )
			{
				ssize_t recvBytes = read(clients[i], buff, sizeof(buff));
				if (recvBytes < 0)
					error("read()");
				else if(recvBytes == 0)
				{
					// EOF received, no more data from this fd, close it
					close(clients[i]);
					FD_CLR(clients[i], &allSet);
					clients[i] = -1;			
				}
				else
				{
					buff[recvBytes] = '\0';	// NULL terminate
					printf("Recv: %s", buff);

					int ret = processRequest(clients[i], buff);
					if (ret < 0)
						printf("Error - processRequest()\n");

				}

				if (--numReady <= 0)
					break;	// Exit for loop if all ready fds have been processed
			}
		}

	}

	return 0;
}