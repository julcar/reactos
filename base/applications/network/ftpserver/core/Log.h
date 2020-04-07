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
// Log.h: interface for the CLog class.
//
//////////////////////////////////////////////////////////////////////
#ifndef LOG_H
#define LOG_H

    //The default log file name
#define LOG_DEFLOGFILE "out.log"

    //Log levels
#define LOG_LEVELDISPLAY 0  //only display the logs on the screen
#define LOG_LEVELNORMAL  1  //only write to the log file
#define LOG_LEVELDEBUG   2  //write to the log and the screen


class CLog
{
public:
    CLog(const char *logfile = NULL, int minflushtimesec = 5);
    virtual ~CLog();

    int WriteToLog(const char *message, int level = LOG_LEVELNORMAL);

    void SetLogFilename(const char *logfile, const char *basepath = NULL);
    char *GetLogFilename(char *buffer, int maxsize);

    void SetMinFlushTime(int minflushtimesec);
    int GetMinFlushTime();

private:
    char *m_logfile;    //name of the current logfile

    FILE *m_fd;         //descriptor for the log file

    long m_lastflushtime;   //stores the last time the logfile was flushed
    long m_minflushtimesec; //minimum number of seconds between flushes
};

#endif //LOG_H
