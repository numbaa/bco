#pragma once

#ifdef _WIN32
#include "detail/proactor_windows.h"
#else
#include "detail/proactor_linux.h"
#endif