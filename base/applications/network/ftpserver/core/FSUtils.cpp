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
// FSUtils.cpp: implementation of the CFSUtils class.
//
// Utility functions used for File System Access
// (Ex. Directory listing)
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>

#ifdef WIN32
  #include <io.h>
  #include <direct.h>
  #include <sys/utime.h>
#else
  #include <glob.h>
  #include <unistd.h>
  #include <grp.h>  //for getgrgid
  #include <pwd.h>  //for getpwuid
  #include <utime.h>
#endif

    //Needed for GetDiskInfo()
#ifdef WIN32
  #include <Windows.h>
#endif
#ifdef SOLARIS
  #include <sys/statvfs.h>
#endif
#ifdef BSD
  #include <sys/param.h>
  #include <sys/ucred.h>
  #include <sys/mount.h>
  #define fsbtoblk(num, fsbs, bs) (((fsbs) != 0 && (fsbs) < (bs)) ? (num) / ((bs) / (fsbs)) : (num) * ((fsbs) / (bs)))
#endif
#ifdef LINUX
  #include <sys/vfs.h>
#endif

#include "StrUtils.h"
#include "FSUtils.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFSUtils::CFSUtils()
{

}

CFSUtils::~CFSUtils()
{

}


//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Functions used for
// directory/fileinfo
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Get the first filename in a directory listing.
//
// [in] dirpath     : Path of the directory to list.
// [out] filename   : First filename in the directory listing.
// [in] maxfilename : Max size of the "filename" string.
//
// Return : On success a handle to the directory listing is returned.
//          On failure 0 is returned.
//
long CFSUtils::DirGetFirstFile(const char *dirpath, char *filename, int maxfilename)
{
    long handle = 0;
    char *dirpathbuff;

    if (dirpath == NULL)
        return(0);

    if ((dirpathbuff = (char *)malloc(strlen(dirpath)+3)) == NULL)
        return(0);
    strcpy(dirpathbuff,dirpath);    //make a local copy of dirpath

    if (strchr(dirpathbuff,'*') == NULL) {  //check if wildcard
        CheckSlashEnd(dirpathbuff,strlen(dirpath)+3); //make sure the path ends in a slash (if no wildcard)
        strcat(dirpathbuff,"*");    //add the wildcard (if not already done)
    }

    CheckSlash(dirpathbuff);        //make sure the path has correct slashes

    #ifdef WIN32
      long hdir;
      struct _finddata_t fileinfo;

      if ((hdir = _findfirst(dirpathbuff,&fileinfo)) == -1L) {
          free(dirpathbuff);
          return(0);                //error opening the dir
      }

      handle = hdir;
      if (filename != NULL) {
          strncpy(filename,fileinfo.name,maxfilename-1);
          *(filename+maxfilename-1) = '\0';
      }
    #else
      glob_t *pglob;

      if ((pglob = new glob_t) == NULL) {
          free(dirpathbuff);
          return(0);
      }
      if (glob(dirpathbuff,0,NULL,pglob) != 0) {
          delete pglob;
          free(dirpathbuff);
          return(0);
      }
        //No paths were matched
      if (pglob->gl_pathc == 0) {
          delete pglob;
          free(dirpathbuff);
          return(0);
      }

      handle = (long)pglob;
      if (filename != NULL) {
               //only copy the filename (not the path)
          GetFileName(pglob->gl_pathv[0],strlen(pglob->gl_pathv[0])+1,pglob->gl_pathv[0]);
          strncpy(filename,pglob->gl_pathv[0],maxfilename-1);
          *(filename+maxfilename-1) = '\0';
      }
      pglob->gl_offs = 1;   //set the offset
    #endif

    free(dirpathbuff);
    return(handle);
}

//////////////////////////////////////////////////////////////////////
// Get the next filename in a directory listing.
//
// [in] handle      : Handle to the directory listing.
// [out] filename   : Next filename in the directory listing.
// [in] maxfilename : Max size of the "filename" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::DirGetNextFile(long handle, char *filename, int maxfilename)
{
    #ifdef WIN32
      struct _finddata_t fileinfo;

      if (_findnext(handle,&fileinfo) == -1L)
          return(0);
      if (filename != NULL) {
          strncpy(filename,fileinfo.name,maxfilename-1);
          *(filename+maxfilename-1) = '\0';
      }
    #else
      glob_t *pglob = (glob_t *)handle;

      if (pglob->gl_pathv[pglob->gl_offs] == NULL)
          return(0);
      if (filename != NULL) {
               //only copy the filename (not the path)
          GetFileName(pglob->gl_pathv[pglob->gl_offs],strlen(pglob->gl_pathv[pglob->gl_offs])+1,pglob->gl_pathv[pglob->gl_offs]);
          strncpy(filename,pglob->gl_pathv[pglob->gl_offs],maxfilename-1);
          *(filename+maxfilename-1) = '\0';
      }
      (pglob->gl_offs)++;   //increment the offset
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Close the handle to the directory listing.
//
// [in] handle : Handle to the directory listing.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::DirClose(long handle)
{
    #ifdef WIN32
      _findclose(handle);
    #else
      ((glob_t *)handle)->gl_offs = 0;  //reset the offset
      globfree((glob_t *)handle);
      delete (glob_t *)handle;
    #endif

    return 1;
}

//////////////////////////////////////////////////////////////////////
// Checks if a directory is empty.
//
// [in] dirpath : Path of the directory to check.
//
// Return : Returns 1 if the directory is not empty and 0 if the
//          directory is empty or does not exist.
//
int CFSUtils::DirIsEmpty(char *dirpath)
{
    long handle;
    char buffer[3];

    if ((handle = DirGetFirstFile(dirpath,buffer,sizeof(buffer))) <= 0)
        return(0);
    do {
        if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
            DirClose(handle);
            return(1);
        }
    } while (DirGetNextFile(handle,buffer,sizeof(buffer)) > 0);
    DirClose(handle);

    return(0);
}

