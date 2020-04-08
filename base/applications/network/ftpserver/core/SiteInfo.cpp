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
// SiteInfo.cpp: implementation of the CSiteInfo class.
//
// This class is used to store and access information about
// the server such as configuration information or getting a list
// of which users are online.
//
// NOTE: Only 1 of this class should be used.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  //for ftime (struct timeb)
#include <sys/timeb.h>  //for ftime (struct timeb)
#include <stdarg.h>     //for va_list, etc.

#ifdef WIN32
  #include <windows.h>  //for FileTimeToSystemTime()
#endif

#include "SiteInfo.h"
#include "FSUtils.h"
#include "StrUtils.h"

    //Global variable used to signal the program to exit
int siteinfoFlagQuit = 0;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSiteInfo::CSiteInfo(char *progname, char *progversion, char *coreversion, char *sitedefroot /*=NULL*/)
{
    CFSUtils fsutils;

        //Initialize the program information
    if ((progname != NULL && progversion != NULL) || (coreversion != NULL)) {
        strncpy(m_progname,progname,sizeof(m_progname)-1);
        m_progname[sizeof(m_progname)-1] = '\0';
        strncpy(m_progversion,progversion,sizeof(m_progversion)-1);
        m_progversion[sizeof(m_progversion)-1] = '\0';
        strncpy(m_coreversion,coreversion,sizeof(m_coreversion)-1);
        m_coreversion[sizeof(m_coreversion)-1] = '\0';
    } else {
        strcpy(m_progname,"MYFTPD");
        strcpy(m_progversion,"1.0.0");
        strcpy(m_coreversion,"1.0.0");
    }

        //Set the default root directory for the site
    if (sitedefroot != NULL) {
        if ((m_sitedefroot = (char *)malloc(strlen(sitedefroot)+2)) != NULL) {
            strcpy(m_sitedefroot,sitedefroot);
                //make sure the path ends in a slash (if its not empty)
            if (*m_sitedefroot != '\0')
                fsutils.CheckSlashEnd(m_sitedefroot,strlen(sitedefroot)+2);
        }
    } else {
        if ((m_sitedefroot = (char *)malloc(SITEINFO_MAXPATH)) != NULL) {
                //by default use the CWD of the server
            if (fsutils.GetCWD(m_sitedefroot,SITEINFO_MAXPATH) == 0) {
                free(m_sitedefroot);
                m_sitedefroot = NULL;
            } else {
                    //make sure the path ends in a slash
                fsutils.CheckSlashEnd(m_sitedefroot,SITEINFO_MAXPATH);
            }
        }
    }

        //Initialize the variables used to limit the site transfer speed
    m_maxdlspeed = 0;  //max download speed (bytes/sec)
    m_maxulspeed = 0;  //max upload speed (bytes/sec)
    m_bytessent = 0;   //num bytes sent in the current transfer cycle
    m_bytesrecv = 0;   //num bytes recv in the current transfer cycle
    m_sendtimer = 0;   //used for sending rate timing (downloading)
    m_recvtimer = 0;   //used for receiving rate timing (uploading)
    m_thr.InitializeCritSec(&m_mutexsend);
    m_thr.InitializeCritSec(&m_mutexrecv);

        //Initialize the formatting strings
    strcpy(m_dateformat,SITEINFO_DEFDATEFORMAT);
    strcpy(m_progformat,SITEINFO_DEFPROGFORMAT);
    strcpy(m_accessformat,SITEINFO_DEFACCESSFORMAT);

        //Initialize the Log file names
    if ((m_proglogname = (char *)malloc(strlen(SITEINFO_DEFPROGLOG)+1)) != NULL)
        strcpy(m_proglogname,SITEINFO_DEFPROGLOG);      //program log file name
    if ((m_accesslogname = (char *)malloc(strlen(SITEINFO_DEFACCESSLOG)+1)) != NULL)
        strcpy(m_accesslogname,SITEINFO_DEFACCESSLOG);  //access log file name

        //Initialize the logging levels
    m_progloglevel = SITEINFO_LOGLEVELNORMAL;
    m_accessloglevel = SITEINFO_LOGLEVELNORMAL;

        //initialize the logfile flush frequencies
    m_proglog.SetMinFlushTime(0);   //do not buffer (write to the file immediately)
    m_accesslog.SetMinFlushTime(5); //buffer the log for 5 seconds (avoid excessive writing)

        //Initialize mutexes
    m_thr.InitializeCritSec(&m_mutexproglog);
    m_thr.InitializeCritSec(&m_mutexaccesslog);
}

