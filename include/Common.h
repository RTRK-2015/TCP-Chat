#ifndef COMMON_H
#define COMMON_H


#include <stdlib.h>


#define ERROR(str) { perror(str); exit(EXIT_FAILURE); }

#define PORT 27015
#define NAME_LENGTH 32
#define BUFFER_SIZE 512

#define SEPARATOR ':'
#define SEPARATOR_S ":"
#define TERMINATOR ';'
#define TERIMATOR_S ";"
#define ESCAPER '\\'
#define ESCAPER_S "\\"

#define CMD_LOGIN 'i'
#define CMD_LOGIN_S "i"

#define CMD_LOGOFF 'o'
#define CMD_LOGOFF_S "o"

#define CMD_USERS 'u'
#define CMD_USERS_S "u"

#define CMD_TALK 't'
#define CMD_TALK_S "t"

#define CMD_SEND 's'
#define CMD_SEND_S "s"

#define CMD_CLOSE 'c'
#define CMD_CLOSE_S "c"

#define CMD_YES 'y'
#define CMD_YES_S "y"

#define CMD_NO 'n'
#define CMD_NO_S "n"

#define CMD_WTF 'w'
#define CMD_WTF_S "w"

#endif
