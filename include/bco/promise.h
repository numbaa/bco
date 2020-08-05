#pragma once
#ifdef _WIN32
#include "detail/promise_windows.h"
#else
#include "detail/promise_linux.h"
#endif