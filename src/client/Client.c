#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include "Common.h"
#include "Config.h"


int Sock;
char Name[NAME_LENGTH + 1];
fd_set master, readfds;
char Buffer[BUFFER_SIZE];

regex_t TalkRegex, YesRegex, NoRegex, SendRegex, TalkCmdRegex, NotCmdRegex, SendCmdRegex;

#define READ_SIZE 512


void Users();
void Close();
void Logoff();
void Send(const char *UserBuffer, regmatch_t match);
void Talk(const char *UserBuffer, regmatch_t match);


int main(int argc, char *argv[])
{
	Sock = socket(AF_INET, SOCK_STREAM, 0);

	if (regcomp(&TalkRegex, "^" CMD_TALK_S REGEX_NAME ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&YesRegex, "^" CMD_YES_S ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&NoRegex, "^" CMD_NO_S ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&SendRegex, "^" CMD_SEND_S REGEX_TEXT ";$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&TalkCmdRegex, "^/talk " REGEX_NAME "$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&NotCmdRegex, "^/(.+)$", REG_EXTENDED) != 0)
		ERROR("regcomp");
	if (regcomp(&SendCmdRegex, "^(.+)$", REG_EXTENDED) != 0)
		ERROR("regcomp");

	if (argc == 1)
		ABORT("Usage: %s <username> [<server address>]\n", argv[0]);

	strncpy(Name, argv[1], NAME_LENGTH);

	struct sockaddr_in server = 
	{ .sin_family = AF_INET,
	  .sin_addr.s_addr = inet_addr((argc > 2)? argv[2] : "127.0.0.1"),
	  .sin_port = htons(PORT) };

	printf("Connecting to server...\n");
	if (connect(Sock, (struct sockaddr *)&server, sizeof(server)) == -1)
		ERROR("connect");

	snprintf(Buffer, BUFFER_SIZE, CMD_LOGIN_S "%s;", Name);
	if (send(Sock, Buffer, BUFFER_SIZE, 0) == -1)
		ERROR("send");
	int ret = recv(Sock, Buffer, BUFFER_SIZE, 0);
	if (ret == -1)
		ERROR("recv");
	if (ret == 0)
		ABORT("Server disconnected\n");

	if (strcmp(Buffer, CMD_YES_S ";") == 0)
		printf("Connected!\n");
	else if (strcmp(Buffer, CMD_NO_S ";") == 0)
		ABORT("Server refused the login!\n")
	else
		ABORT("Unrecognized response1 by the server\n")

	FD_ZERO(&readfds);
	FD_ZERO(&master);
	FD_SET(Sock, &master);
	FD_SET(STDIN_FILENO, &master);
	int maxfd = (Sock > STDIN_FILENO)? Sock : STDIN_FILENO;

	while (true)
	{
		memcpy(&readfds, &master, sizeof(master));

		if (select(maxfd + 1, &readfds, NULL, NULL, NULL) == -1)
			ERROR("select");

		if (FD_ISSET(Sock, &readfds))
		{
			recv(Sock, Buffer, BUFFER_SIZE, 0);

			regmatch_t matches[2];
			if (regexec(&TalkRegex, Buffer, 2, matches, 0) == 0)
			{
				char name[NAME_LENGTH + 1] = "";
				strncpy(name, Buffer + matches[1].rm_so, MIN(NAME_LENGTH, matches[1].rm_eo - matches[1].rm_so));
				printf("The user '%s' wants to talk\n", name);
				send(Sock, Buffer, BUFFER_SIZE, 0);
			}
			else if (regexec(&YesRegex, Buffer, 0, NULL, 0) == 0)
			{
				printf("Accepted the connection\n");
			} 
			else if (regexec(&NoRegex, Buffer, 0, NULL, 0) == 0)
			{
				printf("The server refused\n");
			}
			else if (regexec(&SendRegex, Buffer, 2, matches, 0) == 0)
			{
				char text[BUFFER_SIZE - 1] = "";
				strncpy(text, Buffer + matches[1].rm_so, MIN(BUFFER_SIZE - 1, matches[1].rm_eo - matches[1].rm_so));
				printf("%s\n", text);
			}
			else
			{
				printf("Unrecognized command from the server!\n");
			}
		}

		if (FD_ISSET(STDIN_FILENO, &readfds))
		{
			char userBuffer[READ_SIZE] = "";
			int n = read(STDIN_FILENO, userBuffer, READ_SIZE);

			// Remove \n
			userBuffer[n - 1] = '\0';

			regmatch_t matches[2];

			if (strcmp(userBuffer, "/users") == 0)
				Users();
			else if (regexec(&TalkCmdRegex, userBuffer, 2, matches, 0) == 0)
				Talk(userBuffer, matches[1]);
			else if (strcmp(userBuffer, "/close") == 0)
				Close();
			else if (strcmp(userBuffer, "/logoff") == 0)
				Logoff();
			else if (regexec(&NotCmdRegex, userBuffer, 0, NULL, 0) == 0)
				printf("Unrecognized command\n");
			else if (regexec(&SendCmdRegex, userBuffer, 2, matches, 0) == 0)
				Send(userBuffer, matches[1]);
		}
	}

	close(Sock);
}


