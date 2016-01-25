#ifndef CONN_H
#define CONN_H


#include "Common.h"
#include "Vector.h"


typedef struct
{
	int Sock1;
	int Sock2;
} Conn;


bool Active(const Conn *C, va_list args);
bool Waiting(const Conn *C, va_list args);


DECLARE_VECTOR(Conn);


#endif
