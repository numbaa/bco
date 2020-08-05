#pragma once
#ifdef _WIN32
#include "detail/task_windows.h"
#else
#include "detail/task_linux.h"
#endif