CSiteInfo::~CSiteInfo()
{

    if (m_sitedefroot != NULL)
        free(m_sitedefroot);

    if (m_proglogname != NULL)
        free(m_proglogname);
    if (m_accesslogname != NULL)
        free(m_accesslogname);

        //destroy site transfer speed mutex
    m_thr.DestroyCritSec(&m_mutexsend);
    m_thr.DestroyCritSec(&m_mutexrecv);

        //destroy mutexes
    m_thr.DestroyCritSec(&m_mutexproglog);
    m_thr.DestroyCritSec(&m_mutexaccesslog);
}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Functions used load/read the user
// properties files
////////////////////////////////////////

int CSiteInfo::CheckUserProp(char *username)
{

    return(1);  //allow all users by default
}

////////////////////////////////////////
// Functions used to access the password
// (passwd) file.
////////////////////////////////////////

siteinfoPWInfo_t *CSiteInfo::GetPWInfo(char *username)
{
    siteinfoPWInfo_t *pwinfo = NULL;
    CStrUtils strutils;

    if ((pwinfo = (siteinfoPWInfo_t *)calloc(sizeof(siteinfoPWInfo_t),1)) == NULL)
        return(NULL);

        //set the group to anonymous (no password required)
    sprintf(pwinfo->groupnumber,"%d",SITEINFO_ANONYMOUSGROUPNUM);

        //set the home directory
    if (m_sitedefroot != NULL)
        strutils.SNPrintf(pwinfo->homedir,sizeof(pwinfo->homedir),"%s",m_sitedefroot);

    return(pwinfo);
}

void CSiteInfo::FreePWInfo(siteinfoPWInfo_t *pwinfo)
{

    if (pwinfo != NULL)
        free(pwinfo);
}

////////////////////////////////////////
// Functions used to check the user's
// permissions.
////////////////////////////////////////

    //Returns 1 if any of the "pflags" are present in the user's permissions
int CSiteInfo::CheckPermissions(const char *username, const char *path, const char *pflags)
{
    const char *ptr;

    if (pflags == NULL)
        return(0);

    for (ptr = pflags; *ptr != '\0'; ptr++) {
        if (strchr(SITEINFO_DEFAULTPERMISSIONS,*ptr) != NULL)
            return(1);
    }

    return(0);
}

////////////////////////////////////////
// Create various server paths
////////////////////////////////////////

    //"subdir" is the sub-directory of the root path of the site
    //that should be added to the path.
    //"filename" is used to specify a filename within the directory.
    //Example root path: /usr/ftpserver/
char *CSiteInfo::BuildPathRoot(char *subdir, char *filename /*=NULL*/)
{
    CFSUtils fsutils;
    char *path;
    int len;

    if (m_sitedefroot == NULL || subdir == NULL)
        return(NULL);

        //+1 in case need ending '/'
    len = strlen(m_sitedefroot) + strlen(subdir) + ((filename != NULL) ? strlen(filename) : 0) + 1;

    if ((path = (char *)malloc(len+1)) == NULL)
        return(NULL);

        //use the default root dir
    strcpy(path,m_sitedefroot);
    strcat(path,subdir);
    fsutils.CheckSlash(path);           //make sure the slashes are correct
    fsutils.CheckSlashEnd(path,len+1);  //add ending '/' if necessary
    if (filename != NULL)
        strcat(path,filename);

    return(path);
}

    //builds a path in the default home directory for the site
    //Example home path: /usr/ftpserver/site/
char *CSiteInfo::BuildPathHome(const char *subdir, char *filename /*=NULL*/)
{
    CFSUtils fsutils;
    char *path;
    int len;

    if (m_sitedefroot == NULL || subdir == NULL)
        return(NULL);

        //+1 in case need ending '/'
    len = strlen(m_sitedefroot) + strlen(subdir) + ((filename != NULL) ? strlen(filename) : 0) + 1;

    if ((path = (char *)malloc(len+1)) == NULL)
        return(NULL);

        //use the default root dir as the default home dir
    strcpy(path,m_sitedefroot);
    strcat(path,subdir);
    fsutils.CheckSlash(path);           //make sure the slashes are correct
    fsutils.CheckSlashEnd(path,len+1);  //add ending '/' if necessary
    if (filename != NULL)
        strcat(path,filename);

    return(path);
}

