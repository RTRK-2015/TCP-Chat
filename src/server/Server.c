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


int main()
{
	// Initialize vectors
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

		// Are there new connections?
		if (ready > 0 && FD_ISSET(Sock, &readfds))
		{
			--ready;
			NewUser();
		}

		// Have any existing connections sent some data?
		VEC_FOR(User, it, Users)
		{
			if (ready <= 0)
				break;

			if (FD_ISSET(it->Sock, &readfds))
			{
				--ready;
				Respond(it);
			}
		}

		// Recalculate maxfd, to account for any new users, or for users
		// who have disconnected
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

	// Client disconnected, nothing to do
	if (size == 0)
		return;

	// Prepare a regex to try and match the login command
	regex_t inRegex;
	regmatch_t matches[2];
	if (regcomp(&inRegex, "^" CMD_LOGIN_S REGEX_NAME ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");

	char outBuffer[BUFFER_SIZE] = "";

	// Is the command malformed?
	if (regexec(&inRegex, inBuffer, 2, matches, 0) != 0)
	{
		snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
	}
	else
	{
		// Copy the name into a local buffer
		char name[NAME_LENGTH + 1] = "";
		strncpy(name, inBuffer + matches[1].rm_so,
			MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));

		// Check whether the user already exists
		User* it = VFUN(User, Find)(Users, SameName, name);

		if (it != VFUN(User, End)(Users))
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
		}
		else
		{
			// If the user does not already exists, add to vector
			User newuser = { .Sock = newfd };
			strncpy(newuser.Name, name, NAME_LENGTH);

			VFUN(User, Push)(Users, &newuser);
			FD_SET(newfd, &master);

			snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S ";");
		}
	}

	send(Sock, outBuffer, BUFFER_SIZE, 0);
	regfree(&inRegex);
}


void RemoveUser(User *U)
{
	// If the user is a part of a connection, remove it
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
		// Get the number of logged users and send it
		snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S "%zu;", VFUN(User, Size)(Users));
		send(U->Sock, outBuffer, BUFFER_SIZE, 0);

		// Send the usernames one by one
		VEC_FOR(User, it, Users)
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_YES_S "%s;", it->Name);
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		break;

	case CMD_CLOSE:
		// If the user is in an active connection, remove it
		cit = VFUN(Conn, Find)(Conns, Active, U->Sock);
		if (cit != VFUN(Conn, End)(Conns))
			VFUN(Conn, Remove)(Conns, cit);
		break;

	case CMD_TALK:
		// Prepare a regex to match a talk command
		if (regcomp(&regex, "^" CMD_TALK_S REGEX_NAME ";$", REG_EXTENDED) != 0)
			ERROR("TALK regcomp");

		// Is the command malformed?
		if (regexec(&regex, inBuffer, 2, matches, 0) != 0)
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		else
		{
			// Copy the name in a local buffer
			strncpy(name, inBuffer + matches[1].rm_so,
				MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));

			// Check whether the user exists
			uit = VFUN(User, Find)(Users, SameName, name);

			if (uit == VFUN(User, End)(Users))
			{
				snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
				send(U->Sock, outBuffer, BUFFER_SIZE, 0);
			}
			else
			{
				// Check whether the user is in an active connection already
				cit = VFUN(Conn, Find)(Conns, Active, uit->Sock);

				if (cit != VFUN(Conn, End)(Conns))
				{
					snprintf(outBuffer, BUFFER_SIZE, CMD_NO_S ";");
					send(U->Sock, outBuffer, BUFFER_SIZE, 0);
				}
				else
				{
					// If the user is not in an active connection, check whether
					// the user is waiting for a response
					cit = VFUN(Conn, Find)(Conns, Waiting, uit->Sock);

					// No, this is a request
					if (cit == VFUN(Conn, End)(Conns))
					{
						conn.Sock1 = U->Sock;
						conn.Sock2 = 0;

						VFUN(Conn, Push)(Conns, &conn);
						snprintf(outBuffer, BUFFER_SIZE, CMD_TALK_S "%s;", U->Name);
						send(uit->Sock, outBuffer, BUFFER_SIZE, 0);
					}
					// Yes, the user is waiting
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
		// Prepare a regex to match the send command
		if (regcomp(&regex, "^" CMD_SEND_S REGEX_TEXT ";$", REG_EXTENDED) != 0)
			ERROR("SEND regcomp");

		// Is the command malformed?
		if (regexec(&regex, inBuffer, 3, matches, 0) != 0)
		{
			snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
			send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		}
		else
		{
			// Copy the text into a local buffer
			strncpy(text, inBuffer + matches[1].rm_so,
				MIN(BUFFER_SIZE - 1, matches[1].rm_eo - matches[1].rm_so));

			// Check whether the user is in an active connection
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
		// No idea what was sent
		snprintf(outBuffer, BUFFER_SIZE, CMD_WTF_S ";");
		send(U->Sock, outBuffer, BUFFER_SIZE, 0);
		break;
	}
}