//////////////////////////////////////////////////////////////////////
// Gets information about a file.
//
// [in] filepath : Path to the file.
// [out] infoptr : Structure containing the file information.
//
// Return : On success 1 is returned.  0 is returned if there was as
//          error accessing the file or the file does not exist.
//
int CFSUtils::GetFileStats(char *filepath, fsutilsFileInfo_t *infoptr)
{
    int len, flagslashend = 0;

    if (filepath == NULL || infoptr == NULL)
        return(0);

        //remove any trailing slashes if necessary
    len = strlen(filepath);
    if (len > 0) {
        if (*(filepath+len-1) == '\\' || *(filepath+len-1) == '/') {
                //only remove the trailing slash if it is not the only slash
            if (strchr(filepath,'\\') != (filepath+len-1) && strchr(filepath,'/') != (filepath+len-1)) {
                *(filepath+len-1) = '\0';
                flagslashend = 1;
            }
        }
    } else {
        return(0);  //the filepath is empty
    }

    #ifdef WIN32
      struct _stat filestatus;
      if (_stat(filepath,&filestatus) != 0) {
          if (flagslashend != 0) CheckSlashEnd(filepath,len+1); //restore the trailing slash
          return(0);
      }
    #else
      struct stat filestatus;
      if (stat(filepath,&filestatus) != 0) {
          if (flagslashend != 0) CheckSlashEnd(filepath,len+1); //restore the trailing slash
          return(0);
      }
    #endif
    memset(infoptr,0,sizeof(fsutilsFileInfo_t));
    infoptr->timeaccess = filestatus.st_atime;
    infoptr->timecreate = filestatus.st_ctime;
    infoptr->timemod = filestatus.st_mtime;
    infoptr->size = filestatus.st_size;
    infoptr->groupid = filestatus.st_gid;
    infoptr->userid = filestatus.st_uid;
    infoptr->mode = filestatus.st_mode;
    infoptr->devnum = filestatus.st_dev;
    infoptr->inodnum = filestatus.st_ino;   //only useful in UNIX
    infoptr->nlinks = filestatus.st_nlink;  //only useful in UNIX (always 1 in WINDOWS)

    if (flagslashend != 0) CheckSlashEnd(filepath,len+1);   //restore the trailing slash
    return(1);
}