void CSiteInfo::FreePath(char *path)
{

    if (path != NULL)
        free(path);
}

////////////////////////////////////////
// Functions used for logging
////////////////////////////////////////

    //Format string for the date fields in the log file.
    //Uses the same format specifiers as strftime()
void CSiteInfo::SetDateFormatStr(char *formatstr)
{

    if (formatstr == NULL)
        return;

    m_thr.P(&m_mutexaccesslog); //enter the critical section for the access log file
    m_thr.P(&m_mutexproglog);   //enter the critical section for the program log file

    strcpy(m_dateformat,formatstr);

    m_thr.V(&m_mutexproglog);   //exit the critical section
    m_thr.V(&m_mutexaccesslog); //exit the critical section
}

    //Set the logging level for the program log file
void CSiteInfo::SetProgLoggingLevel(int level)
{
    m_progloglevel = level;
}

    //Set the format string for the program log file
void CSiteInfo::SetProgLogFormatStr(char *formatstr)
{

    if (formatstr == NULL)
        return;

    m_thr.P(&m_mutexproglog);   //enter the critical section for the program log file

    strcpy(m_progformat,formatstr);

    m_thr.V(&m_mutexproglog);   //exit the critical section
}

    //Set the file name and base path for the program log file.
    //Returns 1 if the new file name is successfully set and
    //0 if the "logfile" or "basepath" are invalid.
int CSiteInfo::SetProgLogName(char *logfile, char *basepath /*=NULL*/)
{
    CFSUtils fsutils;
    char *tmppath, *pathptr, empty[]="";

    if (logfile == NULL)
        return(0);

    if (basepath != NULL) {
        if ((pathptr = (char *)malloc(strlen(basepath)+2)) == NULL)
            return(0);
        strcpy(pathptr,basepath);
        fsutils.CheckSlash(pathptr);    //make sure the slashes are correct for the OS
        fsutils.CheckSlashEnd(pathptr,strlen(basepath)+2);  //add a trailing slash if necessary
            //make sure the path is a directory
        if (fsutils.ValidatePath(pathptr) != 1) {
            free(pathptr);
            return(0);
        }
    } else {
        pathptr = empty;
    }

    if ((tmppath = fsutils.BuildPath(pathptr,"",logfile)) == NULL) {
        if (pathptr != empty) free(pathptr);
        return(0);
    }
    if (pathptr != empty) free(pathptr);

    m_thr.P(&m_mutexproglog);   //enter the critical section for the program log file

    if (m_proglogname != NULL)
        free(m_proglogname);
    m_proglogname = tmppath;

    m_thr.V(&m_mutexproglog);   //exit the critical section

    return(1);
}

    //Write a message to the program log file
