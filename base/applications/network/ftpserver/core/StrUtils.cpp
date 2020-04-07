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
// StrUtils.cpp: implementation of the CStrUtils class.
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h> //for va_list, etc.

#ifdef WIN32
  #include <conio.h>  //for getch()
#endif

#include "StrUtils.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CStrUtils::CStrUtils()
{

}

CStrUtils::~CStrUtils()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Case insensitive compare of str1 and str2.
//
// [in] str1 : First string to compare.
// [in] str2 : Second string to compare.
//
// Return : 0 if the strings are the same, !0 otherwise.
//
int CStrUtils::CaseCmp(const char *str1, const char *str2)
{
    int retval;

    if (str1 == NULL || str2 == NULL)
        return(-1);

    #ifdef WIN32
        retval = stricmp(str1,str2);        //for WINDOWS
    #else
        retval = strcasecmp(str1,str2);     //for UNIX
    #endif

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// Compares not more than "len" characters of "str1" and "str2".
// The comparison is not case-sensitive.
//
// [in] str1 : First string to compare.
// [in] str2 : Second string to compare.
// [in] len  : Number of bytes to compare.
//
// Return : 0 if the strings are the same, !0 otherwise.
//
int CStrUtils::NCaseCmp(const char *str1, const char *str2, int len)
{
    char *tmpbuf1, *tmpbuf2;
    int retval;

    if (str1 == NULL || str2 == NULL)
        return(-1);

    if ((tmpbuf1 = (char *)malloc(strlen(str1)+1)) == NULL)
        return(-1);

    if ((tmpbuf2 = (char *)malloc(strlen(str2)+1)) == NULL) {
        free(tmpbuf1);
        return(-1);
    }

    strcpy(tmpbuf1,str1);
    strcpy(tmpbuf2,str2);

    ToLower(tmpbuf1);
    ToLower(tmpbuf2);

    retval = strncmp(tmpbuf1,tmpbuf2,len);

    free(tmpbuf2);
    free(tmpbuf1);

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// Converts a string to all lower case ASCII characters (in-place).
//
// [in/out] buffer : string to be converted to all lower case.
//
// Return : VOID
//
void CStrUtils::ToLower(char *buffer)
{
    unsigned int i;

    if (buffer == NULL)
        return;

    for (i = 0; i < strlen(buffer); i++) {
        if (isalpha(*(buffer+i)))
            *(buffer+i) = tolower(*(buffer+i));
    }
}

//////////////////////////////////////////////////////////////////////
// Converts a string to all upper case ASCII characters (in-place).
//
// [in/out] buffer : string to be converted to all upper case.
//
// Return : VOID
//
void CStrUtils::ToUpper(char *buffer)
{
    unsigned int i;

    if (buffer == NULL)
        return;

    for (i = 0; i < strlen(buffer); i++) {
        if (isalpha(*(buffer+i)))
            *(buffer+i) = toupper(*(buffer+i));
    }
}

//////////////////////////////////////////////////////////////////////
// This function makes sure the string "buffer" ends in "\r\n"
//
// [in/out] buffer     : string to add end of line characters to.
// [in]  maxbuffersize : Max size of the buffer.
//
// Return : String length of the new "buffer."
//
// NOTE: If "\n" is detected it will be replaced by "\r\n"
//
int CStrUtils::AddEOL(char *buffer, int maxbuffersize)
{
    char *ptr;
    int n;

    if (buffer == NULL)
        return(0);

    n = strlen(buffer);
    if ((ptr = strchr(buffer,'\r')) != NULL) {  //if '\r' is found set it to '\0'
        *ptr = '\0';
        n = strlen(buffer);
    }
    if ((ptr = strchr(buffer,'\n')) != NULL) {  //if '\n' is found set it to '\0'
        *ptr = '\0';
        n = strlen(buffer);
    }

        //check if the buffer is big enough
    if (n+3 > maxbuffersize)
        return(0);

    buffer[n] = '\r';
    buffer[n+1] = '\n';
    buffer[n+2] = '\0';
    n+=2;

    return(n);  //return the length of the new string
}

//////////////////////////////////////////////////////////////////////
// Removes '\r' and '\n' in a line (in-place).
//
// [in/out] buffer : String to remove newline characters from.
//
// Return : String length of the new "buffer."
//
int CStrUtils::RemoveEOL(char *buffer)
{
    char *ptr;

    if (buffer == NULL)
        return(0);

    if ((ptr = strchr(buffer,'\r')) != NULL)    //if '\r' is found set it to '\0'
        *ptr = '\0';
    if ((ptr = strchr(buffer,'\n')) != NULL)    //if '\n' is found set it to '\0'
        *ptr = '\0';

    return(strlen(buffer));   //return the length of the new string
}

//////////////////////////////////////////////////////////////////////
// Appends the string "str" to the string "linebuf" without exceeding
// the length specified by "linebufsize."
//
// [in/out] linebuf     : Buffer to append "str" to.
// [in]     str         : String to append to "linebuf."
// [in]     linebufsize : Maximum size of "linebuf."
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CStrUtils::Append(char *linebuf, const char *str, int linebufsize)
{
    int len;

    if (linebuf == NULL || str == NULL || linebufsize == 0)
        return(0);

    len = strlen(linebuf);
    
    if (len == (linebufsize-1))
        return(0);  //the line buffer is full

    strncat(linebuf,str,linebufsize-len-1);

    linebuf[linebufsize-1] = '\0';

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Checks if a string contains only numbers.
//
// [in] buffer : String to check for all numbers.
//
// Return : If the string is all numbers 1 is returned, otherwise 0
//          is returned.
//
int CStrUtils::IsNum(const char *buffer)
{
    int i;

    if (buffer == NULL)
        return(0);

    if (strcmp(buffer,"-") == 0)
        return(0);  //if only a minus sign

    for (i = 0; i < (int)strlen(buffer); i++) {
        if (i == 0 && *buffer == '-')
            continue;   //for negative numbers
        if (isdigit(*(buffer+i)) == 0)
            return(0);
    }

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Removes the comments from the string "line" --where a comment is
// indicated by "commentstr."  The part of "line" that is not
// commented is kept (and returned).
//
// [in/out] line       : String to remove the comments from.
// [in]     commentstr : String used to indicate a comment (Ex "//").
//
// Return : Pointer to "line."
//
// NOTE: The comments must be in C++ "//" style (or "#" in scripts) format.
//       C-style comments such as /* */ are not supported.
//
char *CStrUtils::RemoveComments(char *line, const char *commentstr)
{
    char *ptr;

    if (line == NULL || commentstr == NULL)
        return(line);

    if ((ptr = strstr(line,commentstr)) != NULL)
        *ptr = '\0';

    return(line);
}

//////////////////////////////////////////////////////////////////////
// Gets a line from stdin without echoing the string to the screen.
// This function is needed when reading sensitive information, such
// as a password, and the input should not be displayed on the
// screen.
//
// [out] buffer        : Stores the string read from stdin.
// [in]  maxbuffersize : Max size of the read string.
//
char *CStrUtils::GetStrHidden(char *buffer, int maxbuffersize)
{

    if (buffer == NULL || maxbuffersize <= 0)
        return(buffer);

    #ifdef WIN32
      char c;
      int i = 0;

      do {
          c = getch();
          *(buffer+i) = c;
          if (c == 8) {    //if Backspace was pressed
              if (i > 0)
                  i--;
          } else {
              i++;
          }
      } while (c != '\n' && c != '\r' && i < maxbuffersize-1);
      *(buffer+i) = '\0';
    #else
      system("stty -echo");   // turn off echo
      fgets(buffer,maxbuffersize,stdin);
      system("stty echo");    // restore echo
    #endif
    printf("\n");

    return(buffer);
}

//////////////////////////////////////////////////////////////////////
// Builds a buffer based on a formatting string.
// This function is very similar to the time function strftime().
// It fills in all the occurrences of the specifiers, from the
// "specifiers" array, in the "format" string with the corresponding
// values in the "values" array.  "nparams" is the number of elements
// in the "specifiers" and "values" arrays.  "escchar" is the
// character used to start the specifier (Ex. '%').
//
// [in] format     : Format string.
// [in] specifiers : String containing the specifier characters.
// [in] values     : Array of strings used to substitute for the
//                   corresponding specifiers.
// [in] nparams    : Number of elements in "specifiers" or "values"
// [in] escchar    : Escape character used to indicate a specifier.
//
// Return : A dynamically allocated buffer containing the format
//          string with all the expanded specifiers.
//
// Ex. format = "User is %u and command is %m",
//     specifiers = "um", values = {"root","RETR"}, escchar = '%'
//     --> returned buffer = "User is root and command is RETR"
//
// NOTE: The returned buffer must be free().
//
char *CStrUtils::BuildFormattedString(const char *format, const char *specifiers, char *values[], int nparams, char escchar /*='%'*/)
{
    char *ptr, *buffer = NULL;
    int i, len;

    if (format == NULL || specifiers == NULL || values == NULL || nparams == 0)
        return(NULL);
        
        //get the length of the new buffer
    len = 0;
    for (ptr = (char *)format; *ptr != '\0'; ptr++) {
        if (*ptr == escchar) {
            ptr++;  //move to the next character
            if (*ptr == '\0')
                break;  //reached the end of the string
            if (*ptr == escchar) {
                len++;  //Ex. found "%%" -> print just "%"
            } else {
                    //lookup the value for the specifier
                for (i = 0; i < nparams; i++) {
                    if (*ptr == specifiers[i])
                        len += strlen(values[i]);
                }
            }
        } else {
            len++;
        }
    }

        //allocate the buffer
    if ((buffer = (char *)malloc(len+1)) == NULL)
        return(NULL);

        //write the log line
    len = 0;
    for (ptr = (char *)format; *ptr != '\0'; ptr++) {
        if (*ptr == escchar) {
            ptr++;  //move to the next character
            if (*ptr == '\0')
                break;  //reached the end of the string
            if (*ptr == escchar) {
                buffer[len] = escchar;
                len++;  //Ex. found "%%" -> print just "%"
            } else {
                    //lookup the value for the specifier
                for (i = 0; i < nparams; i++) {
                    if (*ptr == specifiers[i]) {
                        buffer[len] = '\0';
                        strcat(buffer,values[i]);   //fill in the value
                        len += strlen(values[i]);
                    }
                }
            }
        } else {
            buffer[len] = *ptr;
            len++;
        }

    }
    buffer[len] = '\0';

    return(buffer);
}

//////////////////////////////////////////////////////////////////////
// Checks if "filename" is contained in the "wildcard" string.  The
// wildcard character is '*'.
//
// [in] wildcard    : string containing the wildcard characters
//                    Ex. 192.168.*.*
// [in] matchstring : string to match to the wildcard.
//                    Ex. 192.168.60.211
//
// Return : 1 if "matchstring" is contained in "wildcard", 0 otherwise.
//
int CStrUtils::MatchWildcard(const char *wildcard, const char *matchstring)
{
    char *ptri, *ptra, *ptr1, *ptr2;
    char *allowed;

    if ((allowed = (char *)malloc(strlen(wildcard)+1)) == NULL)
        return(0);

    strcpy(allowed,wildcard);
    ptra = allowed;
    ptri = (char *)matchstring;
    for (;;) {
        if ((ptr1 = strchr(ptra,'*')) != NULL) {
            *ptr1 = '\0';
            if (ptra == allowed) {
                if (strncmp(ptra,ptri,strlen(ptra)) != 0) {
                    free(allowed); return(0);
                }
                ptr2 = ptri;
            } else {
                if ((ptr2 = strstr(ptri,ptra)) == NULL) {
                    free(allowed); return(0);
                }
            }
            ptri = ptr2 + strlen(ptra);
            ptra = ptr1 + 1;
        } else {
            if (ptra == allowed) {
                if (strcmp(ptra,ptri) != 0) {
                    free(allowed); return(0);
                } else {
                    free(allowed); return(1);
                }
            } else {
                if (*ptra == '\0') {
                    free(allowed); return(1);
                }
                if ((ptr2 = strstr(ptri,ptra)) == NULL) {
                    free(allowed); return(0);
                }
                if (*(ptr2 + strlen(ptra)) != '\0') {
                    free(allowed); return(0);
                }
                free(allowed);
                return(1);
            }
        }
    }

    free(allowed);
    return(0);
}

//////////////////////////////////////////////////////////////////////
// Prints a formatted string to a buffer and makes sure the buffer
// is NULL terminated.  This function is basically the same as
// sprintf.
//
// [out] buffer : buffer to write the formatted string to
// [in] size    : max size of the buffer
// [in] format  : format of the string to write to the buffer
// [in] ...     : arguments of the formatted string
//
// Return : 1 if "matchstring" is contained in "wildcard", 0 otherwise.
//
int CStrUtils::SNPrintf(char *buffer, unsigned int size, const char *format, ...)
{
    va_list parg;
    int nchars;

    if (buffer == NULL || size == 0 || format == NULL)
        return(0);

        //get the argument list
    va_start(parg,format);

        //write the arguments into the buffer
    #ifdef WIN32    //for Windows
        nchars = _vsnprintf((char*)buffer,size,format,parg);
    #else   //for UNIX
        nchars = vsnprintf((char*)buffer,size,format,parg);
    #endif

    va_end(parg);

        //make sure the string is terminated
    buffer[size-1] = '\0';

    return(nchars);
}

//////////////////////////////////////////////////////////////////////
// Makes sure a string is formatted to be contained in quotes.  All
// occurrences of " will be replaced by \" and all occurrences of \
// will be replaced by \\ if necessary.
//
// [in] buffer : String to reformat
//
// Return : A dynamically allocated buffer containing the formatted
//          string.
//
// NOTE: This function only adds escape characters if necessary.  A
//       string that is already formatted to be contained in quotes
//       will not be changed by this function.
//       The returned buffer must be free().
//
char *CStrUtils::CheckQuotedString(char *buffer)
{
    char *outstr;
    unsigned i, offset, len;

    if (buffer == NULL)
        return(NULL);

    len = strlen(buffer);

    if (len == 0) {
        return((char *)calloc(1,1));
    } else if (len == 1) {
        if ((outstr = (char *)calloc(3,1)) != NULL) {
            if (*buffer == '\\' || *buffer == '"')
                sprintf(outstr,"\\%c",*buffer);
            else
                sprintf(outstr,"%c",*buffer);
        }
        return(outstr);
    } else {
        if (buffer[0] == '"')
            len++;
        for (i = 1; i < strlen(buffer); i++) {
            if (buffer[i] == '"' && buffer[i-1] != '\\') {
                len++;
            } else if (buffer[i-1] == '\\' && buffer[i] != '\\') {
                len++;
            }
        }
        if (buffer[i-1] == '\\' && buffer[i-2] != '\\')
            len++;
        if ((outstr = (char *)calloc(len+1,1)) != NULL) {
            offset = 0;
            if (buffer[0] == '"')
                outstr[offset++] = '\\';
            outstr[offset++] = buffer[0];
            for (i = 1; i < strlen(buffer); i++) {
                if (buffer[i] == '"' && buffer[i-1] != '\\') {
                    outstr[offset++] = '\\';
                } else if (buffer[i-1] == '\\' && buffer[i] != '\\') {
                    outstr[offset++] = '\\';
                }
                outstr[offset++] = buffer[i];
            }
            if (buffer[i-1] == '\\' && buffer[i-2] != '\\')
                outstr[offset] = '\\';
        }
        return(outstr);
    }
}

//////////////////////////////////////////////////////////////////////
// Formats a string to be contained in quotes.  All occurrences of "
// will be replaced by \" and all occurrences of \ will be replaced
// by \\.
//
// [in] buffer : String to reformat
//
// Return : A dynamically allocated buffer containing the formatted
//          string.
//
// NOTE: The returned buffer must be free().
//
char *CStrUtils::BuildQuotedString(char *buffer)
{
    char *outstr;
    unsigned i, offset, len;

    if (buffer == NULL)
        return(NULL);

    len = strlen(buffer);

    for (i = 0; i < strlen(buffer); i++) {
        if (buffer[i] == '\\' || buffer[i] == '"')
            len++;
    }

    if ((outstr = (char *)calloc(len+1,1)) != NULL) {
        offset = 0;
        for (i = 0; i < strlen(buffer); i++) {
            if (buffer[i] == '\\' || buffer[i] == '"')
                outstr[offset++] = '\\';
            outstr[offset++] = buffer[i];
        }
    }

    return(outstr);
}
