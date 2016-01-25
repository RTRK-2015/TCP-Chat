#include "Conn.h"


bool Active(const Conn *C, va_list args)
{
	int sock = va_arg(args, int);

	if ((C->Sock1 == sock && C->Sock2 != 0) ||
		(C->Sock1 != 0    && C->Sock2 == sock))
		return true;
	else
		return false;
}


bool Waiting(const Conn *C, va_list args)
{
	int sock = va_arg(args, int);

	if ((C->Sock1 == sock && C->Sock2 == 0) ||
		(C->Sock1 == 0    && C->Sock2 == sock))
		return true;
	else
		return false;
}


INSTANTIATE_VECTOR(Conn, NULL, NULL);
