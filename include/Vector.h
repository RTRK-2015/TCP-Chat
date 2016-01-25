#ifndef VECTOR_H
#define VECTOR_H


#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>


// Private
#define _VTYPE(Type) _vec_##Type

// Public
#define VEC(Type) _VTYPE(Type)*
#define VFUN(Type, Fun) _vec_##Type##_##Fun


#define INSTANTIATE_VECTOR(Type, CopyF, DestroyF)                       \
typedef struct _VTYPE(Type)                                             \
{                                                                       \
	Type *Data;                                                         \
	size_t Length;                                                      \
	size_t Reserved;                                                    \
} _VTYPE(Type);                                                         \
                                                                        \
void VFUN(Type, check_reserved)(VEC(Type) Vec)                          \
{                                                                       \
	if (Vec->Length == Vec->Reserved)                                   \
	{                                                                   \
		Vec->Reserved = 3 * (Vec->Reserved / 2);                        \
		Vec->Data     = (Type*)realloc(Vec->Data,                       \
			Vec->Reserved * sizeof(Type));                              \
	}                                                                   \
}                                                                       \
                                                                        \
VEC(Type) VFUN(Type, New)()                                             \
{                                                                       \
	VEC(Type) vec = (VEC(Type))malloc(sizeof(_VTYPE(Type)));            \
                                                                        \
	vec->Length   = 0;                                                  \
	vec->Reserved = 10;                                                 \
	vec->Data     = (Type*)malloc(vec->Reserved * sizeof(Type));        \
                                                                        \
	return vec;                                                         \
}                                                                       \
                                                                        \
void VFUN(Type, Delete)(VEC(Type) Vec, ...)                             \
{                                                                       \
	va_list args;                                                       \
	va_start(args, Vec);                                                \
                                                                        \
	if (DestroyF != NULL)                                               \
	{                                                                   \
		typedef void (*destroy_t)(Type*, va_list);                      \
		destroy_t destroy = (destroy_t)DestroyF;                        \
                                                                        \
		for (size_t i = 0; i < Vec->Length; ++i)                        \
			destroy(&Vec->Data[i], args);                               \
                                                                        \
	}                                                                   \
                                                                        \
	free(Vec->Data);                                                    \
	free(Vec);                                                          \
	va_end(args);                                                       \
}                                                                       \
                                                                        \
void VFUN(Type, Push)(VEC(Type) Vec, const Type *T, ...)                \
{                                                                       \
	VFUN(Type, check_reserved)(Vec);                                    \
                                                                        \
	if (CopyF != NULL)                                                  \
	{                                                                   \
		typedef Type (*copy_t)(const Type*, va_list);                   \
		copy_t copy = (copy_t)CopyF;                                    \
		va_list args;                                                   \
		va_start(args, T);                                              \
		Vec->Data[Vec->Length++] = copy(T, args);                       \
		va_end(args);                                                   \
	}                                                                   \
	else                                                                \
	{                                                                   \
		Vec->Data[Vec->Length++] = *T;                                  \
	}                                                                   \
}                                                                       \
                                                                        \
Type* VFUN(Type, Begin)(VEC(Type) Vec)                                  \
{                                                                       \
	return Vec->Data;                                                   \
}                                                                       \
                                                                        \
Type* VFUN(Type, End)(VEC(Type) Vec)                                    \
{                                                                       \
	return Vec->Data + Vec->Length;                                     \
}                                                                       \
                                                                        \
Type* VFUN(Type, Find)(VEC(Type) Vec,                                   \
	bool (*pred)(const Type*, va_list), ...)                            \
{                                                                       \
	va_list args;                                                       \
	va_start(args, pred);                                               \
                                                                        \
	for (size_t i = 0; i < Vec->Length; ++i)                            \
	{                                                                   \
		if (pred(&Vec->Data[i], args))                                  \
		{                                                               \
			va_end(args);                                               \
			return &Vec->Data[i];                                       \
		}                                                               \
	}                                                                   \
                                                                        \
	va_end(args);                                                       \
	return VFUN(Type, End)(Vec);                                        \
}                                                                       \
                                                                        \
void VFUN(Type, Remove)(VEC(Type) Vec, Type *Start, ...)                \
{                                                                       \
	if (Start < VFUN(Type, Begin)(Vec) || Start >= VFUN(Type, End)(Vec))\
		return;                                                         \
                                                                        \
	if (DestroyF != NULL)                                               \
	{                                                                   \
		va_list args;                                                   \
		va_start(args, Start);                                          \
                                                                        \
		typedef void (*destroy_t)(Type*, va_list);                      \
		destroy_t destroy = (destroy_t)DestroyF;                        \
		destroy(Start, args);                                           \
                                                                        \
		va_end(args);                                                   \
	}                                                                   \
                                                                        \
	for (; Start != VFUN(Type, End)(Vec); ++Start)                      \
		*Start = *(Start + 1);                                          \
                                                                        \
	--Vec->Length;                                                      \
}


#define DECLARE_VECTOR(Type)                                            \
struct _VTYPE(Type);                                                    \
typedef struct _VTYPE(Type) _VTYPE(Type);                               \
VEC(Type) VFUN(Type, New)();                                            \
void VFUN(Type, Delete)(VEC(Type) Vec, ...);                            \
void VFUN(Type, Push)(VEC(Type) Vec, const Type* T, ...);               \
Type* VFUN(Type, Begin)(VEC(Type) Vec);                                 \
Type* VFUN(Type, End)(VEC(Type) Vec);                                   \
Type* VFUN(Type, Find)(VEC(Type) Vec,                                   \
	bool (*pred)(const Type*, va_list), ...);                           \
void VFUN(Type, Remove)(VEC(Type) Vec, Type *Start, ...);


#endif
