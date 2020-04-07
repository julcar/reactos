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
// CmdLine.cpp: implementation of the CCmdLine class.
//
// This class is used to store and manipulate a command line input.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CmdLine.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Constructor for the CCmdLine class.
//
// [in] cmdlinesize : Size of the buffer to use for the command line.
//
CCmdLine::CCmdLine(int cmdlinesize /*=1024*/)
{

    m_cmdlinesize = cmdlinesize;

    if (m_cmdlinesize != 0) {
        m_cmdline = (char *)malloc(m_cmdlinesize);
    } else {
        m_cmdline = NULL;
    }

    m_cmdbuffer = NULL;
    m_argptr = NULL;
}

CCmdLine::~CCmdLine()
{

    if (m_cmdline != NULL)
        free(m_cmdline);

    if (m_cmdbuffer != NULL)
        free(m_cmdbuffer);

    if (m_argptr != NULL)
        free(m_argptr);
}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Load the command line buffer.
//
// [in] cmdline : String to load the command line with.
//
// Return : VOID
//
void CCmdLine::SetCmdLine(const char *cmdline)
{
    
    if (m_cmdline == NULL)
        return;

    strncpy(m_cmdline,cmdline,m_cmdlinesize-1);
    m_cmdline[m_cmdlinesize-1] = '\0';
}

//////////////////////////////////////////////////////////////////////
// Get the current command line buffer.
//
// [out] outbuff    : String to load the command line into.
// [in] maxbuffsize : Max size of the "outbuff" string.
//
// Return : VOID
//
void CCmdLine::GetCmdLine(char *outbuff, int maxbuffsize)
{

    if (m_cmdline == NULL || outbuff == NULL)
        return;

    strncpy(outbuff,m_cmdline,maxbuffsize-1);
    outbuff[maxbuffsize-1] = '\0';
}

//////////////////////////////////////////////////////////////////////
// Set the size of the command line buffer.
//
// [in] cmdlinesize : Size the command line buffer should be set to.
//
// Return : VOID
//
// NOTE: Setting the cmd line size may change the m_cmdline pointer.
//
void CCmdLine::SetCmdLineSize(int cmdlinesize)
{

    if (m_cmdline != NULL)
        free(m_cmdline);    //free the previous command line

    if ((m_cmdline = (char *)malloc(cmdlinesize)) == NULL)
        m_cmdlinesize = 0;
    else
        m_cmdlinesize = cmdlinesize;
}

//////////////////////////////////////////////////////////////////////
// Get the size of the current command line buffer.
//
// Return : Size of the current command line.
//
int CCmdLine::GetCmdLineSize()
{
    
    return(m_cmdlinesize);
}

