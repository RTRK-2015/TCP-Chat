#ifndef COMMON_H
#define COMMON_H


#define PORT 27015
#define NAME_LENGTH 32
#define BUFFER_SIZE 512

#define SEPARATOR ':'
#define TERMINATOR ';'
#define ESCAPER '\\'

enum Command
{
	CMD_LOGIN  = 'i',
	CMD_LOGOFF = 'o',
	CMD_USERS  = 'u',
	CMD_TALK   = 't',
	CMD_SEND   = 's',
	CMD_CLOSE  = 'c',
	CMD_YES    = 'y',
	CMD_NO     = 'n',
	CMD_WTF    = 'w'
};


#endif
