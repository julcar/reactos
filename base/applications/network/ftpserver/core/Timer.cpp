// Copyright (C) 2003 by Oren Avissar
// (go to www.dftpd.com for more information)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//
// Timer.cpp: implementation of the CTimer class.
//
//////////////////////////////////////////////////////////////////////
#ifdef WIN32
  #include <windows.h>
  #include <winbase.h>
#else
  #include <unistd.h>
  #include <sys/time.h> //for gettimeofday()
#endif

#include "Timer.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTimer::CTimer()
{

}

CTimer::~CTimer()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Gets the current timer value.  The value returned is a random
// counter that increments every millisecond
//
// Return : Current value of the timer.
//
unsigned long CTimer::Get() {
    unsigned long rettime = 1;

    #ifdef WIN32
      FILETIME systemtimeasfiletime;
      LARGE_INTEGER litime;

      GetSystemTimeAsFileTime(&systemtimeasfiletime);
      memcpy(&litime,&systemtimeasfiletime,sizeof(LARGE_INTEGER));
      litime.QuadPart /= 10000;  //convert to milliseconds
      litime.QuadPart &= 0xFFFFFFFF;    //keep only the low part
      rettime = (unsigned long)(litime.QuadPart);
    #else
      struct timeval tv;
      struct timezone tz;
      unsigned long t1, t2;

      gettimeofday(&tv,&tz);
      t1 = (tv.tv_sec & 0x003FFFFF) * 1000; //convert sec to milliseconds
      t2 = (tv.tv_usec / 1000) % 1000;      //convert usec to milliseconds
      rettime = t1 + t2;
    #endif

    return(rettime);
}

//////////////////////////////////////////////////////////////////////
// Computes the absolute difference (accounting for overflow) between
// "timestart" and "timeend."  The returned value corresponds to the
// number of milliseconds between them.
//
// [in] timestart : timer value at the start of an interval
// [in] timeend   : timer value at the end of an interval
//
// Return : The absolute difference between the timer values
//
unsigned long CTimer::Diff(unsigned long timestart, unsigned long timeend) {
    unsigned long timediff = 0;

    if (timestart > timeend)
        timediff = ((long)(-1) - timestart) + timeend + 1;    //if overflow
    else
        timediff = timeend - timestart;

    return(timediff);
}

//////////////////////////////////////////////////////////////////////
// Computes the difference between "timestart" and "timeend" in units
// of seconds.
//
// [in] timestart : timer value at the start of an interval
// [in] timeend   : timer value at the end of an interval
//
// Return : The difference in seconds between the timer values as a
//          floating point number.
//
float CTimer::DiffSec(unsigned long timestart, unsigned long timeend) {
    unsigned long timediff;
    float timesec;

    timediff = Diff(timestart,timeend);
    timesec = timediff / (float)1000.00;

        //always return at least 0.01 sec;
    if (timesec < 0.01)
        timesec = (float)0.01;

    return(timesec);
}

//////////////////////////////////////////////////////////////////////
// Go to sleep for "numms" milliseconds
//
// [in] numms : Number of milliseconds to sleep for.
//
// Return : VOID
//
void CTimer::Sleep(unsigned long numms) {

    #ifdef WIN32
      SleepEx(numms,false);
    #else
      usleep(numms*1000);   //usleep uses microseconds
    #endif
}