//////////////////////////////////////////////////////////////////////
// Parses the command line that is currently stored in the internal
// command line buffer.  This must be called to process a new command
// line.
//
// [out] argc      : Number of command line arguments.
// [in] delimiter  : Delimiter to use when parsing the command line.
// [in] usepadding : If == 0, multiple delimiters count as 1 delimiter
//
// Return : On successs, an array of pointers for the command line
//          arguments is returned.  On failure, NULL is returned.
//
char **CCmdLine::ParseCmdLine(int *argc, char delimiter /*=' '*/, int usepadding /*=0*/)
{
    char *ptr, delimiter2;
    int numargs, delimiterflag;

    if (argc == NULL)
        return(NULL);

    *argc = 0;  //initialize the number of arguments to 0

        //check if the current command line is valid
    if (m_cmdline == NULL || m_cmdlinesize == 0)
        return(NULL);

        //make sure the command line is NULL terminated
    m_cmdline[m_cmdlinesize-1] = '\0';

    if (m_cmdbuffer != NULL)
        free(m_cmdbuffer);  //free the old cmd buffer if necessary
    m_cmdbuffer = NULL;
    if (m_argptr != NULL)
        free(m_argptr);     //free the old argument pointer if necessary
    m_argptr = NULL;

    if ((m_cmdbuffer = (char *)malloc(strlen(m_cmdline)+1)) == NULL)
        return(NULL);
    strcpy(m_cmdbuffer,m_cmdline);

        //if the delimiter is a ' ' also make '\t' a delimiter
    delimiter2 = (delimiter == ' ') ? '\t' : '\0';

        //get the number of parameters in the command line
    numargs = 0;
    ptr = m_cmdbuffer;
    delimiterflag = 1;
    if (usepadding != 0) numargs++; //the first argument
    while (*ptr != '\0') {
        if (*ptr == delimiter || *ptr == delimiter2) {
            if (usepadding != 0) numargs++; //a new argument was encountered            
            delimiterflag = 1;  //a "delimiter" was encountered
        } else if (*ptr != delimiter && *ptr != delimiter2 && delimiterflag != 0) {
            if (usepadding == 0) numargs++; //a new argument was encountered            
            delimiterflag = 0;  //current character is not a "delimiter"
        }
        ptr++;
    }

    if (numargs == 0) {     //if no arguments were present
        m_argptr = NULL;
        return(NULL);
    }

    if ((m_argptr = (char **)malloc(sizeof(char *)*(numargs+1))) == NULL)
        return(NULL);

        //set all the delimiters to '\0'
    numargs = 0;
    ptr = m_cmdbuffer;
    delimiterflag = 1;
    if (usepadding != 0) m_argptr[numargs++] = ptr; //add the argument to the list
    while (*ptr != '\0') {
        if (*ptr == delimiter || *ptr == delimiter2) {
            *ptr = '\0';    //set the "delimiter" to '\0'
            if (usepadding != 0) m_argptr[numargs++] = ptr + 1; //add the argument to the list
            delimiterflag = 1;  //a "delimiter" was encountered
        } else if (*ptr != delimiter && *ptr != delimiter2 && delimiterflag != 0) {
            if (usepadding == 0) m_argptr[numargs++] = ptr;     //add the argument to the list
            delimiterflag = 0;  //current character is not a "delimiter"
        }
        ptr++;
    }
    m_argptr[numargs] = NULL;   //terminate the list with a NULL entry

    *argc = numargs;
    return(m_argptr);
}

//////////////////////////////////////////////////////////////////////
// Returns a buffer containing the combined arguments in argv starting
// with argv[startarg].  Any arguments starting with "optionchar" will
// be ignored.  The buffer returned has one extra byte in case a
// trailing slash (or other char) needs to be added.
//
// [in] argc       : Number of command line arguments.
// [in] argv       : Array of command line arguments.
// [in] optionchar : Character used to indicate an option on the
//                   command line.  If == 0, none is used.
// [in] startarg   : Argument to start will (first arg in output)
//
// Return : On successs, a sting containing the combined arguments is
//          returned.  On failure, NULL is returned.
//
// NOTE: The returned buffer must be free().
//
char *CCmdLine::CombineArgs(int argc, char **argv, char optionchar /*=0*/, int startarg /*=1*/)
{
    char *buffer;
    int i, flagspace = 0, pathlen = 0;

    for (i = startarg; i < argc; i++) {
        if (*argv[i] != optionchar)     //if the argument is not an option
            pathlen += (strlen(argv[i]) + 1);   //increment the path length
    }

        //NOTE: add +2 in case a trailing character is added
    if ((buffer = (char *)malloc(pathlen+2)) == NULL)
        return(NULL);

        //combine all the arguments
    *buffer = '\0';
    for (i = startarg; i < argc; i++) {
        if (*argv[i] != optionchar) {
            if (flagspace != 0)
                strcat(buffer," ");
            strcat(buffer,argv[i]);
            flagspace = 1;
        }
    }

    return(buffer);
}

//////////////////////////////////////////////////////////////////////
// Frees the line returned by CombineArgs().
//
// [in] argbuffer : Line returned by CombineArgs().
//
// Return : VOID
//
void CCmdLine::FreeCombineArgs(char *argbuffer)
{

    if (argbuffer != NULL)
        free(argbuffer);
}
