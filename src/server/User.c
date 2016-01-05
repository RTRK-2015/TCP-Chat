#include "User.h"
#include <stdlib.h>
#include <string.h>


User UserCopy(const User* U, va_list v)
{
	User ret = { };

	strncpy(ret.Name, U->Name, NAME_LENGTH);
	ret.Name[NAME_LENGTH] = '\0';
	ret.Sock = U->Sock;

	return ret;
}

void UserDelete(User *U, va_list v)
{

}


INSTANTIATE_VECTOR(User, UserCopy, UserDelete);