int CSiteInfo::WriteToProgLog(const char *subsystem, const char *msgformat, ...)
{
    va_list parg;
    char *buffer1, *buffer2, datebuffer[64];
    int flagfinish = 0, buffersize = 100, nchars;

        //return if logging is turned off
    if (m_progloglevel == SITEINFO_LOGLEVELNONE)
        return(1);

    if (msgformat == NULL)
        return(0);

        //get the argument list
    va_start(parg,msgformat);

        //Write the arguments to the string, but 
        //ensure that we don't overflow the buffer.
    while (flagfinish == 0) {

            //allocate buffer to write this string
        if ((buffer1 = (char *)calloc(buffersize+1,1)) == NULL) {
            va_end(parg);
            return(0);
        }

            //write the arguments into the buffer
        #ifdef WIN32
          nchars = _vsnprintf((char*)buffer1,buffersize,msgformat,parg); //for Windows
        #else
          nchars = vsnprintf((char*)buffer1,buffersize,msgformat,parg);  //for UNIX
        #endif

        if (nchars < 0) {
                //The string was too long!
                //Double the size of the buffer and try again
            free(buffer1);
            buffersize *= 2;
        } else {
            flagfinish = 1; //the buffer was successfully written to
        }
    }

    va_end(parg);

    m_thr.P(&m_mutexproglog);   //enter the critical section for the program log file

    if (m_proglogname == NULL) {
        free(buffer1);
        m_thr.V(&m_mutexproglog);   //exit the critical section
        return(0);
    }

        //Builds a log line for the program log file based on m_progformat
    GetTimeString(datebuffer,sizeof(datebuffer),m_dateformat);
    if ((buffer2 = BuildProgLogLine(datebuffer,const_cast<char*>(subsystem),buffer1)) == NULL) {
        free(buffer1);
        m_thr.V(&m_mutexproglog);   //exit the critical section
        return(0);
    }
    free(buffer1);

        //update the program log file name
    if ((buffer1 = (char *)malloc(strlen(m_proglogname)+8)) == NULL) {
        free(buffer2);
        m_thr.V(&m_mutexproglog);   //exit the critical section
        return(0);
    }
    GetTimeString(buffer1,strlen(m_proglogname)+8,m_proglogname);
    m_proglog.SetLogFilename(buffer1);
    free(buffer1);

        //Write to the program log file
    if (m_progloglevel == SITEINFO_LOGLEVELDISPLAY)
        m_proglog.WriteToLog(buffer2,LOG_LEVELDISPLAY);
    else if (m_progloglevel == SITEINFO_LOGLEVELNORMAL)
        m_proglog.WriteToLog(buffer2,LOG_LEVELNORMAL);
    else if (m_progloglevel == SITEINFO_LOGLEVELDEBUG)
        m_proglog.WriteToLog(buffer2,LOG_LEVELDEBUG);

    m_thr.V(&m_mutexproglog);   //exit the critical section

    free(buffer2);

    return(1);
}

    //Set the logging level for the access log file
void CSiteInfo::SetAccessLoggingLevel(int level)
{
    m_accessloglevel = level;
}

    //Set the format string for the access log file
void CSiteInfo::SetAccessLogFormatStr(char *formatstr)
{

    if (formatstr == NULL)
        return;

    m_thr.P(&m_mutexaccesslog); //enter the critical section for the access log file

    strcpy(m_accessformat,formatstr);

    m_thr.V(&m_mutexaccesslog); //exit the critical section
}

    //Set the file name and base path for the access log file.
    //Returns 1 if the new file name is successfully set and
    //0 if the "logfile" or "basepath" are invalid.
int CSiteInfo::SetAccessLogName(char *logfile, char *basepath /*=NULL*/)
{
    CFSUtils fsutils;
    char *tmppath, *pathptr, empty[]="";

    if (logfile == NULL)
        return(0);

    if (basepath != NULL) {
        if ((pathptr = (char *)malloc(strlen(basepath)+2)) == NULL)
            return(0);
        strcpy(pathptr,basepath);
        fsutils.CheckSlash(pathptr);    //make sure the slashes are correct for the OS
        fsutils.CheckSlashEnd(pathptr,strlen(basepath)+2);  //add a trailing slash if necessary
            //make sure the path is a directory
        if (fsutils.ValidatePath(pathptr) != 1) {
            free(pathptr);
            return(0);
        }
    } else {
        pathptr = empty;
    }

    if ((tmppath = fsutils.BuildPath(pathptr,"",logfile)) == NULL) {
        if (pathptr != empty) free(pathptr);
        return(0);
    }
    if (pathptr != empty) free(pathptr);

    m_thr.P(&m_mutexaccesslog); //enter the critical section for the access log file

    if (m_accesslogname != NULL)
        free(m_accesslogname);
    m_accesslogname = tmppath;

    m_thr.V(&m_mutexaccesslog); //exit the critical section

    return(1);
}
    
    //Write to the access log
    //NOTE: all fields that are NULL or empty are printed as "-" in the log file.
