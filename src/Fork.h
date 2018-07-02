#pragma once
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <winternl.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <process.h>


DWORD fork();


