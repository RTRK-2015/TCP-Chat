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


DECLARE_VECTOR(User);


#endif
