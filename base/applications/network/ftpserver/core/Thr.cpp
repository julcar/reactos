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

  #include <process.h>

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

int CThr::Create(void (* fptr)(void *), void *args)
{

    if ((int)_beginthread(fptr,0,args) == -1)  //WINDOWS implementation
        return(0);
    else
        return(1);
}

//////////////////////////////////////////////////////////////////////
// Terminates a thread created by Create()
//
// Return : VOID
//
void CThr::Destroy(void)
{

      _endthread(); //WINDOWS implementation
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

      return(GetCurrentThreadId());
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

      InitializeCriticalSection(mutex);

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

      EnterCriticalSection(mutex);

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

      LeaveCriticalSection(mutex);

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

      DeleteCriticalSection(mutex);

    return(1);
}
