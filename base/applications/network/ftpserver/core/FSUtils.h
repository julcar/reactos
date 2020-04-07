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
// FSUtils.h: interface for the CFSUtils class.
//
//////////////////////////////////////////////////////////////////////
#ifndef FSUTILS_H
#define FSUTILS_H

#include <sys/stat.h>

#define FSUTILS_MAXPATH 256     //max length of a file/dir path

#ifdef WIN32
  #define FSUTILS_SLASH '\\'    //Windows slash
#else
  #define FSUTILS_SLASH '/'     //UNIX slash
#endif

typedef struct {
    long groupid;
    long timeaccess;
    long timecreate;
    long devnum;
    long mode;
    long timemod;
    long size;
    long userid;
    long inodnum;   //only useful in UNIX
    long nlinks;    //only useful in UNIX
} fsutilsFileInfo_t;

typedef struct {
    double totalbytes;
    double bytesfree;
} fsutilsDiskInfo_t;

class CFSUtils  
{
public:
    CFSUtils();
    virtual ~CFSUtils();

        //directory/fileinfo functions
    long DirGetFirstFile(const char *dirpath, char *filename, int maxfilename);
    int DirGetNextFile(long handle, char *filename, int maxfilename);
    int DirClose(long handle);
    int DirIsEmpty(char *dirpath);
    int GetFileStats(char *filepath, fsutilsFileInfo_t *infoptr);
    int SetFileTime(char *filepath, long timemod, long timeaccess = 0);
    int GetUsrName(long uid, char *username, int maxusername);
    int GetGrpName(long gid, char *grpname, int maxgrpname);
    int GetPrmString(long mode, char *prmstr, int maxprmstr);

        //path functions
    char *BuildPath(const char *rootpath, const char *cwd, const char *arg);
    void FreePath(char *path);
    int ValidatePath(char *path);
    int CheckSlashLead(char *path, int maxpathsize);
    int CheckSlashEnd(char *path, int maxpathsize);
    int CheckSlash(char *outpath, const char *wrkdir = NULL);
    int CheckSlashUNIX(char *outpath, const char *wrkdir = NULL);
    int CatPath(char *path, int maxpathsize, const char *dir);
    int GetFileName(char *filename, int maxfilename, const char *fullpath);
    int GetDirPath(char *dirname, int maxdirname, const char *fullpath);
    char *GetRelPathPtr(char *fullpath, char *rootpath);
    int RemoveSlashLead(char *path);
    int RemoveSlashEnd(char *path);
    int SetCWD(char *cwd);
    int GetCWD(char *cwd, int maxcwd);

        //basic file functions
    int DelFile(const char *path);
    int MkDir(const char *path);
    int RmDir(const char *path);
    int Rename(const char *oldname, const char *newname);

        //advanced file functions
    int CopyFiles(char *input, char *output);
    int CopySingleFile(char *inputfilename, char *outputfilename);
    int MoveFiles(char *input, char *output);
    int DeleteFiles(char *input, int flagkeepdirs = 0);

        //disk drive information functions
    int GetDiskInfo(const char *fullpath, fsutilsDiskInfo_t *diskinfo);

private:
    int CompactPath(char *path);
    int CopyPaths(char *outpath, int maxpathsize, const char *path1, const char *path2);
    char *GetFileNamePtr(char *path);
    void CreateRelativePath(char *path, int maxpath);
    int VerifyOutputDir(char *path, int maxpath);
};

#endif //FSUTILS_H
