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
// Thr.cpp: implementation of the CThr class.
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
  #include <process.h>
#else
  #include <pthread.h>
#endif

#include "Thr.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CThr::CThr()
{

}

CThr::~CThr()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Starts a function as a new thread
//
// [in] fptr : Pointer to the function to start in the thread.
// [in] args : Arguments to the function
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: For WINDOWS the passed-in function must return "void"
//       For UNIX the passed-in function must return "void *"
//
#ifdef WIN32
int CThr::Create(void (* fptr)(void *), void *args)
{

    if (_beginthread(fptr,0,args) == -1)  //WINDOWS implementation
        return(0);
    else
        return(1);
}
#else
int CThr::Create(void *(* fptr)(void *), void *args)
{
    pthread_t tid;

    if (pthread_create(&tid,NULL,fptr,args) != 0) { //UNIX implementation
        return(0);
    } else {
        pthread_detach(tid); //indicates resources can be reclaimed on exit
        return(1);
    }

}
#endif

//////////////////////////////////////////////////////////////////////
// Terminates a thread created by Create()
//
// Return : VOID
//
void CThr::Destroy(void)
{

    #ifdef WIN32
      _endthread(); //WINDOWS implementation
    #else
      void *retval;
      pthread_exit((void *)(&retval));  //UNIX implementation
    #endif
}

//////////////////////////////////////////////////////////////////////
// Initializes the critical section
//
// [out] mutex : Pointer to the critical section object
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
long CThr::GetCurrentThrID()
{

    #ifdef WIN32
      return(GetCurrentThreadId());
    #else
      return((long)pthread_self());
    #endif
}

//////////////////////////////////////////////////////////////////////
// Initializes the critical section
//
// [out] mutex : Pointer to the critical section object
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CThr::InitializeCritSec(thrSync_t *mutex)
{

    #ifdef WIN32
      InitializeCriticalSection(mutex);
    #else
      pthread_mutex_init(mutex,NULL);
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////
// Enter the critical section
//
// [in] mutex : Pointer to the critical section object
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CThr::P(thrSync_t *mutex)
{

    #ifdef WIN32
      EnterCriticalSection(mutex);
    #else
      pthread_mutex_lock(mutex);
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Leave the critical section
//
// [in] mutex : Pointer to the critical section object
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CThr::V(thrSync_t *mutex)
{

    #ifdef WIN32
      LeaveCriticalSection(mutex);
    #else
      pthread_mutex_unlock(mutex);
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Destroy the critical section
//
// [in] mutex : Pointer to the critical section object
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CThr::DestroyCritSec(thrSync_t *mutex)
{

    #ifdef WIN32
      DeleteCriticalSection(mutex);
    #else
      pthread_mutex_destroy(mutex);
    #endif

    return(1);
}
