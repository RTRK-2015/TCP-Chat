#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Common.h"
#include "Config.h"


#define ERROR(str) { perror(str); exit(EXIT_FAILURE); }

// The "main" socket
static int Sock;

// An array of accept()ed sockets.
static int *Socks;
static size_t Length;
static size_t Reserved;


void Cleanup(int signum);
void InstallSignalHandler();


int main(int argc, char *argv[])
{
	InstallSignalHandler();

	// Prepare the array
	Length   = 0;
	Reserved = 10;
	Socks    = (int*)malloc(Reserved * sizeof(int));
	if (Socks == NULL)
		ERROR("malloc");

	// Create the "main" socket, bind it to port and start listening
	Sock = socket(AF_INET, SOCK_STREAM, 0);
	if (Sock == -1)
		ERROR("socket");

	struct sockaddr_in server =
	{
		.sin_family      = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port        = htons(PORT)
	};

	if (bind(Sock, (struct sockaddr *)&server, sizeof(server)) == -1)
		ERROR("bind");

	if (listen(Sock, 3) == -1)
		ERROR("listen");

	// The main processing loop
	while (true)
	{

	}
}


void Cleanup(int signum)
{
	static const char sigintText[]     = "SIGINT";
	static const char sigtermText[]    = "SIGTERM";
	static const char sigunknownText[] = "Unknown signal";
	static const char caught[]         = "Caught ";
	static const char exiting[]        = ", exiting.\n";
	static const int fd                = STDERR_FILENO;

	write(fd, caught, sizeof(caught));
	if (signum == SIGINT)
		write(fd, sigintText, sizeof(sigintText));
	else if (signum == SIGTERM)
		write(fd, sigtermText, sizeof(sigtermText));
	else
		write(fd, sigunknownText, sizeof(sigunknownText));
	write(fd, exiting, sizeof(exiting));
	fsync(fd);

	close(Sock);

	exit(EXIT_SUCCESS);
}

void InstallSignalHandler()
{
	struct sigaction act = { };

	sigfillset(&act.sa_mask);
	act.sa_handler = Cleanup;

	if (sigaction(SIGINT, &act, NULL) == -1)
		ERROR("sigaction (SIGINT)");

	if (sigaction(SIGTERM, &act, NULL) == -1)
		ERROR("sigaction (SIGTERM)");
}
