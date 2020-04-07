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
// SiteInfo.h: interface for the CSiteInfo class.
//
// This class is used to store and access information about
// the server such as configuration information or getting a list
// of which users are online.
//
// NOTE: Only 1 of this class should be used.
//
//////////////////////////////////////////////////////////////////////
#ifndef SITEINFO_H
#define SITEINFO_H

#include <time.h>

#include "Thr.h"
#include "Log.h"

#ifndef NULL
#define NULL 0
#endif

#define SITEINFO_MAXPWLINE 64      //max size of a string parameter in the password (passwd) file
#define SITEINFO_MAXPATH   265     //max size of a path

#define SITEINFO_ANONYMOUSGROUPNUM 99   //the group number used to specify anonymous (no Password)

    //The default access permissions (applied to directories only)
    //l = list directory contents
    //v = recursive directory listing
    //c = change current working directory (CWD)
    //p = print working dir (PWD)
    //s = system info (SYST and STAT)
    //t = SITE command
    //d = download files
    //u = upload files
    //o = remove files
    //a = rename files/directories
    //m = make directories
    //n = remove directories
    //r = "read" -> d
    //w = "write" -> uoamn
    //x = "execute" -> lvcpst
#define SITEINFO_DEFAULTPERMISSIONS "lvcpstduoamn"

    //Formatting string for the log date field
    //This uses the formatting for strftime() except the "%O"
    //specifier was added to display the UTC offset in minutes
#define SITEINFO_DEFDATEFORMAT "%d/%m/%Y:%H:%M:%S"

    //Formatting string for the program log file
    // %D -> date (format is specified using SetDateFormatStr())
    // %S -> sub-system (where in the prog the log is coming from)
    // %M -> message (message that is passed in)
#define SITEINFO_DEFPROGFORMAT "%D;%M"
    //Default program log file name
    //This uses the same formatting as SITEINFO_DEFDATEFORMAT
#define SITEINFO_DEFPROGLOG "ftpd.log"

    //Formatting string for the access log file
    // %d -> date (format is specified using SetDateFormatStr())
    // %s -> server IP
    // %l -> server listening port
    // %c -> client IP
    // %p -> client port
    // %u -> user name
    // %m -> FTP command (method)
    // %a -> argument to the command
    // %r -> 3-digit response code from the server
    // %t -> response text from the server
#define SITEINFO_DEFACCESSFORMAT "%d;%s;%l;%c;%p;%u;%m;%a;%r;%t"
    //Default access log file name
    //This uses the same formatting as SITEINFO_DEFDATEFORMAT
#define SITEINFO_DEFACCESSLOG "%d%m%Y_access.log"

    //Log levels
#define SITEINFO_LOGLEVELNONE    0  //do not log anything
#define SITEINFO_LOGLEVELDISPLAY 1  //only display the logs on the screen
#define SITEINFO_LOGLEVELNORMAL  2  //only write to the log file
#define SITEINFO_LOGLEVELDEBUG   3  //write to the log and the screen

typedef struct {
    char username[SITEINFO_MAXPWLINE];      //client's username
    char passwordraw[SITEINFO_MAXPWLINE];   //raw password (maybe encrypted)
    char usernumber[16];                    //string version of the user number (int)
    char groupnumber[16];                   //string version of the group number (int)
    char dateadded[SITEINFO_MAXPWLINE];     //date the user was added
    char homedir[SITEINFO_MAXPATH];         //user's home directory
} siteinfoPWInfo_t;

    //Global variable used to signal the program to exit
extern int siteinfoFlagQuit;


class CSiteInfo
{
public:
    CSiteInfo(char *progname, char *progversion, char *coreversion, char *sitedefroot = NULL);
    virtual ~CSiteInfo();

        //user functions
    virtual int CheckUserProp(char *username);

        //password functions
    virtual siteinfoPWInfo_t *GetPWInfo(char *username);
    virtual void FreePWInfo(siteinfoPWInfo_t *pwinfo);

        //permissions functions
    virtual int CheckPermissions(char *username, char *path, char *pflags);

        //path functions
    virtual char *BuildPathRoot(char *subdir, char *filename = NULL);
    virtual char *BuildPathHome(char *subdir, char *filename = NULL);
    virtual void FreePath(char *path);

