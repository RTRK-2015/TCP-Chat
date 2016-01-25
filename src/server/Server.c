#include <arpa/inet.h>
#include <fcntl.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "Common.h"
#include "Config.h"
#include "User.h"


#define ERROR(str) { perror(str); exit(EXIT_FAILURE); }
#define MIN(x, y) (((x) < (y))? (x) : (y))
#define REGEX_NAME "([[:alpha:]]+)"

// The "main" socket
static int Sock;

VEC(User) Users;
fd_set master, readfds;


// Forward declarations
void NewUser();
void Respond(const User* U);


int main(int argc, char *argv[])
{
	Users = VFUN(User, New)();

	// Create the "main" socket, bind it to port and start listening
	if ((Sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
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
	int maxfd = Sock;
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	FD_SET(Sock, &master);

	while (true)
	{
		memcpy(&readfds, &master, sizeof(master));

		int ready;
		if ((ready = select(maxfd + 1, &readfds, NULL, NULL, NULL)) == -1)
			ERROR("select");

		if (ready > 0 && FD_ISSET(Sock, &readfds))
		{
			--ready;
			NewUser();
		}

		for (User *it = VFUN(User, Begin)(Users);
			it != VFUN(User, End)(Users) && ready > 0; ++it)
		{
			if (FD_ISSET(it->Sock, &readfds))
			{
				--ready;
				Respond(it);
			}
		}

		for (User *it = VFUN(User, Begin)(Users);
			it != VFUN(User, End)(Users); ++it)
		{
			if (it->Sock > maxfd)
				maxfd = it->Sock;
		}
	}
}


void NewUser()
{
	int newfd;

	if ((newfd = accept(Sock, NULL, NULL)) == -1)
		ERROR("accept");

	char inBuffer[BUFFER_SIZE + 1] = "";
	int size = recv(newfd, inBuffer, BUFFER_SIZE, 0);

	if (size == -1)
		ERROR("recv");

	if (size == 0)
		return;

	regex_t inRegex;
	regmatch_t matches[2];
	if (regcomp(&inRegex, "^" CMD_LOGIN_S REGEX_NAME ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");

	char outBuffer[BUFFER_SIZE + 1] = "";
	if (regexec(&inRegex, inBuffer, 2, matches, 0) != 0)
	{
		outBuffer[0] = CMD_WTF;
		outBuffer[1] = ';';
	}
	else
	{
		char name[NAME_LENGTH + 1] = "";
		strncpy(name, matches[1].rm_so, MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));

		User* it = VFUN(User, Find)(Users, SameName, name);

		if (it != VFUN(User, End)(Users))
		{
			outBuffer[0] = CMD_NO;
			outBuffer[1] = ';';
		}
		else
		{
			User newuser = { .Sock = newfd, .Name = name };
			VFUN(User, Push)(Users, &newuser);

			FD_SET(newfd, &master);

			outBuffer[0] = CMD_YES;
			outBuffer[1] = ';';
		}
	}

	send(newfd, outBuffer, BUFFER_SIZE, MSG_DONTWAIT);
	regfree(&inRegex);
}

void Respond(const User* U)
{
    char inBuffer[BUFFER_SIZE + 1] = "", outBuffer[BUFFER_SIZE + 1] = "";
    bool canSend = true;
    size_t len;
    char name[NAME_LENGTH + 1] = "";
    User *it;
    regex_t regex;
    regmatch_t matches[2];

    int ret = recv(U->Sock, inBuffer, BUFFER_SIZE, 0);
    if (ret == -1)
    {
	ERROR("recv");
    }
    if (ret == 0)
    {
	VFUN(User, Remove)(Users, U);
	canSend = false;
    }

    switch (inBuffer[0])
    {
    case CMD_LOGOFF:
	VFUN(User, Remove)(Users, U);
	canSend = false;
	break;

    case CMD_TALK:
	if (regcomp(&regex, "^" CMD_TALK REGEX_NAME ";$", REG_EXTENDED) != 0)
	    ERROR("regcomp");
	if (regexec(&regex, inBuffer, 2, matches, 0) != 0)
	{
	    outBuffer[0] = CMD_WTF;
	    outBuffer[1] = ';';
	}
	else
	{
	    strncpy(name, matches[1].rm_so, MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));
	    it = VFUN(User, Find)(Users, SameName, name);

	    if (it == VFUN(User, End)(Users))
	    {
		outBuffer[0] = CMD_NO;
		outBuffer[1] = ';';
	    }
	    else
	    {
		canSend = false;
		outBuffer[0] = CMD_TALK;
		strncpy(outBuffer + 1, U->Name, NAME_LENGTH);
		len = strlen(outBuffer);
		outBuffer[len] = ';';
		send(it->Sock, outBuffer, BUFFER_SIZE, 0);
	    }
	}
	break;

    case CMD_SEND:
	break;

    default:
	outBuffer[0] = CMD_WTF;
	outBuffer[1] = ';';
	break;
    }

    if (canSend)
        send(U->Sock, outBuffer, BUFFER_SIZE, 0);
}
