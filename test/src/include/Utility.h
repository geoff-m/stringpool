#pragma once
#include "stringpool/stringpool.h"

[[nodiscard]] bool sameSign(int x, int y);

void expectSameSign(int x, int y);


void expectEqual(const stringpool::string_handle& interned, const char* str);

void expectEqual(const stringpool::string_handle& x, const stringpool::string_handle& y);

void expectLength(size_t length, const stringpool::string_handle& interned);
