#ifndef USER_H
#define USER_H


#include <string.h>
#include "Common.h"
#include "Vector.h"


typedef struct
{
	char Name[NAME_LENGTH + 1];
	int  Sock;
} User;


bool SameName(const User* U, va_list args);


DECLARE_VECTOR(User);


#endif
