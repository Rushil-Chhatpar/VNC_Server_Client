#ifdef _WIN32
#include <winsock2.h>        // For sockaddr_in
#include <windows.h>         // For CRITICAL_SECTION, CONDITION_VARIABLE
#include <time.h>            // For timeval (if needed)
#endif

#include <memory>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <string.h>