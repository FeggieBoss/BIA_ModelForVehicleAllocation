#ifndef _MAIN
#define _MAIN

#define DEBUG_MODE

#ifdef DEBUG_MODE
#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;
#endif

#include "Highs.h"

#include <climits>

// seconds between Jan-1-1900(Serial Date) and Jan-1-1970(UNIX Time)
const long long serial_unix_offset = 2208988800 + 2*24*60*60 + 3*60*60; 

#endif // _MAIN
