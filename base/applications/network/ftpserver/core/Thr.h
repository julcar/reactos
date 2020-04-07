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
// Thr.h: interface for the CThr class.
//
//////////////////////////////////////////////////////////////////////
#ifndef THR_H
#define THR_H

#ifdef WIN32
  #include <windows.h>
  #include <winbase.h>
  typedef CRITICAL_SECTION thrSync_t;
#else
  #include <pthread.h>
  typedef pthread_mutex_t thrSync_t;
#endif


class CThr  
{
public:
    CThr();
    virtual ~CThr();

    #ifdef WIN32
      int Create(void (* fptr)(void *), void *args);  //WINDOWS
    #else
      int Create(void *(* fptr)(void *), void *args); //UNIX
    #endif
    void Destroy(void);  //called by the running thread (to exit)

    long GetCurrentThrID();

    int InitializeCritSec(thrSync_t *mutex);
    int P(thrSync_t *mutex);
    int V(thrSync_t *mutex);
    int DestroyCritSec(thrSync_t *mutex);
};

#endif //THR_H