int CSiteInfo::WriteToAccessLog(char *sip, char *sp, char *cip, char *cp, char *user, const char *cmd, const char *arg, const char *status, const char *text)
{
    char *buffer, *buffer1, datebuffer[64];

    m_thr.P(&m_mutexaccesslog); //enter the critical section for the access log file

    if (m_accesslogname == NULL) {
        m_thr.V(&m_mutexaccesslog); //exit the critical section
        return(0);
    }

        //Builds a log line for the access log file based on m_accessformat
    GetTimeString(datebuffer,sizeof(datebuffer),m_dateformat);
    if ((buffer = BuildAccessLogLine(datebuffer,sip,sp,cip,cp,user,const_cast<char*>(cmd),const_cast<char*>(arg),const_cast<char*>(status),const_cast<char*>(text))) == NULL) {
        m_thr.V(&m_mutexaccesslog); //exit the critical section
        return(0);
    }

        //update the access log file name
    if ((buffer1 = (char *)malloc(strlen(m_accesslogname)+8)) == NULL) {
        free(buffer);
        m_thr.V(&m_mutexaccesslog); //exit the critical section
        return(0);
    }
    GetTimeString(buffer1,strlen(m_accesslogname)+8,m_accesslogname);
    m_accesslog.SetLogFilename(buffer1);
    free(buffer1);

        //Write to the access log file
    if (m_accessloglevel == SITEINFO_LOGLEVELDISPLAY)
        m_accesslog.WriteToLog(buffer,LOG_LEVELDISPLAY);
    else if (m_accessloglevel == SITEINFO_LOGLEVELNORMAL)
        m_accesslog.WriteToLog(buffer,LOG_LEVELNORMAL);
    else if (m_accessloglevel == SITEINFO_LOGLEVELDEBUG)
        m_accesslog.WriteToLog(buffer,LOG_LEVELDEBUG);

    m_thr.V(&m_mutexaccesslog); //exit the critical section

    free(buffer);

    return(1);
}

////////////////////////////////////////
// Functions used to specify/limit the
// range of ports used in data
// connections (PASV)
////////////////////////////////////////

    //Returns -1 to indicate the function is unimplemented
int CSiteInfo::SetDataPortRange(unsigned short startport, unsigned short endport)
{

    return(-1);  //by default this feature is not implemented
}

    //Returns -1 to indicate the function is unimplemented
int CSiteInfo::GetDataPort(char *bindport, int maxportlen, int *sdptr, char *bindip)
{

    return(-1);  //by default this feature is not implemented
}

    //Returns -1 to indicate the function is unimplemented
int CSiteInfo::FreetDataPort(int sd)
{

    return(-1);  //by default this feature is not implemented
}

////////////////////////////////////////
// Functions used to build LIST lines
// returned to the client.
////////////////////////////////////////

    //This function is called in CFtpsXfer to build a full line for the LIST command.
    //Loads "listline" with a directory listing line that should be returned.
    //"filepath" is the full path of the file for this LIST line.
    //"thisyear" is the current year as a 4 digit string (if NULL, the current year is assumed for the file)
char *CSiteInfo::BuildFullListLine(char *listline, int maxlinesize, char *filepath, char *thisyear /*=NULL*/)
{
    CFSUtils fsutils;
    CStrUtils strutils;
    fsutilsFileInfo_t finfo;
    char *filename, *ptr, timestr[32], permissions[10], user[9], group[9], filetype;
    int len = 0;

    if (listline == NULL || maxlinesize <= 0 || filepath == NULL)
        return(listline);

    *listline = '\0';

    if (fsutils.GetFileStats(filepath,&finfo) == 0)
        return(listline);

    if ((filename = (char *)calloc(strlen(filepath)+1,1)) == NULL)
        return(listline);

    fsutils.GetFileName(filename,strlen(filepath)+1,filepath);

    filetype = (finfo.mode & S_IFDIR) ? 'd' : '-';

    GetTimeString(timestr,sizeof(timestr),"%Y %b %d %H:%M",finfo.timecreate);
    if ((ptr = strrchr(timestr,' ')) != NULL) {
        ptr++;
        if (thisyear != NULL) {
            if (strncmp(timestr,thisyear,4) != 0) {
                *ptr = ' ';
                memmove(ptr+1,timestr,4);     //overwrite the time of day with the year
            }
        }
        ptr = timestr + 5;  //move to the start of the month name
        fsutils.GetPrmString(finfo.mode,permissions,sizeof(permissions));
        fsutils.GetUsrName(finfo.userid,user,sizeof(user));
        fsutils.GetGrpName(finfo.groupid,group,sizeof(group));
        strutils.SNPrintf(listline,maxlinesize,"%c%9s %3ld %-8s %-8s %10lu %s ",filetype,permissions,finfo.nlinks,user,group,finfo.size,ptr);
        len = strlen(listline);
        strncat(listline,filename,maxlinesize-len-1);
    }

    free(filename);

    return(listline);
}

