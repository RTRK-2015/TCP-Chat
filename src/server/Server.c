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


// The "main" socket
static int Sock;

VEC(User) Users;
fd_set master, readfds;


// Forward declarations
void Cleanup(int signum);
void InstallSignalHandler();
void NewUser();
void Respond(const User* U);


int main(int argc, char *argv[])
{
	InstallSignalHandler();

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

	VFUN(User, Delete)(Users);
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


bool SameName(const User* U, va_list args)
{
	return strncmp(U->Name, va_arg(args, char*), NAME_LENGTH) == 0;
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
	if (regcomp(&inRegex, "^i(([[:alpha:]]|\\\\;)+);$", REG_EXTENDED) != 0)
		ERROR("regcomp");

	char outBuffer[BUFFER_SIZE + 1] = "";
	if (regexec(&inRegex, inBuffer, 2, matches, 0) != 0)
	{
		outBuffer[0] = CMD_WTF;
		outBuffer[1] = TERMINATOR;
	}
	else
	{
		char name[NAME_LENGTH] = "";

		for (regoff_t i = matches[1].rm_so, namei = 0;
			i < matches[1].rm_eo; ++i, ++namei)
		{
			if (inBuffer[i] == '\\')
			{
				name[namei] = ';';
				++i;
			}
			else
			{
				name[namei] = inBuffer[i];
			}
		}

		User* it = VFUN(User, Find)(Users, SameName, name);

		if (it != VFUN(User, End)(Users))
		{
			outBuffer[0] = CMD_NO;
			outBuffer[1] = TERMINATOR;
		}
		else
		{
			User newuser = { .Sock = newfd, .Name = name };
			VFUN(User, Push)(Users, &newuser);

			FD_SET(newfd, &master);

			outBuffer[0] = CMD_YES;
			outBuffer[1] = TERMINATOR;
		}
	}

	send(newfd, outBuffer, BUFFER_SIZE, MSG_DONTWAIT);
	regfree(&inRegex);
}

void Respond(const User* U)
{

}
