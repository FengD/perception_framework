// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: macros

#pragma once

// // A macro to disallow the copy constructor and operator= functions
// // This should be used in the priavte:declarations for a class
// #define DISALLOW_COPY_AND_ASSIGN(TypeName) \
// TypeName(TypeName&) = delete;              \
// void operator=(TypeName) = delete;

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)