////////////////////////////////////////
// Functions used to handle adding new
// files to the server.
////////////////////////////////////////

    //This function is called in CFtpsXfer to add files to the server
    //(called during upload).
    //If size = -1, the file size will be set by getting the file stats from the OS.
int CSiteInfo::AddFile(char *filepath, char *username, int size /*=-1*/)
{

    return(1);  //by default do nothing
}

////////////////////////////////////////
// Functions used to control/monitor
// transfer rates.
////////////////////////////////////////

    //This function is called in CFtpsXfer to periodically update
    //the user's current transfer rate.  The rate at which this 
    //function is called is set in FtpsXfer.h.
void CSiteInfo::SetCurrentXferRate(int userid, long bytespersec)
{

    //override to implement
}

    //Returns the maximum allowable download speed for a user.
    //This function is called in CFtpsXfer to regulate a
    //user's download speed.
int CSiteInfo::GetMaxDLSpeed(char *username)
{

    return(0);  //0 -> no speed limit
}

    //Returns the maximum allowable upload speed for a user.
    //This function is called in CFtpsXfer to regulate a
    //user's upload speed.
int CSiteInfo::GetMaxULSpeed(char *username)
{

    return(0);  //0 -> no speed limit
}


////////////////////////////////////////
// Functions used to update the file
// transfer information/statistics
////////////////////////////////////////

    //update download statistics
int CSiteInfo::UpdateDLStats(int userid, char *username, int bytes, int bytespersec)
{

    return(-1);  //by default this feature is not implemented
}

    //update upload statistics
int CSiteInfo::UpdateULStats(int userid, char *username, int bytes, int bytespersec)
{

    return(-1);  //by default this feature is not implemented
}

////////////////////////////////////////
// Functions used to update the user's
// credits
////////////////////////////////////////

    //Add credits to a user's account.
    //flagupload != 0 -> Add after upload, flagupload == 0 -> Add after download
int CSiteInfo::AddCredits(char *filepath, int userid, char *username, int bytes, int flagupload)
{

    return(-1);  //by default this feature is not implemented
}

    //Remove credits from a user's account
    //flagupload != 0 -> Remove after upload, flagupload == 0 -> Remove after download
int CSiteInfo::RemoveCredits(char *filepath, int userid, char *username, int bytes, int flagupload)
{

    return(-1);  //by default this feature is not implemented
}

////////////////////////////////////////
// Functions used to check if FXP is
// allowed
////////////////////////////////////////

int CSiteInfo::IsAllowedFXP(char *username, int flagupload, int flagpasv)
{

    return(1);  //allow FXP by default
}

////////////////////////////////////////
// SSL key/certificate functions
////////////////////////////////////////

    //returns a buffer containing the private key
char *CSiteInfo::GetPrivKey(int *privkeysize)
{

    return(NULL);   //by default this feature is not implemented
}

    //free() the buffer returned by GetPrivKey()
void CSiteInfo::FreePrivKey(char *privkeybuf)
{

    if (privkeybuf != NULL)
        free(privkeybuf);
}

    //returns a buffer containing the certificate
char *CSiteInfo::GetCert(int *certsize)
{

    return(NULL);   //by default this feature is not implemented
}

    //free() the buffer returned by GetCert()
void CSiteInfo::FreeCert(char *certbuf)
{

    if (certbuf != NULL)
        free(certbuf);
}

    //gets the password to use with the private key
int CSiteInfo::GetPrivKeyPW(char *pwbuff, int maxpwbuff)
{

    if (pwbuff == NULL || maxpwbuff == 0)
        return(0);

    *pwbuff = '\0';

    return(0);  //by default this feature is not implemented
}