        //logging functions
    virtual void SetDateFormatStr(char *formatstr);
    virtual void SetProgLoggingLevel(int level);
    virtual void SetProgLogFormatStr(char *formatstr);
    virtual int SetProgLogName(char *logfile, char *basepath = NULL);
    virtual int WriteToProgLog(char *subsystem, char *msgformat, ...);
    virtual void SetAccessLoggingLevel(int level);
    virtual void SetAccessLogFormatStr(char *formatstr);
    virtual int SetAccessLogName(char *logfile, char *basepath = NULL);
    virtual int WriteToAccessLog(char *sip, char *sp, char *cip, char *cp, char *user, char *cmd, char *arg, char *status, char *text);

        //FTP data connection functions
    virtual int SetDataPortRange(unsigned short startport, unsigned short endport);
    virtual int GetDataPort(char *bindport, int maxportlen, int *sdptr, char *bindip);
    virtual int FreetDataPort(int sd);

        //builds a full line for the LIST command
    virtual char *BuildFullListLine(char *listline, int maxlinesize, char *filepath, char *thisyear = NULL);

        //used to handle added (uploaded) files
    virtual int AddFile(char *filepath, char *username, int size = -1);

        //transfer rate functions
    virtual void SetCurrentXferRate(int userid, long bytespersec);
    virtual int GetMaxDLSpeed(char *username);
    virtual int GetMaxULSpeed(char *username);

        //file transfer information functions
    virtual int UpdateDLStats(int userid, char *username, int bytes, int bytespersec);
    virtual int UpdateULStats(int userid, char *username, int bytes, int bytespersec);

        //credits functions
    virtual int AddCredits(char *filepath, int userid, char *username, int bytes, int flagupload);
    virtual int RemoveCredits(char *filepath, int userid, char *username, int bytes, int flagupload);

        //checks if a user is allowed to use FXP
        //(use a different IP address for data than their control connection).
    virtual int IsAllowedFXP(char *username, int flagupload, int flagpasv);

        //SSL key/certificate functions
    virtual char *GetPrivKey(int *privkeysize);
    virtual void FreePrivKey(char *privkeybuf);
    virtual char *GetCert(int *certsize);
    virtual void FreeCert(char *certbuf);
    virtual int GetPrivKeyPW(char *pwbuff, int maxpwbuff);

        //time functions
    char *GetTimeString(char *buffer, int maxsize, char *formatstring, long ctime = 0, int flaggmt = 0);
    struct tm *GetTimeR(long ctime, struct tm *tmoutput, int flaggmt);
    int GetGMTOffsetMinutes();
    int GetGMTHour(int localhour, int gmtoffsetmin);
    int GetGMTMinutes(int localmin, int gmtoffsetmin);

        //program information functions
    char *GetProgName(char *buffer, int maxsize);
    char *GetProgVersion(char *buffer, int maxsize);
    char *GetCoreVersion(char *buffer, int maxsize);

public:
        //Used in CFtpsXfer to limit the transfer speed for the site
    long m_maxdlspeed;  //max download speed (bytes/sec)
    long m_maxulspeed;  //max upload speed (bytes/sec)
    long m_bytessent;   //num bytes sent in the current transfer cycle (downloading)
    long m_bytesrecv;   //num bytes recv in the current transfer cycle (uploading)
    unsigned long m_sendtimer;  //used for sending rate timing (downloading)
    unsigned long m_recvtimer;  //used for receiving rate timing (uploading)
    thrSync_t m_mutexsend;  //used for synchronization when limiting download speed
    thrSync_t m_mutexrecv;  //used for synchronization when limiting upload speed

private:
    char *BuildProgLogLine(char *date, char *subsystem, char *message);
    char *BuildAccessLogLine(char *date, char *sip, char *sp, char *cip, char *cp, char *user, char *cmd, char *arg, char *status, char *text);

protected:
    CThr m_thr;         //contains thread and mutex functions

    CLog m_proglog;     //program log file
    CLog m_accesslog;   //access log file

        //The default root directory for the site
    char *m_sitedefroot;

        //Mutex for the program log file
    thrSync_t m_mutexproglog;
        //Mutex for the access log file
    thrSync_t m_mutexaccesslog;

        //Program Information
    char m_progname[64];        //name of the program (Ex. IndiFTPD)
    char m_progversion[32];     //program version (as a string)
    char m_coreversion[32];     //core version (as a string)
                                //This is the version of the base code.

        //stores the format strings for the program and access log file names
    char *m_proglogname;
    char *m_accesslogname;

        //Format strings for the log files
    char m_dateformat[64];      //format string for log date field
    char m_progformat[64];      //format string for the program log file
    char m_accessformat[64];    //format string for the access log file

        //used to set the logging levels
    int m_progloglevel;
    int m_accessloglevel;
};

#endif //SITEINFO_H
