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
// Log.cpp: implementation of the CLog class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Log.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Constructor for the CLog class.
//
// [in] logfile : Name of the file to log to.
//
CLog::CLog(const char *logfile /*=NULL*/, int minflushtimesec /*=5*/)
{

        //initialize the log file name
    if (logfile != NULL) {
        if ((m_logfile = (char *)malloc(strlen(logfile)+1)) != NULL)
            strcpy(m_logfile,logfile);
    } else {
        if ((m_logfile = (char *)malloc(sizeof(LOG_DEFLOGFILE)+1)) != NULL)
            strcpy(m_logfile,LOG_DEFLOGFILE);
    }

    m_fd = NULL;    //initialize the descriptor for the log file

        //initialize the last logfile flush time (cause an immediate flush)
    m_lastflushtime = time(NULL) - minflushtimesec;
        //set the min number of sec between flushes (log is only flushed on write)
    m_minflushtimesec = minflushtimesec;
}

CLog::~CLog()
{

    if (m_logfile != NULL)
        free(m_logfile);

    if (m_fd != NULL)
        fclose(m_fd);   //close the logfile if necessary
}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Writes the string "massage" to the log file.
//
// [in] message : Message to write into the log.
// [in] level   : Logging level to use (see Log.h).
//
// Return : On success 1 is returned.  On failure 0 is returned.
//                
int CLog::WriteToLog(const char *message, int level /*=LOG_LEVELNORMAL*/)
{

    if (message == NULL)
        return(0);

        //if level is set to debugging or display, write to the screen
    if (level == LOG_LEVELDEBUG || level == LOG_LEVELDISPLAY)
        printf("%s\n",message);

    if (m_logfile == NULL)
        return(0);

        //if level is set to display, do not write to the log file
    if (level == LOG_LEVELDISPLAY)
        return(1);

        //open a descriptor for the logfile if necessary
    if (m_fd == NULL) {
        if ((m_fd = fopen(m_logfile,"at+")) == NULL)
            return(0);
    }

        //write the message to the log file
    fprintf(m_fd,"%s\n",message);

        //flush the logfile if necessary
    if ((time(NULL) - m_lastflushtime) >= m_minflushtimesec) {
        fflush(m_fd);
        m_lastflushtime = time(NULL);
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Set the name of the current log file.
// "logfile" is the name of the file and "basepath" is the directory
// the file will be written in.
//
// [in] logfile  : Name of the log file.
// [in] basepath : Path to write the file into (default is local dir)
//
// Return : VOID
//
// Ex. logfile = access.log, basepath = /var/log/
//     The log will be written to "/var/log/access.log"
//
// NOTE: This function assumes "logfile" and "basepath" are valid.
//       "basepath" must end in a "/" (UNIX) or "\" (WINDOWS).
//
void CLog::SetLogFilename(const char *logfile, const char *basepath /*=NULL*/)
{
    char *tmpname;
    int len;

    if (logfile == NULL)
        return;

    len = strlen(logfile) + 1;
    if (basepath != NULL)
        len += strlen(basepath);

    if ((tmpname = (char *)malloc(len)) != NULL) {
        if (basepath != NULL) {
            strcpy(tmpname,basepath);
            strcat(tmpname,logfile);
        } else {
            strcpy(tmpname,logfile);
        }
            //check if the logfile name changed
        if (strcmp(m_logfile,tmpname) != 0) {
            if (m_fd != NULL)
                fclose(m_fd);   //free the old descriptor if necessary
            m_fd = NULL;        //a new fd will be created in the next call to WriteToLog()
            if (m_logfile != NULL)
                free(m_logfile);    //free the old log file name if necessary
            m_logfile = tmpname;    //set the new log file name
        } else {
            free(tmpname);  //log file name did not change
        }
    }
}

//////////////////////////////////////////////////////////////////////
// Get the name of the current log file.
//
// [out] buffer  : Stores the name of the current log file.
// [in]  maxsize : Maximum length string "buffer" can hold.
//
// Return : Pointer to "buffer."
//
char *CLog::GetLogFilename(char *buffer, int maxsize)
{

    if (buffer == NULL || maxsize == 0)
        return(buffer);

    strncpy(buffer,m_logfile,maxsize-1);
    buffer[maxsize-1] = '\0';

    return(buffer);
}

//////////////////////////////////////////////////////////////////////
// Set the minimum nuber of seconds between logfile flushes
// (when the internal buffer is written out to the logfile).
//
// [in] minflushtimesec : Minimum number of seconds between flushes.
//
// Return : VOID
//
// NOTE: The logfile is only flushed on calls to WriteToLog().
//
void CLog::SetMinFlushTime(int minflushtimesec)
{
    
    m_minflushtimesec = minflushtimesec;
}

//////////////////////////////////////////////////////////////////////
// Gets the current value for the minimum number of seconds beteen
// logfile flushes.
//
// Return : Minimum number of seconds between logfile flushes.
//
int CLog::GetMinFlushTime()
{

    return(m_minflushtimesec);
}

//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