////////////////////////////////////////
// Time Functions
////////////////////////////////////////

    //Loads "buffer" with the string version of the time (returns a pointer to buffer).
    //"formatstring" is the string that is passed to strftime().
    //If ctime = 0, the current time is used.  If flaggmt != 0 GMT time will be used (else localtime).
char *CSiteInfo::GetTimeString(char *buffer, int maxsize, const char *formatstring, long ctime /*=0*/, int flaggmt /*=0*/)
{
    struct tm *ltimeptr, ltime;
    struct timeb timebuffer;
    long tmptime, len;
    const char *ptr; char *tmpformatstring = NULL, utcoffset[16];

    if (buffer == NULL || maxsize == 0)
        return(buffer);

    *buffer = '\0'; //initialize to an empty string

    if (formatstring == NULL)
        return(buffer);

    ftime(&timebuffer);     //get the current time

    for (ptr = formatstring; *ptr != '\0'; ptr++) {
        if (*ptr == '%') {
            ptr++;  //move to the next character
            if (*ptr == '\0')
                break;
            if (*ptr == 'O') {
                len = strlen(formatstring) + 16;    //add 16 for UTC offset string
                    //allocate the temp format string buffer
                if ((tmpformatstring = (char *)malloc(len)) == NULL)
                    return(buffer);
                    //copy over the first part of the format string
                strncpy(tmpformatstring,formatstring,ptr-formatstring-1);
                tmpformatstring[ptr-formatstring-1] = '\0';
                    //build the UTC offset string
                if (timebuffer.timezone > 0) {
                    sprintf(utcoffset,"-%04d",timebuffer.timezone); //add "-" western time zones
                } else {
                    sprintf(utcoffset,"+%04d",(-1)*(timebuffer.timezone));
                }
                    //append the UTC offset string
                strcat(tmpformatstring,utcoffset);
                    //append the rest of the format string
                strcat(tmpformatstring,ptr+1);
                break;
            }
        }
    }

        //if a time was not specified, use the current time
    tmptime = (ctime != 0) ? ctime : timebuffer.time;

        //If "tmptime" is an invalid time, try to use 0 instead
    if ((ltimeptr = GetTimeR(tmptime,&ltime,flaggmt)) == NULL) {
        tmptime = 0;
        ltimeptr = GetTimeR(tmptime,&ltime,flaggmt);
    }

    if (ltimeptr != NULL) {
        if (tmpformatstring != NULL)
            strftime(buffer,maxsize-1,tmpformatstring,&ltime);
        else
            strftime(buffer,maxsize-1,formatstring,&ltime);
        buffer[maxsize-1] = '\0';
    }

    if (tmpformatstring != NULL)
        free(tmpformatstring);

    return(buffer);
}

    //A thread safe version of the C standard function localtime()/gmtime()
struct tm *CSiteInfo::GetTimeR(long ctime, struct tm *tmoutput, int flaggmt)
{
    struct tm *ltimeptr;

    if (tmoutput == NULL)
        return(NULL);

    memset(tmoutput,0,sizeof(struct tm));

    #ifdef WIN32
      FILETIME filetime;
      LONGLONG ll;
      SYSTEMTIME systemtime;
          //converts from the standard C time (sec since Jan 1970) to FILETIME.
      ll = Int32x32To64(ctime,10000000) + 116444736000000000;
      filetime.dwLowDateTime = (DWORD)(ll);
      filetime.dwHighDateTime = (DWORD)(ll >> 32);
          //load the tmoutput struct
      if (flaggmt == 0)
          FileTimeToLocalFileTime(&filetime,&filetime);
      if (FileTimeToSystemTime(&filetime,&systemtime) != 0) {
          tmoutput->tm_hour = systemtime.wHour;
          tmoutput->tm_mday = systemtime.wDay;
          tmoutput->tm_min = systemtime.wMinute;
          tmoutput->tm_mon = systemtime.wMonth - 1;
          tmoutput->tm_sec = systemtime.wSecond;
          tmoutput->tm_wday = systemtime.wDayOfWeek;
          tmoutput->tm_year = systemtime.wYear - 1900;
          ltimeptr = tmoutput;
      } else {
          ltimeptr = NULL;
      }
    #else
      if (flaggmt == 0)
        ltimeptr = localtime_r(&ctime,tmoutput);
      else
        ltimeptr = gmtime_r(&ctime,tmoutput);
    #endif

