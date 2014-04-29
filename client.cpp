#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <fstream>
#include <inttypes.h>
#include "util.h"

using std::string;

#define MAX_BUFFER_SIZE 1024

const string CMD_LIST("ls");
const string CMD_GET("get");

int recvFile(int client_fd, string cmd);
int main(int argc, char* argv[]);


int recvFile(int client_fd, string cmd)
{
	string fname = cmd.substr(4, cmd.length()-5);

	int recvBytes;
	// Receive file length and file name length first
	uint32_t fileLen, fnameLen, fileLen_net, fnameLen_net;
	recvBytes = read_n(client_fd, (char*) &fileLen_net, sizeof(fileLen_net));
	if (recvBytes < 0)
		error("read_n()");
	recvBytes = read_n(client_fd, (char*) &fnameLen_net, sizeof(fnameLen_net));
	if (recvBytes < 0)
		error("read_n()");

	fileLen = ntohl(fileLen_net);
	fnameLen = ntohl(fnameLen_net);

	char* writebuff = new char[fileLen];

	recvBytes = read_n(client_fd, writebuff, fileLen);
	if (recvBytes < 0)
		error("read_n()");
	printf("Recv %d bytes from server\n", recvBytes);
	printf("Saving file to %s\n", fname.c_str());

	std::fstream os(fname.c_str(), std::fstream::binary | std::fstream::out);

	if (!os.is_open()) {
		delete[] writebuff;
		error("Error - open() file to write\n");		
	} else {
		os.write(writebuff, recvBytes);
		os.close();	
		delete[] writebuff;	
	}
	return 0;	
}

int main(int argc, char* argv[])
{
	int client_fd;
	struct sockaddr_in serverAddr;
	char buff[MAX_BUFFER_SIZE];

	client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (client_fd < 0)
		error("socket()");

	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9999);

	if ( connect(client_fd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0 )
		error("connect()");

	printf("$ ");
	while (fgets(buff, sizeof(buff), stdin) != NULL)
	{
		ssize_t sentBytes = write_n(client_fd, buff, strlen(buff));
		if (sentBytes < 0)
			error("write_n()");

		string cmd(buff);
		
		if (cmd.substr(0, 3) == CMD_GET)
		{	// CMD_GET
			recvFile(client_fd, cmd);			
		}
		else if (cmd.substr(0, 2) == CMD_LIST)
		{	// CMD_LIST
			ssize_t recvBytes = read(client_fd, buff, sizeof(buff));
			if (recvBytes < 0)
				error("read()");
			else if(recvBytes == 0) {
				printf("The server terminated prematurely\n");
				exit(1);
			} else {
				buff[recvBytes] = '\0';		// Null terminates
				printf("Recv: %s", buff);				
			}
		}
		else
			printf("ERROR - command not found\n");
		printf("$ ");			

	}

	return 0;
}