//////////////////////////////////////////////////////////////////////
// Sets the file modification and access time.
//
// [in] filepath   : Path to the file.
// [in] timemode   : Time to set the file modification time to.
// [in] timeaccess : Time to set the file access time to.
//                   By default the access time will be set to the
//                   same values as the modification time.
//
// Return : On success 1 is returned.  0 is returned if there was as
//          error accessing the file or the file does not exist.
//
int CFSUtils::SetFileTime(char *filepath, long timemod, long timeaccess /*=0*/)
{
    long accesstime;

        //by default set the access time to the modification time
    accesstime = (timeaccess == 0) ? timemod : timeaccess;

    #ifdef WIN32
        struct _utimbuf utimebuffer;
        utimebuffer.modtime = timemod;
        utimebuffer.actime = accesstime;
        if (_utime(filepath,&utimebuffer) != 0)
            return(0);
    #else
        struct utimbuf utimebuffer;
        utimebuffer.modtime = timemod;
        utimebuffer.actime = accesstime;
        if (utime(filepath,&utimebuffer) != 0)
            return(0);
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Gets the user name based on the user ID (for UNIX).
//
// [in] uid         : The user ID.
// [out] username   : Name of the user corresponding to uid.
// [in] maxusername : Max size of the "username" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::GetUsrName(long uid, char *username, int maxusername)
{

    if (username == NULL || maxusername == 0)
        return(0);

    #ifndef WIN32
      struct passwd *pw;
          
      if ((pw = getpwuid(uid)) != NULL) {
          strncpy(username,pw->pw_name,maxusername-1);
          username[maxusername-1] = '\0';
          return(1);
      }
    #endif

        //use generic user by default
    strncpy(username,"owner",maxusername-1);
    username[maxusername-1] = '\0';

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Gets the group name based on the group ID (for UNIX).
//
// [in] gid        : The group ID.
// [out] grpname   : Name of the group corresponding to gid.
// [in] maxgrpname : Max size of the "grpname" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::GetGrpName(long gid, char *grpname, int maxgrpname)
{

    if (grpname == NULL || maxgrpname == 0)
        return(0);

    #ifndef WIN32
      struct group *grp;

      if ((grp = getgrgid(gid)) != NULL) {
          strncpy(grpname,grp->gr_name,maxgrpname-1);
          grpname[maxgrpname-1] = '\0';
          return(1);
      }
    #endif

        //use generic group by default
    strncpy(grpname,"group",maxgrpname-1);
    grpname[maxgrpname-1] = '\0';

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Get the permission string based on the mode for the file/directory.
//
// [in] mode      : The mode of the file/directory.
// [out] prmstr   : Permission string for the file or directory.
// [in] maxprmstr : Max size of the "prmstr" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// Example: (mode & 0777) = 0755 -> "rwxr-xr-x"
//
int CFSUtils::GetPrmString(long mode, char *prmstr, int maxprmstr)
{
    char *ptr = prmstr;

    if (prmstr == NULL || maxprmstr == 0)
        return(0);

    *prmstr = '\0';
    if (maxprmstr < 10)
        return(0);      //permissions string must be at least 10 chars

        //Build the permission string one char at a time
    *(ptr++) = (mode & 0400) ? 'r' : '-';
    *(ptr++) = (mode & 0200) ? 'w' : '-';
    *(ptr++) = (mode & 0100) ? 'x' : '-';
    *(ptr++) = (mode & 0040) ? 'r' : '-';
    *(ptr++) = (mode & 0020) ? 'w' : '-';
    *(ptr++) = (mode & 0010) ? 'x' : '-';
    *(ptr++) = (mode & 0004) ? 'r' : '-';
    *(ptr++) = (mode & 0002) ? 'w' : '-';
    *(ptr++) = (mode & 0001) ? 'x' : '-';
    *ptr = '\0';    //terminate the string

    return(1);
}

////////////////////////////////////////
// Functions used for building paths
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Creates the path based on the inputs "rootdir," "cwd," and
// "arg."  The entire path is returned.  The buffer returned has
// one extra byte in case a trailing slash (or other char) needs to
// be added.
//
// [in] rootpath : Base path of the path to be built.
// [in] cwd      : CWD relative to the base path.
// [in] arg      : New file/directory path (relative to CWD).
//
// Return : On success the full path to the new directory is returned.
//          On failure NULL is returned.
//
// NOTE: All directory paths should always end in a slash.  "cwd" should
//       never start with a slash.
//
// Example: rootpath = /ftpd/, cwd = site/files/upload/, 
//          arg = ../download/file1.dat
//          --> /ftpd/site/files/download/file1.dat
//
char *CFSUtils::BuildPath(const char *rootpath, const char *cwd, const char *arg)
{
    char *argbuff, *tmppath, *outpath;

    if (rootpath == NULL || cwd == NULL)
        return(NULL);

        //if no arguments are present, do nothing
    if (arg == NULL || *arg == '\0') {
        if ((outpath = (char *)malloc(strlen(rootpath)+strlen(cwd)+2)) != NULL)
            CopyPaths(outpath,strlen(rootpath)+strlen(cwd)+1,rootpath,cwd);
        return(outpath);
    }

    if ((argbuff = (char *)malloc(strlen(arg)+2)) == NULL)
        return(NULL);
    strcpy(argbuff,arg);    //make a copy of the arguments

        //if the path ends in a '.' add an ending slash
    if (*(argbuff+strlen(argbuff)-1) == '.')
        CheckSlashEnd(argbuff,strlen(arg)+2);

    if ((tmppath = (char *)malloc(strlen(cwd)+strlen(argbuff)+1)) == NULL) {
        free(argbuff);
        return(NULL);
    }

    *tmppath = '\0';
    CheckSlash(argbuff);
    switch (*argbuff) {
        case FSUTILS_SLASH: {
          if (*(argbuff+1) != '\0')  //if there was more than '\' or '/'
              strcat(tmppath,argbuff+1);
        } break;
        default: {
            strcpy(tmppath,cwd);
            strcat(tmppath,argbuff);
        } break;
    } // end switch

    CompactPath(tmppath); //make an absolute path

        //NOTE: add +2 in case a trailing slash is added
    if ((outpath = (char *)malloc(strlen(rootpath)+strlen(tmppath)+2)) != NULL)
        CopyPaths(outpath,strlen(rootpath)+strlen(tmppath)+1,rootpath,tmppath);

    free(argbuff);
    free(tmppath);
    return(outpath);
}

//////////////////////////////////////////////////////////////////////
// Free the path allocated in BuildPath().
//
// [in] path : Path to be free().
//
// Return : VOID
//
void CFSUtils::FreePath(char *path)
{

    if (path != NULL)
        free(path);
}

//////////////////////////////////////////////////////////////////////
// Checks if a path exists.
//
// [in] path : Path to be checked.
//
// Return : Returns 0 if the path DNE.  Returns 1 if path is a
//          directory and 2 if a file.
//
int CFSUtils::ValidatePath(char *path)
{
    fsutilsFileInfo_t fileinfo;
    int retval;

    if (GetFileStats(path,&fileinfo) != 0) {
        if (fileinfo.mode & S_IFDIR)
            retval = 1; //path is a directory
        else
            retval = 2; //path is a file
    } else {
        retval = 0;     //path DNE
    }

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// Makes sure the path starts with '/' or '\'.
//
// [in] path        : Path to be checked.
// [in] maxpathsize : Max size of the "path" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: The type of slash will depend on the local platform.
//       Also, the "path" buffer must be large enough to contain
//       an added slash character.
//
int CFSUtils::CheckSlashLead(char *path, int maxpathsize)
{
    char *buffer;

    if (path == NULL || maxpathsize == 0)
        return(0);

    if (*path == '\\' || *path == '/')
        return(1);  //path already starts with a slash

    if ((unsigned)maxpathsize < (strlen(path)+2))
        return(0);  //path is not big enough

    if ((buffer = (char *)malloc(strlen(path)+1)) == NULL)
        return(0);
    strcpy(buffer,path);
    
    sprintf(path,"%c",FSUTILS_SLASH);
    strcat(path,buffer);

    free(buffer);
    return(1);
}

//////////////////////////////////////////////////////////////////////
// Makes sure the path ends in '/' or '\'.
//
// [in] path        : Path to be checked.
// [in] maxpathsize : Max size of the "path" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: The type of slash will depend on the local platform.
//       Also, the "path" buffer must be large enough to contain
//       an added slash character.
//
int CFSUtils::CheckSlashEnd(char *path, int maxpathsize)
{
    int len;

    if (path == NULL || maxpathsize == 0)
        return(0);

    len = strlen(path);

    if (len > 0) {
        if (path[len-1] == '\\' || path[len-1] == '/')
            return(1);  //path already ends in a slash
    }

    if (maxpathsize < (len+2))
        return(0);  //path is not big enough

        //make sure the path ends in a '\' or '/'
    #ifdef WIN32
      strcat(path,"\\");    //for Windows
    #else
      strcat(path,"/");     //for UNIX
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Converts any '/' to '\' for WINDOWS or '\' to '/' for UNIX.
// "wrkdir" is used to avoid overwriting the string.
// Set "wrkdir" to NULL to use "outpath" only.
//
// [in/out] outpath : Path with the new slashes.  It is also an input
//                    when "wrkdir" is NULL.
// [in] wrkdir      : Path the slashes will be converted on.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: If "wrkdir" is used, "outpath" must be at least the
//       same size as "wrkdir."
//
int CFSUtils::CheckSlash(char *outpath, const char *wrkdir /*=NULL*/)
{
    char *ptr, *pathptr;

    if (outpath == NULL)
        return(0);

    if (wrkdir != NULL)
        strcpy(outpath,wrkdir);

        //convert all '/' to '\' for WINDOWS and '\' to '/' for UNIX
    pathptr = outpath;
    #ifdef WIN32
      while ((ptr = strchr(pathptr,'/')) != NULL) {
          *ptr = '\\';
          pathptr = ptr + 1;
      }
    #else
      while ((ptr = strchr(pathptr,'\\')) != NULL) {
          *ptr = '/';
          pathptr = ptr + 1;
      }
    #endif

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Similar to CheckSlash() except it always outputs '/' slashes.
//
// [in/out] outpath : Path with the new slashes.  It is also an input
//                    when "wrkdir" is NULL.
// [in] wrkdir      : Path the slashes will be converted on.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: If "wrkdir" is used, "outpath" must be at least the
//       same size as "wrkdir."
//
int CFSUtils::CheckSlashUNIX(char *outpath, const char *wrkdir /*=NULL*/)
{
    char *ptr;

    if (outpath == NULL)
        return(0);

    if (wrkdir != NULL)
        strcpy(outpath,wrkdir);

    while ((ptr = strchr(outpath,'\\')) != NULL) {
        *ptr = '/';
        ptr++;
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Adds a directory path to a base path.
//
// [in/out] path    : Base path the directory should be appended to.
// [in] maxpathsize : Max size of the "path" string.
// [in] dir         : Directory to append to "path".
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::CatPath(char *path, int maxpathsize, const char *dir)
{
    char *ptr;
    int len;

    if (path == NULL || dir == NULL)
        return(0);

    if ((ptr = strrchr(path,FSUTILS_SLASH)) == NULL) {
        return(0);
    } else {
        *(ptr+1) = '\0';
        len = strlen(path);
        strncat(path,dir,maxpathsize-1-len);
        path[maxpathsize-1] = '\0';
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Returns only the filename from the full path.
//
// [out] filename   : Filename portion of the full path.
// [in] maxfilename : Max size of the "filename" string.
// [in] fullpath    : Path to extract the filename from.
//
// Return : On success 1 is returned.  Returns 0 if the path is
//          invalid.
//
int CFSUtils::GetFileName(char *filename, int maxfilename, const char *fullpath)
{
    char *ptr, *pathptr;

    if (filename == NULL || maxfilename == 0 || fullpath == NULL)
        return(0);

        //check to see if the fullpath is valid
    if (strstr(fullpath,"\\\\") != NULL || strstr(fullpath,"//") != NULL)
        return(0);  //invalid path

    if (filename == fullpath) {
        if ((pathptr = (char *)malloc(strlen(fullpath)+1)) != NULL)
            strcpy(pathptr,fullpath);
        else
            return(0);
    } else {
        pathptr = (char *)fullpath;
    }

    if ((ptr = strrchr(pathptr,FSUTILS_SLASH)) == NULL) {
        strncpy(filename,pathptr,maxfilename-1);
        filename[maxfilename-1] = '\0';
    } else {
        strncpy(filename,(ptr+1),maxfilename-1);
        filename[maxfilename-1] = '\0';
    }

    if (filename == fullpath)
        free(pathptr);

        //Do not allow any file names with '\' or '/' in it.
    #ifdef WIN32
      if (strchr(filename,'/') != NULL || *filename == '\0')
    #else
      if (strchr(filename,'\\') != NULL || *filename == '\0')
    #endif
        return(0);  //invalid filename

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Returns only the directory name from the full path.
//
// [out] dirname   : Directory name portion of the full path.
// [in] maxdirname : Max size of the "dirname" string.
// [in] fullpath   : Path to extract the directory name from.
//
// Return : On success 1 is returned.  Returns 0 if the path is
//          invalid.
//
int CFSUtils::GetDirPath(char *dirname, int maxdirname, const char *fullpath)
{
    char *ptr, *pathptr;

    if (dirname == NULL || maxdirname == 0 || fullpath == NULL)
        return(0);

    if (dirname == fullpath) {
        if ((pathptr = (char *)malloc(strlen(fullpath)+1)) != NULL)
            strcpy(pathptr,fullpath);
        else
            return(0);
    } else {
        pathptr = (char *)fullpath;
    }

    if ((ptr = strrchr(pathptr,FSUTILS_SLASH)) == NULL) {
        *dirname = '\0';    //the fullpath is a filename
    } else {
        if ((ptr-pathptr+2) <= maxdirname) {
            strncpy(dirname,pathptr,ptr-pathptr+1);   //copy the dir part
            dirname[ptr-pathptr+1] = '\0';
        } else {
            *dirname = '\0';    //the buffer is too small
        }
    }

    if (dirname == fullpath)
        free(pathptr);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Returns a pointer into "fullpath" after the "rootpath" part.
//
// [in] fullpath : Full path to get a pointer into.
// [in] rootpath : Root path to get te pointer after.
//
// Return : A pointer into "fullpath" after the "rootpath" part.
//          If "fullpath" does not begin with "rootpath", NULL is
//          returned.
//
char *CFSUtils::GetRelPathPtr(char *fullpath, char *rootpath)
{

        //make sure fullpath begins with userroot
    #ifdef WIN32
      if (_strnicmp(fullpath,rootpath,strlen(rootpath)) != 0)   //case insensitive for Windows
    #else
      if (strncmp(fullpath,rootpath,strlen(rootpath)) != 0)     //case sensitive for UNIX
    #endif
        return(NULL);   //fullpath does not start with rootpath

    return(fullpath + strlen(rootpath));
}

//////////////////////////////////////////////////////////////////////
// Remove any leading slashes from a path.
//
// [in/out] path : Path to remove leading slashes from.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::RemoveSlashLead(char *path)
{
    int i;

        //remove any leading '/'
    if (*path == '/' || *path == '\\') {
        for (i = 1; path[i] != '\0'; i++)
            path[i-1] = path[i];
        path[i-1] = '\0';
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Remove any trailing slashes from a path.
//
// [in/out] path : Path to remove trailing slashes from.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::RemoveSlashEnd(char *path)
{
    int len;

    if (path == NULL)
        return(0);

        //remove any trailing slashes
    len = strlen(path);
    if (len > 0) {
        if (*(path+len-1) == '\\' || *(path+len-1) == '/')
            *(path+len-1) = '\0';
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Sets the current working directory.
//
// [in] cwd : New current working directory.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::SetCWD(char *cwd)
{
    
    if (cwd == NULL)
        return(0);

    if (chdir(cwd) == 0)
        return(1);
    else
        return(0);
}

//////////////////////////////////////////////////////////////////////
// Gets the current working directory.
//
// [out] cwd   : Current working directory.
// [in] maxcwd : Max size of the "cwd" string.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::GetCWD(char *cwd, int maxcwd)
{
    
    if (cwd == NULL || maxcwd == 0)
        return(0);

    *cwd = '\0';
    if (getcwd(cwd,maxcwd) != NULL) {
        cwd[maxcwd-1] = '\0';   //make sure the path is NULL terminated
    } else {
        *cwd = '\0';
    }

    return(1);
}

////////////////////////////////////////
// Functions for basic filesystem
// actions
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Deletes a specified file.
//
// [in] path : Path to the file to be deleted.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::DelFile(const char *path)
{

    if (path == NULL)
        return(0);

    if (remove(path) == 0)
        return(1);  //file successfully deleted
    else
        return(0);  //error deleting the file
}

//////////////////////////////////////////////////////////////////////
// Make a directory.
//
// [in] path : Path for the directory to be created.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::MkDir(const char *path)
{

    if (path == NULL)
        return(0);

    #ifdef WIN32
      if (mkdir(path) == 0) {
    #else
      if (mkdir(path,0000755) == 0) {   //make dir w/permissions "rwxr-xr-x"
    #endif
        return(1);  //dir successfully created
    } else {
        return(0);  //error creating the dir
    }
}

//////////////////////////////////////////////////////////////////////
// Remove a directory.
//
// [in] path : Path of the directory to remove.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: The directory being removed must be empty.
//
int CFSUtils::RmDir(const char *path)
{

    if (path == NULL)
        return(0);

    if (rmdir(path) == 0) {
        return(1);  //dir successfully removed
    } else {
        return(0);  //error removing the dir
    }
}

//////////////////////////////////////////////////////////////////////
// Rename a file or directory.
//
// [in] oldname : file/dir to be renamed.
// [in] newname : new file/dir name.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::Rename(const char *oldname, const char *newname)
{

    if (oldname == NULL || newname == NULL)
        return(0);

    if (rename(oldname,newname) == 0) {
        return(1);  // file/directory successfully renamed
    } else {
        return(0);  // error renaming the file/directory
    }
}


////////////////////////////////////////
// Functions for advanced filesystem
// actions
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Copy files or directories.
//
// [in] input  : file or directory to copy
// [in] output : destination file/directory
//
// Return : returns the number of files copied.
//
// NOTE: If "input" is a file and "output" is a directory, a file
//       with the name "input" will be created in the "output"
//       directory.  If "input" is a directory and "output" is an
//       existing directory, then the contents of "input" will
//       recursively be copied to the "output" directory.  If "input"
//       is a directory and "output" is not an existing directory,
//       a new directory will be created for that directory.  If
//       "input" is a wildcard, output is assumed to be a directory.
//       If both "input" and "output" are files, "input" will be
//       copied to "output".  Wildcards can be used when specifying
//       "input".
int CFSUtils::CopyFiles(char *input, char *output)
{
    CStrUtils strutils;
    char *buffer, *tmpout, *tmpin, *outputbuff, *inputbuff, *indirptr = NULL, *infileptr = NULL;
    int type, ncopied = 0, flagdir = 0, flagwildcard = 0;
    long handle;

    if (input == NULL || output == NULL)
        return(0);

    if ((inputbuff = (char *)malloc(strlen(input)+3)) == NULL)
        return(0);
    strcpy(inputbuff,input);
    CheckSlash(inputbuff);  //make sure the slashes are correct for the OS

    indirptr = inputbuff;     //set the dir pointer to the start of the path
    if (strchr(inputbuff,'*') == NULL) {    //if no wildcards
        type = ValidatePath(inputbuff);
        if (type == 0) {
            free(inputbuff);
            return(0);  //input path does not exist (exit)
        }
        if (type == 1) {
            flagdir = 1;    //input is a directory
        } else {
            flagdir = 0;    //input is a file
            if ((infileptr = GetFileNamePtr(inputbuff)) == NULL)
                infileptr = inputbuff;
        }
    } else {
        flagwildcard = 1;   //the path contains wildcards
        if ((infileptr = GetFileNamePtr(inputbuff)) == NULL) {
            CreateRelativePath(inputbuff,strlen(input)+3);  //add leading "./" if necessary
            infileptr = inputbuff;      //only a file name was specified
        } else {
            *(infileptr - 1) = '\0';    //terminate the dir name
        }
    }

        //if only a single file should be copied
    if (flagdir == 0 && flagwildcard == 0) {
        type = ValidatePath(output);
        if (type == 0) {
            free(inputbuff);
                //output is a new filename
            return(CopySingleFile(input,output));
        } else {
            if (type == 1) {  //the output path is a directory
                if ((buffer = (char *)malloc(strlen(output)+strlen(infileptr)+2)) != NULL) {
                    strcpy(buffer,output);
                    CheckSlash(buffer);
                    CheckSlashEnd(buffer,strlen(output)+strlen(infileptr)+2);
                    strcat(buffer,infileptr);   //build the full output file path
                    ncopied = CopySingleFile(inputbuff,buffer);   //copy the file
                    free(buffer);
                    free(inputbuff);
                    return(ncopied);
                } else {
                    free(inputbuff);
                    return(0);
                }
            } else {
                free(inputbuff);
                return(CopySingleFile(input,output));   //overwrite an existing file
            }
        }
    }

        //copy the output path to a buffer
    if ((outputbuff = (char *)malloc(strlen(output)+2)) == NULL) {
        free(inputbuff);
        return(0);
    }
    strcpy(outputbuff,output);
        //make sure output is a directory (create if necessary)
    if (VerifyOutputDir(outputbuff,strlen(output)+2) == 0) {
        free(outputbuff);
        free(inputbuff);
        //printf("CopyFiles: Invalid output path '%s'\n",output);
        return(0);  //could not verify/create the output dir
    }

        //allocate buffers
    buffer = (char *)malloc(FSUTILS_MAXPATH);
    tmpout = (char *)malloc(FSUTILS_MAXPATH);
    tmpin = (char *)malloc(FSUTILS_MAXPATH);
    if (buffer == NULL || tmpout == NULL || tmpin == NULL) {
        free(outputbuff);
        free(inputbuff);
        if (tmpin != NULL) free(tmpin);
        if (tmpout != NULL) free(tmpout);
        if (buffer != NULL) free(buffer);
        return(0);
    }

        //if a wildcard was specified
    if (flagwildcard != 0) {
            //scan through the input directory
        if ((handle = DirGetFirstFile(inputbuff,buffer,FSUTILS_MAXPATH)) != 0) {
            do {
                if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
                    if (strutils.MatchWildcard(infileptr,buffer) != 0) {
                        if (infileptr != inputbuff) {
                            strutils.SNPrintf(tmpin,FSUTILS_MAXPATH-1,"%s",indirptr);
                            CheckSlashEnd(tmpin,FSUTILS_MAXPATH);
                        } else {
                            *tmpin = '\0';
                        }
                        strncat(tmpin,buffer,FSUTILS_MAXPATH-strlen(tmpin)-1);
                        tmpin[FSUTILS_MAXPATH-1] = '\0';
                        strutils.SNPrintf(tmpout,FSUTILS_MAXPATH,"%s%s",outputbuff,buffer);
                        ncopied += CopySingleFile(tmpin,tmpout);
                    }
                }
            } while (DirGetNextFile(handle,buffer,FSUTILS_MAXPATH) != 0);
            DirClose(handle);
        }
    } else if (flagdir != 0) {
            //scan through the input directory
        if ((handle = DirGetFirstFile(inputbuff,buffer,FSUTILS_MAXPATH)) != 0) {
             do {
                if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
                    strutils.SNPrintf(tmpin,FSUTILS_MAXPATH-1,"%s",indirptr);
                    CheckSlashEnd(tmpin,FSUTILS_MAXPATH);   //make sure str ends in a slash
                    strncat(tmpin,buffer,FSUTILS_MAXPATH-strlen(tmpin)-1);
                    tmpin[FSUTILS_MAXPATH-1] = '\0';
                    type = ValidatePath(tmpin);
                    if (type != 0) {
                        strutils.SNPrintf(tmpout,FSUTILS_MAXPATH,"%s%s",outputbuff,buffer);
                        if (type == 2)
                            ncopied += CopySingleFile(tmpin,tmpout);    //tmpin is a file
                        else
                            ncopied += CopyFiles(tmpin,tmpout);         //tmpin is a dir (recurse)
                    }
                }
            } while (DirGetNextFile(handle,buffer,FSUTILS_MAXPATH) != 0);
            DirClose(handle);
        }
    }

    free(tmpin);
    free(tmpout);
    free(buffer);
    free(outputbuff);
    free(inputbuff);

    return(ncopied);
}

//////////////////////////////////////////////////////////////////////
// Copies a single file
//
// [in] inputfilename  : file to be copied
// [in] outputfilename : destination file
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::CopySingleFile(char *inputfilename, char *outputfilename)
{
    FILE *fdr, *fdw;
    char *buffer;
    int nbytes = 0, transferbuffersize = 512;

    if (ValidatePath(inputfilename) != 2)
        return(0);  //input is not a file

    if ((fdr = fopen(inputfilename,"rb")) == NULL)
        return(0);
    if ((fdw = fopen(outputfilename,"wb")) == NULL) {
        fclose(fdr);
        return(0);
    }
    //printf("Copy %s -> %s\n",inputfilename,outputfilename);

    if ((buffer = (char *)malloc(transferbuffersize)) == NULL) {
        fclose(fdw);
        fclose(fdr);
        return(0);
    }

    while ((nbytes = fread(buffer,1,transferbuffersize,fdr)) > 0)
        fwrite(buffer,1,nbytes,fdw);

    free(buffer);

    fclose(fdw);
    fclose(fdr);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Similar to CopyFiles except only files are moved (not directories)
//
// [in] input  : file(s) to be moved
// [in] output : destination file/directory
//
// Return : returns the number of files moved.
//
// NOTE: Wildcards can be used when specifying "input".
//
int CFSUtils::MoveFiles(char *input, char *output)
{
    CStrUtils strutils;
    char *buffer, *tmpout, *tmpin, *outputbuff, *inputbuff, *indirptr = NULL, *infileptr = NULL;
    int type, nmoved = 0, flagwildcard = 0;
    long handle;

    if (input == NULL || output == NULL)
        return(0);

    if ((inputbuff = (char *)malloc(strlen(input)+3)) == NULL)
        return(0);
    strcpy(inputbuff,input);
    CheckSlash(inputbuff);  //make sure the slashes are correct for the OS

    indirptr = inputbuff;     //set the dir pointer to the start of the path
    if (strchr(inputbuff,'*') == NULL) {    //if no wildcards
        if (ValidatePath(inputbuff) != 2) {
            free(inputbuff);
            return(0);  //input path must be a file
        }
        if ((infileptr = GetFileNamePtr(inputbuff)) == NULL)    //input is a file
            infileptr = inputbuff;
    } else {
        flagwildcard = 1;   //the path contains wildcards
        if ((infileptr = GetFileNamePtr(inputbuff)) == NULL) {
            CreateRelativePath(inputbuff,strlen(input)+3);  //add leading "./" if necessary
            infileptr = inputbuff;      //only a file name was specified
        } else {
            *(infileptr - 1) = '\0';    //terminate the dir name
        }
    }

        //if only a single file should be moved
    if (flagwildcard == 0) {
        type = ValidatePath(output);
        if (type == 0) {
            free(inputbuff);
            //printf("Move %s -> %s\n",input,output);
            if (rename(input,output) == 0) {  //output is a new file name (file -> file)
                return(1);
            } else {
                return(0);
            }
        } else {
            if (type == 1) {  //the output path is a directory (file -> dir/file)
                if ((buffer = (char *)malloc(strlen(output)+strlen(infileptr)+2)) != NULL) {
                    strcpy(buffer,output);
                    CheckSlash(buffer);
                    CheckSlashEnd(buffer,strlen(output)+strlen(infileptr)+2);
                    strcat(buffer,infileptr);   //build the full output file path
                    remove(buffer);             //delete the previous file (if necessary)
                    //printf("Move %s -> %s\n",inputbuff,buffer);
                    nmoved = (rename(inputbuff,buffer) == 0) ? 1 : 0;   //move the file
                    free(buffer);
                    free(inputbuff);
                    return(nmoved);
                } else {
                    free(inputbuff);
                    return(0);
                }
            } else {
                free(inputbuff);
                remove(output);                 //delete the previous file
                //printf("Move %s -> %s\n",input,output);
                if (rename(input,output) == 0) {  //overwrite an existing file (file -> file)
                    return(1);
                } else {
                    return(0);
                }
            }
        }
    }

        //copy the output path to a buffer
    if ((outputbuff = (char *)malloc(strlen(output)+2)) == NULL) {
        free(inputbuff);
        return(0);
    }
    strcpy(outputbuff,output);
        //make sure output is a directory (create if necessary)
    if (VerifyOutputDir(outputbuff,strlen(output)+2) == 0) {
        free(outputbuff);
        free(inputbuff);
        return(0);  //could not verify/create the output dir
    }

        //allocate buffers
    buffer = (char *)malloc(FSUTILS_MAXPATH);
    tmpout = (char *)malloc(FSUTILS_MAXPATH);
    tmpin = (char *)malloc(FSUTILS_MAXPATH);
    if (buffer == NULL || tmpout == NULL || tmpin == NULL) {
        free(outputbuff);
        free(inputbuff);
        if (tmpin != NULL) free(tmpin);
        if (tmpout != NULL) free(tmpout);
        if (buffer != NULL) free(buffer);
        return(0);
    }

        //if a wildcard was specified
    if (flagwildcard != 0) {
            //scan through the input directory
        if ((handle = DirGetFirstFile(inputbuff,buffer,FSUTILS_MAXPATH)) != 0) {
            do {
                if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
                    if (strutils.MatchWildcard(infileptr,buffer) != 0) {
                        if (infileptr != inputbuff) {
                            strncpy(tmpin,indirptr,FSUTILS_MAXPATH-2);
                            tmpin[FSUTILS_MAXPATH-2] = '\0';
                            CheckSlashEnd(tmpin,FSUTILS_MAXPATH);
                        } else {
                            *tmpin = '\0';
                        }
                        strncat(tmpin,buffer,FSUTILS_MAXPATH-strlen(tmpin)-1);
                        tmpin[FSUTILS_MAXPATH-1] = '\0';
                        strutils.SNPrintf(tmpout,FSUTILS_MAXPATH,"%s%s",outputbuff,buffer);
                        remove(tmpout);             //delete the previous file (if necessary)
                        //printf("Move %s -> %s\n",tmpin,tmpout);
                        nmoved += (rename(tmpin,tmpout) == 0) ? 1 : 0;
                    }
                }
            } while (DirGetNextFile(handle,buffer,FSUTILS_MAXPATH) != 0);
            DirClose(handle);
        }
    }

    free(tmpin);
    free(tmpout);
    free(buffer);
    free(outputbuff);
    free(inputbuff);

    return(nmoved);
}

//////////////////////////////////////////////////////////////////////
// Deletes the file(s)/directory specified.
//
// [in] input        : file(s)/dir to be deleted
// [in] flagkeepdirs : if not 0, only files, not dirs, will be deleted
//
// Return : returns the number of files and dirs deleted.
//
// NOTE: Wildcards can be used when specifying "input".  If "input"
//       is a directory, it will be recursively deleted
//
int CFSUtils::DeleteFiles(char *input, int flagkeepdirs /*=0*/)
{
    CStrUtils strutils;
    char *buffer, *tmpin, *inputbuff, *indirptr = NULL, *infileptr = NULL;
    int type, ndeleted = 0, flagdir = 0, flagwildcard = 0;
    long handle;

    if (input == NULL)
        return(0);

    if ((inputbuff = (char *)malloc(strlen(input)+3)) == NULL)
        return(0);
    strcpy(inputbuff,input);
    CheckSlash(inputbuff);  //make sure the slashes are correct for the OS

    indirptr = inputbuff;     //set the dir pointer to the start of the path
    if (strchr(inputbuff,'*') == NULL) {    //if no wildcards
        type = ValidatePath(inputbuff);
        if (type == 0) {
            free(inputbuff);
            return(0);  //input path does not exist (exit)
        }
        if (type == 1) {
            flagdir = 1;    //input is a directory
        } else {
            flagdir = 0;    //input is a file
            if ((infileptr = GetFileNamePtr(inputbuff)) == NULL)
                infileptr = inputbuff;
        }
    } else {
        flagwildcard = 1;   //the path contains wildcards
        if ((infileptr = GetFileNamePtr(inputbuff)) == NULL) {
            CreateRelativePath(inputbuff,strlen(input)+3);  //add leading "./" if necessary
            infileptr = inputbuff;      //only a file name was specified
        } else {
            *(infileptr - 1) = '\0';    //terminate the dir name
        }
    }

    buffer = (char *)malloc(FSUTILS_MAXPATH);
    tmpin = (char *)malloc(FSUTILS_MAXPATH);
    if (buffer == NULL || tmpin == NULL) {
        free(inputbuff);
        if (tmpin != NULL) free(tmpin);
        if (buffer != NULL) free(buffer);
        return(0);
    }

        //if only a single file should be deleted
    if (flagdir == 0 && flagwildcard == 0) {
        ndeleted = (remove(input) == 0) ? 1 : 0;
    } else if (flagwildcard != 0) {
            //scan through the input directory
        if ((handle = DirGetFirstFile(inputbuff,buffer,FSUTILS_MAXPATH)) != 0) {
            do {
                if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
                    if (strutils.MatchWildcard(infileptr,buffer) != 0) {
                        if (infileptr != inputbuff) {
                            strncpy(tmpin,indirptr,FSUTILS_MAXPATH-2);
                            tmpin[FSUTILS_MAXPATH-2] = '\0';
                            CheckSlashEnd(tmpin,FSUTILS_MAXPATH);
                        } else {
                            *tmpin = '\0';
                        }
                        strncat(tmpin,buffer,FSUTILS_MAXPATH-strlen(tmpin)-1);
                        tmpin[FSUTILS_MAXPATH-1] = '\0';
                        //printf("Delete %s\n",tmpin);
                        ndeleted += (remove(tmpin) == 0) ? 1 : 0;
                    }
                }
            } while (DirGetNextFile(handle,buffer,FSUTILS_MAXPATH) != 0);
            DirClose(handle);
        }
    } else if (flagdir != 0) {
            //scan through the input directory
        if ((handle = DirGetFirstFile(inputbuff,buffer,FSUTILS_MAXPATH)) != (long)NULL) {
            do {
                if (strcmp(buffer,".") != 0 && strcmp(buffer,"..") != 0) {
                    strncpy(tmpin,indirptr,FSUTILS_MAXPATH-2);
                    tmpin[FSUTILS_MAXPATH-2] = '\0';
                    CheckSlashEnd(tmpin,FSUTILS_MAXPATH);   //make sure str ends in a slash
                    strncat(tmpin,buffer,FSUTILS_MAXPATH-strlen(tmpin)-1);
                    tmpin[FSUTILS_MAXPATH-1] = '\0';
                    type = ValidatePath(tmpin);
                    if (type != 0) {
                        if (type == 2) {
                            //printf("Delete %s\n",tmpin);
                            ndeleted += (remove(tmpin) == 0) ? 1 : 0;    //tmpin is a file
                        } else {
                            ndeleted += DeleteFiles(tmpin,flagkeepdirs); //tmpin is a dir (recurse)
                        }
                    }
                }
            } while (DirGetNextFile(handle,buffer,FSUTILS_MAXPATH) != 0);
            DirClose(handle);
        }
        //printf("Delete directory %s\n",inputbuff);
        if (flagkeepdirs == 0)
            ndeleted += (rmdir(inputbuff) == 0) ? 1 : 0; //delete the root dir
    }

    free(tmpin);
    free(buffer);
    free(inputbuff);

    return(ndeleted);
}

//////////////////////////////////////////////////////////////////////
// Gets information for the disk specified by "fullpath."
//
// [in] fullpath  : path to get the disk information for
// [out] diskinfo : information for the specified disk
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CFSUtils::GetDiskInfo(const char *fullpath, fsutilsDiskInfo_t *diskinfo)
{
    int retval = 0;

    if (fullpath == NULL || diskinfo == NULL)
        return(0);

        //initialize the disk info struct
    memset(diskinfo,0,sizeof(fsutilsDiskInfo_t));

    #ifdef WIN32    // Windows
      ULARGE_INTEGER freebytes, totalbytes, totalfreebytes;
      char buffer[4] = "C:\\";
      *buffer = *fullpath;
      if (GetDiskFreeSpaceEx(buffer,&freebytes,&totalbytes,&totalfreebytes)) {
        diskinfo->bytesfree = (double)((freebytes.HighPart * 4294967296) + freebytes.LowPart);
        diskinfo->totalbytes = (double)((totalbytes.HighPart * 4294967296) + totalbytes.LowPart);
        retval = 1;
      }
    #endif
    #ifdef BSD  // BSD
      long i, mntsize, blocksize;
      int headerlen, maxlen = 0;
      struct statfs *mntbuf;
      mntsize = getmntinfo(&mntbuf,MNT_NOWAIT);
      for (i = 0; i < mntsize; i++) {
          if (strncasecmp(mntbuf[i].f_mntonname,fullpath,strlen(mntbuf[i].f_mntonname)) == 0) {
              if (strlen(mntbuf[i].f_mntonname) > (unsigned)maxlen) {
                  getbsize(&headerlen,&blocksize);
                  diskinfo->bytesfree = (double)fsbtoblk(mntbuf[i].f_bavail,mntbuf[i].f_bsize,blocksize) * (double)blocksize;
                  diskinfo->totalbytes = (double)fsbtoblk(mntbuf[i].f_blocks,mntbuf[i].f_bsize,blocksize) * (double)blocksize;
                  maxlen = strlen(mntbuf[i].f_mntonname);
                  retval = 1;
              }
          }
      }
    #endif
    #ifdef SOLARIS  // Solaris
      statvfs_t buf;
      if (statvfs(fullpath,&buf) == 0) {
          diskinfo->bytesfree = (double)buf.f_bfree * (double)buf.f_frsize;
          diskinfo->totalbytes = (double)buf.f_blocks * (double)buf.f_frsize;
          retval = 1;
      }
    #endif
    #ifdef LINUX    // Linux
      struct statfs buf;
      if (statfs(fullpath,&buf) == 0) {
          diskinfo->bytesfree = (double)buf.f_bfree * (double)buf.f_bsize;
          diskinfo->totalbytes = (double)buf.f_blocks * (double)buf.f_bsize;
          retval = 1;
      }
    #endif

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //Converts a path with relative components into an absolute path.
    //NOTE: This function assumes that all paths have first gone
    //      through CheckSlash() and CheckEnd().
int CFSUtils::CompactPath(char *path)
{
    char *tmpptr, *pathptr, *ptr;
    char *buffer;

    if ((buffer = (char *)malloc(strlen(path)+1)) == NULL)
        return(0);

        //remove all occurrences of "./" in the path.
        //look for "./" only
    pathptr = path;
    #ifdef WIN32
      while ((ptr = strstr(pathptr,".\\")) != NULL) {
    #else
      while ((ptr = strstr(pathptr,"./")) != NULL) {
    #endif
        if (ptr == path || *(ptr-1) != '.') {
                //make xxx/./yyy/ -> "xxx/yyy/"
            *ptr = '\0';
            strcpy(buffer,path);
            strcat(buffer,ptr+2);
            strcpy(path,buffer);
            pathptr = ptr;
        } else {
            pathptr = ptr + 2;
        }
    }

        //remove all occurrences of "../" in the path.
        //look for "../" only
    #ifdef WIN32
      while ((ptr = strstr(path,"..\\")) != NULL) {
    #else
      while ((ptr = strstr(path,"../")) != NULL) {
    #endif
        if (ptr == path)
            *ptr = '\0';
        else
            *(ptr-1) ='\0';
        if ((tmpptr = strrchr(path,FSUTILS_SLASH)) == NULL) {
                //make "yyy/../zzz/" -> "zzz/"
            strcpy(buffer,ptr+3);
            strcpy(path,buffer);
        } else {
                //make "xxx/yyy/../zzz/" -> "xxx/zzz/"
            *(tmpptr+1) = '\0';
            strcpy(buffer,path);
            strcat(buffer,ptr+3);
            strcpy(path,buffer);
        }
    }

    free(buffer);
    return(1);
}

    //Copies "path1path2" into "outpath"
int CFSUtils::CopyPaths(char *outpath, int maxpathsize, const char *path1, const char *path2)
{
    int len;

    if (maxpathsize < 1)
        return(0);

    strncpy(outpath,path1,maxpathsize-1);
    outpath[maxpathsize-1] = '\0';
    len = strlen(outpath);
    strncat(outpath,path2,maxpathsize-1-len);
    outpath[maxpathsize-1] = '\0';

    return(1);
}

char *CFSUtils::GetFileNamePtr(char *path)
{
    char *fileptr;

    if ((fileptr = strrchr(path,FSUTILS_SLASH)) == NULL)
        return(NULL);

    fileptr++;
    if (fileptr == '\0')    //no filename
        return(NULL);
    else
        return(fileptr);
}

void CFSUtils::CreateRelativePath(char *path, int maxpath)
{
    char *tmpbuff;

    if (maxpath < 3)
        return;

        //check if the path is already a relative path
    #ifdef WIN32
      if (strncmp(path,".\\",2) == 0 || strncmp(path,"..\\",3) == 0)
    #else
      if (strncmp(path,"./",2) == 0 || strncmp(path,"../",3) == 0)
    #endif
        return;

    if ((tmpbuff = (char *)malloc(strlen(path)+1)) == NULL)
        return;
    strcpy(tmpbuff,path);

    #ifdef WIN32
      strcpy(path,".\\");
    #else
      strcpy(path,"./");
    #endif
    strncat(path,tmpbuff,maxpath-3);
    path[maxpath-1] = '\0';

    free(tmpbuff);
}

    //NOTE: path may grow by 1 byte (from CheckSlashEnd())
int CFSUtils::VerifyOutputDir(char *path, int maxpath)
{

    CheckSlash(path);

    RemoveSlashEnd(path);   //remove the ending slash if necessary

    MkDir(path);            //create the path if necessary

        //make sure the dir exists
    if (ValidatePath(path) != 1)
        return(0);

    if ((strlen(path) + 2) <= (unsigned)maxpath)
        CheckSlashEnd(path,maxpath);    //make sure there is an ending slash

    return(1);
}