    return(ltimeptr);
}

    //returns the offset between the localtime and GMT time (in minutes)
    //Ex. EST -> -300 (5 hrs behind)
int CSiteInfo::GetGMTOffsetMinutes()
{
    struct timeb timebuffer;

    ftime(&timebuffer);     //get the current time

    return(-1 * timebuffer.timezone);
}

    //gets the GMT hour relative to the local hour
int CSiteInfo::GetGMTHour(int localhour, int gmtoffsetmin)
{
    int gmthour, gmtoffsethrs;

    gmtoffsethrs = gmtoffsetmin / 60;

    gmthour = localhour - gmtoffsethrs;

    if (gmthour >= 24)
        gmthour -= 24;
    else if (gmthour < 0)
        gmthour = 24 + gmthour;

    return(gmthour);
}

    //gets the GMT minutes within the hour relative to the local
    //minutes within the hour
int CSiteInfo::GetGMTMinutes(int localmin, int gmtoffsetmin)
{
    int gmtmin, minoffset;

    minoffset = gmtoffsetmin % 60;

    gmtmin = localmin - minoffset;

    if (gmtmin < 0)
        gmtmin = 60 - gmtmin;
    else if (gmtmin >= 60)
        gmtmin -= 60;

    return(gmtmin);
}

////////////////////////////////////////
// Functions used to get Site state
////////////////////////////////////////

char *CSiteInfo::GetProgName(char *buffer, int maxsize)
{

    if (buffer == NULL)
        return(NULL);

    strncpy(buffer,m_progname,maxsize-1);
    buffer[maxsize-1] = '\0';

    return(buffer);
}

char *CSiteInfo::GetProgVersion(char *buffer, int maxsize)
{

    if (buffer == NULL)
        return(NULL);

    strncpy(buffer,m_progversion,maxsize-1);
    buffer[maxsize-1] = '\0';

    return(buffer);
}

char *CSiteInfo::GetCoreVersion(char *buffer, int maxsize)
{

    if (buffer == NULL)
        return(NULL);

    strncpy(buffer,m_coreversion,maxsize-1);
    buffer[maxsize-1] = '\0';

    return(buffer);
}

//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //Builds a log line for the program log file based on m_progformat
    //NOTE: the returned buffer must be free()
char *CSiteInfo::BuildProgLogLine(char *date, char *subsystem, char *message)
{
    CStrUtils strutils;
    char *buffer, *values[3], empty[] = "";
    char specifiers[] = "DSM";    //see SiteInfo.h for description

        //set the date value
    values[0] = (date == NULL) ? empty : date;
        //set the subsystem value
    values[1] = (subsystem == NULL) ? empty : subsystem;
        //set the message value
    values[2] = (message == NULL) ? empty : message;

    buffer = strutils.BuildFormattedString(m_progformat,specifiers,values,3);

    return(buffer);
}

    //Builds a log line for the access log file based on m_accessformat
    //NOTE: the returned buffer must be free()
char *CSiteInfo::BuildAccessLogLine(char *date, char *sip, char *sp, char *cip, char *cp, char *user, char *cmd, char *arg, char *status, char *text)
{
    CStrUtils strutils;
    char *buffer, *values[10], empty[] = "";
    char specifiers[] = "dslcpumart";   //see SiteInfo.h for description

        //set the date value
    values[0] = (date == NULL) ? empty : date;
        //set the server IP value
    values[1] = (sip == NULL) ? empty : sip;
        //set the server listening port value
    values[2] = (sp == NULL) ? empty : sp;
        //set the client IP value
    values[3] = (cip == NULL) ? empty : cip;
        //set the client port value
    values[4] = (cp == NULL) ? empty : cp;
        //set the user name value
    values[5] = (user == NULL) ? empty : user;
        //set the FTP command value
    values[6] = (cmd == NULL) ? empty : cmd;
        //set the argument to the command value
    values[7] = (arg == NULL) ? empty : arg;
        //set the 3-digit response code value
    values[8] = (status == NULL) ? empty : status;
        //set the response text value
    values[9] = (text == NULL) ? empty : text;

    buffer = strutils.BuildFormattedString(m_accessformat,specifiers,values,10);

    return(buffer);
}
