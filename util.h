#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// Read n bytes from a descriptor
ssize_t read_n(int fd, char* buff, size_t n)
{
	size_t numLeft;
	ssize_t numRead;
	char* ptr;

	ptr = buff;
	numLeft = n;
	while (numLeft > 0)
	{
		if ( (numRead = read(fd, ptr, numLeft)) < 0 )
		{
			if (errno == EINTR)
				numRead = 0;
			else
				return -1;
		}
		else if (numRead == 0)
			break;	// EOF

		numLeft -= numRead;
		ptr += numRead;
	}
	return (n - numLeft);
}

// Write n bytes to a descriptor
ssize_t write_n(int fd, const char* buff, size_t n)
{
	size_t numLeft;
	ssize_t numWritten;
	const char* ptr;

	ptr = buff;
	numLeft = n;
	while (numLeft > 0)
	{
		if ( (numWritten = write(fd, ptr, numLeft)) <= 0 )
		{
			if (errno == EINTR && numWritten < 0)
				numWritten = 0;
			else
				return -1;
		}
		numLeft -= numWritten;
		ptr += numWritten;
	}
	return n;
}

void error(const char* msg)
{
	perror(msg);
	exit(1);
}