void Users()
{
	snprintf(Buffer, BUFFER_SIZE, CMD_USERS_S ";");

	int r = send(Sock, Buffer, BUFFER_SIZE, 0);

	if (r == -1)
		ERROR("send");
	if (r == 0)
		ABORT("Server disconnected\n");

	r = recv(Sock, Buffer, BUFFER_SIZE, 0);
	if (r == -1)
		ERROR("recv");
	if (r == 0)
		ABORT("Server disconnected\n");

	size_t n = atoi(Buffer + 1);
	for (size_t i = 0; i < n; ++i)
	{
		char name[NAME_LENGTH + 1] = "";

		r = recv(Sock, Buffer, BUFFER_SIZE, 0);
		if (r == -1)
			ERROR("recv");
		if (r == 0)
			ABORT("Server disconnected\n");

		strncpy(name, Buffer + 1, strlen(Buffer) - 2);
		printf("%s\n", name);
	}
}


void Close()
{
	snprintf(Buffer, BUFFER_SIZE, CMD_CLOSE_S ";");

	int r = send(Sock, Buffer, BUFFER_SIZE, 0);
	if (r == -1)
		ERROR("send");
	if (r == 0)
		ABORT("Server disconnected\n");
}


void Logoff()
{
	snprintf(Buffer, BUFFER_SIZE, CMD_LOGOFF_S ";");

	send(Sock, Buffer, BUFFER_SIZE, 0);
	exit(EXIT_SUCCESS);
}


void Send(const char *UserBuffer, regmatch_t match)
{
	char text[BUFFER_SIZE - 1] = "";
	strncpy(text, UserBuffer + match.rm_so, MIN(BUFFER_SIZE - 2, match.rm_eo - match.rm_so));

	snprintf(Buffer, BUFFER_SIZE, CMD_SEND_S "%s;", text);

	int r = send(Sock, Buffer, BUFFER_SIZE, 0);
	if (r == -1)
		ERROR("send");
	if (r == 0)
		ABORT("Server disconnected\n");
}


void Talk(const char *UserBuffer, regmatch_t match)
{
	char name[NAME_LENGTH + 1] = "";
	strncpy(name, UserBuffer + match.rm_so, MIN(NAME_LENGTH, match.rm_eo - match.rm_so));

	snprintf(Buffer, BUFFER_SIZE, CMD_TALK_S "%s;", name);

	int r = send(Sock, Buffer, BUFFER_SIZE, 0);
	if (r == -1)
		ERROR("send");
	if (r == 0)
		ABORT("Server disconnected\n");

	r = recv(Sock, Buffer, BUFFER_SIZE, 0);
	if (r == -1)
		ERROR("recv");
	if (r == 0)
		ABORT("Server disconnected\n");

	if (strcmp(Buffer, CMD_YES_S ";") == 0)
		printf("The other user accepted the connection\n");
	else if (strcmp(Buffer, CMD_NO_S ";") == 0)
		printf("The other user did not accept the connection\n");
	else
		printf("Unrecognized response from the server\n");
}
