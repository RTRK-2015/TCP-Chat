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
#include "Conn.h"


#define ERROR(str) { perror(str); exit(EXIT_FAILURE); }
#define MIN(x, y) (((x) < (y))? (x) : (y))
#define REGEX_NAME "([[:alpha:]]+)"
#define REGEX_TEXT "(.+)"

// The "main" socket
static int Sock;

VEC(User) Users;
VEC(Conn) Conns;
fd_set master, readfds;


// Forward declarations
void NewUser();
void Respond(User* U);
void RemoveUser(User *U);


int main(int argc, char *argv[])
{
	Users = VFUN(User, New)();
	Conns = VFUN(Conn, New)();

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

		VEC_FOR(User, it, Users)
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

	char outBuffer[BUFFER_SIZE] = "";
	if (regexec(&inRegex, inBuffer, 2, matches, 0) != 0)
	{
		outBuffer[0] = CMD_WTF;
		outBuffer[1] = ';';
	}
	else
	{
		char name[NAME_LENGTH + 1] = "";
		strncpy(name, inBuffer + matches[1].rm_so,
			MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));

		User* it = VFUN(User, Find)(Users, SameName, name);

		if (it != VFUN(User, End)(Users))
		{
			outBuffer[0] = CMD_NO;
			outBuffer[1] = ';';
		}
		else
		{
			User newuser = { .Sock = newfd };
			strncpy(newuser.Name, name, NAME_LENGTH);

			VFUN(User, Push)(Users, &newuser);

			FD_SET(newfd, &master);

			outBuffer[0] = CMD_YES;
			outBuffer[1] = ';';
		}
	}

	send(newfd, outBuffer, BUFFER_SIZE, 0);
	regfree(&inRegex);
}


void RemoveUser(User *U)
{
	VEC_FOR(Conn, cit, Conns)
	{
		if (cit->Sock1 == U->Sock || cit->Sock2 == U->Sock)
		{
			VFUN(Conn, Remove)(Conns, cit);
			break;
		}
	}

	VFUN(User, Remove)(Users, U);
	FD_CLR(U->Sock, &master);
}


void Respond(User* U)
{
	char inBuffer[BUFFER_SIZE] = "";
	int ret = recv(U->Sock, inBuffer, BUFFER_SIZE, 0);
	if (ret == -1)
		ERROR("recv");

	if (ret == 0)
		RemoveUser(U);

	char outBuffer[BUFFER_SIZE + 1] = "";
	char name[NAME_LENGTH + 1]      = "";
	char text[BUFFER_SIZE]          = "";
	User *uit;
	Conn *cit;
	Conn conn;
	regex_t regex;
	regmatch_t matches[2];

	switch (inBuffer[0])
	{
	case CMD_LOGOFF:
		RemoveUser(U);
		break;

	case CMD_USERS:
		snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S "%zu;", VFUN(User, Size)(Users));
		send(U->Sock, outBuffer, BUFFER_SIZE, 0);

		VEC_FOR(User, it, Users)
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S "%s;", it->Name);
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		break;

	case CMD_CLOSE:
		cit = VFUN(Conn, Find)(Conns, Active, U->Sock);
		if (cit != VFUN(Conn, End)(Conns))
			VFUN(Conn, Remove)(Conns, cit);
		break;

	case CMD_TALK:
		if (regcomp(&regex, "^" CMD_TALK_S REGEX_NAME ";$", REG_EXTENDED) != 0)
			ERROR("TALK regcomp");
		if (regexec(&regex, inBuffer, 2, matches, 0) != 0)
		{
			outBuffer[0] = CMD_WTF;
			outBuffer[1] = ';';
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		else
		{
			strncpy(name, inBuffer + matches[1].rm_so,
				MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));
			uit = VFUN(User, Find)(Users, SameName, name);

			// The other user does not exist
			if (uit == VFUN(User, End)(Users))
			{
				snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
				send(U->Sock, outBuffer, BUFFER_SIZE, 0);
			}
			else
			{
				cit = VFUN(Conn, Find)(Conns, Active, uit->Sock);

				// The other user is already in a conversation
				if (cit != VFUN(Conn, End)(Conns))
				{
					snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
					send(U->Sock, outBuffer, BUFFER_SIZE, 0);
				}
				else
				{
					cit = VFUN(Conn, Find)(Conns, Waiting, uit->Sock);

					// This user initiates the connection
					if (cit == VFUN(Conn, End)(Conns))
					{
						conn.Sock1 = U->Sock;
						conn.Sock2 = 0;

						VFUN(Conn, Push)(Conns, &conn);
						snprintf(outBuffer, BUFFER_SIZE, CMD_TALK_S "%s;", U->Name);
						send(uit->Sock, outBuffer, BUFFER_SIZE, 0);
					}
					else
					{
						if (cit->Sock1 == 0)
							cit->Sock1 = U->Sock;
						else
							cit->Sock2 = U->Sock;

						snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S ";");
						send(uit->Sock, outBuffer, BUFFER_SIZE, 0);
					}
				}
			}
		}
		break;

	case CMD_SEND:
		if (regcomp(&regex, "^" CMD_SEND_S REGEX_TEXT ";$",
			REG_EXTENDED) != 0)
			ERROR("SEND regcomp");
		if (regexec(&regex, inBuffer, 3, matches, 0) != 0)
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		else
		{
			strncpy(text, inBuffer + matches[1].rm_so,
				MIN(BUFFER_SIZE - 1, matches[1].rm_eo - matches[1].rm_so));

			cit = VFUN(Conn, Find)(Conns, Active, U->Sock);

			if (cit == VFUN(Conn, End)(Conns))
			{
				snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
				send(U->Sock, outBuffer, BUFFER_SIZE, 0);
			}
			else
			{
				snprintf(outBuffer, BUFFER_SIZE, CMD_SEND_S "%s;", text);
				if (cit->Sock1 == U->Sock)
					send(cit->Sock2, outBuffer, BUFFER_SIZE, 0);
				else
					send(cit->Sock1, outBuffer, BUFFER_SIZE, 0);
			}
		}
		break;

	default:
		snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
		send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		break;
	}
}
