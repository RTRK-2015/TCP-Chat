#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Common.h"
#include "Config.h"


static int Sock;


int main(int argc, char *argv[])
{
	Sock = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in server =
	{
		.sin_family      = AF_INET,
		.sin_addr.s_addr = inet_addr("127.0.0.1"),
		.sin_port        = htons(PORT)
	};

	connect(Sock, (struct sockaddr *)&server, sizeof(server));

	close(Sock);
}
