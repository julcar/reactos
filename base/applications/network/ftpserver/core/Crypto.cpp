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
// Crypto.cpp: implementation of the CCrypto class.
//
// This class provides encryption and decryption using the Blowfish
// algorithm
//
//////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Crypto.h"
#include "BlowfishCrypt.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCrypto::CCrypto()
{

}

CCrypto::~CCrypto()
{

}


//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// This function outputs a printable ASCII HEX string of the
// encrypted data.  "outputsize" is used to specify how many
// bytes the output should be.  The size of the returned
// buffer will be the max of "outputsize" and strlen(buffer).
// The HEX representation of the output will be twice the size of
// the output in bytes.  The input "buffer" is assumed to be a
// NULL terminated ASCII string.
//
// [in] buffer     : The string that will be encrypted.
// [in] username   : Username to use for the encryption.
// [in] password   : Password to use for the encryption.
// [in] outputsize : Size of the output in bytes (Hex representation
//                   will be 2 * outputsize)
//
// Return : A dynamically allocated buffer containing the ASCII HEX
//          representation of the encrypted "buffer" is returned on
//          success. NULL is returned on failure.
//
// NOTE: For Blowfish compatibility the returned encrypted data size
//       will always be a multiple of 8 bytes.
//
char *CCrypto::Encrypt2HexStr(char *buffer, const char *username, const char *password, int outputsize /*=0*/)
{
    CBlowfishCrypt blowfish;
    CBlowfishCrypt::Block blk;
    char *inbuff, *outbuff, *databuff, *pwbuff;;
    unsigned int outsize, pwlen;

    if (username == NULL || password == NULL || buffer == NULL)
        return(NULL);

        //combine the username and password into 1 string
    pwlen = strlen(username)+strlen(password);
    if ((pwbuff = (char *)malloc(pwlen+1)) == NULL)
        return(NULL);
    strcpy(pwbuff,username);
    strcat(pwbuff,password);

        //Initialize blowfish
    blowfish.Initialize((unsigned char *)pwbuff);   //set the blowfish password
    blk.l = 1; blk.r = 1;
    
    free(pwbuff);

    outsize = outputsize;
    if (outsize < strlen(buffer))
        outsize = strlen(buffer);

        //make the output size a multiple of 8
    outsize = blowfish.GetOutputLength(outsize);
    
        //allocate the buffer for the input data
    if ((inbuff = (char *)calloc(outsize,1)) == NULL)
        return(NULL);
    memcpy(inbuff,buffer,strlen(buffer));
        //allocate the buffer for the output data
    if ((outbuff = (char *)calloc(outsize,1)) == NULL) {
        free(inbuff);
        return(NULL);
    }

    blowfish.Encrypt((unsigned char *)outbuff,(unsigned char *)inbuff,outsize,blk);

    free(inbuff);

        //convert the output buffer to a printable ASCII HEX string
    databuff = Binary2Hex(outbuff,outsize);

    free(outbuff);

    return(databuff);
}

//////////////////////////////////////////////////////////////////////
// This decrypts the HEX string produced by Encrypt2HexStr()
//
// [in] buffer   : The ASCII HEX string to be decrypted.
// [in] username : Username to use for the decryption.
// [in] password : Password to use for the decryption.
//
// Return : A dynamically allocated buffer containing the decrypted
//          string.
//
char *CCrypto::DecryptHexStr(char *buffer, const char *username, const char *password)
{
    CBlowfishCrypt blowfish;
    CBlowfishCrypt::Block blk;
    char *inbuff, *outbuff, *pwbuff;;
    unsigned int outsize, pwlen;

    if (username == NULL || password == NULL || buffer == NULL)
        return(NULL);

        //combine the username and password into 1 string
    pwlen = strlen(username)+strlen(password);
    if ((pwbuff = (char *)malloc(pwlen+1)) == NULL)
        return(NULL);
    strcpy(pwbuff,username);
    strcat(pwbuff,password);

        //Initialize blowfish
    blowfish.Initialize((unsigned char *)pwbuff);   //set the blowfish password
    blk.l = 1; blk.r = 1;
    
    free(pwbuff);

        //get the input data in binary form
    if ((inbuff = Hex2Binary(buffer,strlen(buffer))) == NULL)
        return(NULL);
    
    outsize = strlen(buffer) / 2;   //2 HEX chars per 1 binary

        //allocate the buffer for the output data
    if ((outbuff = (char *)calloc(outsize+1,1)) == NULL) {
        free(inbuff);
        return(NULL);
    }

    blowfish.Decrypt((unsigned char *)outbuff,(unsigned char *)inbuff,outsize,blk);

    outbuff[outsize] = '\0';    //make sure the output string is NULL terminated

    free(inbuff);

    return(outbuff);
}

//////////////////////////////////////////////////////////////////////
// Frees the dynamically allocated buffers in Encrypt2HexStr() and
// DecryptHexStr().
//
// [in] databuff : Buffer to be freed.
//
// Return : VOID
//
void CCrypto::FreeDataBuffer(char *databuff)
{

    if (databuff != NULL)
        free(databuff);
}


//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //converts binary data to an ASCII HEX string
char *CCrypto::Binary2Hex(char *buffer, int buffsize)
{
    char *databuff;
    int i;

    if ((databuff = (char *)malloc(2*buffsize+1)) != NULL) {
        for (i = 0; i < buffsize; i++)
            sprintf(databuff+2*i,"%02X",*(buffer+i) & 0x000000FF);
    }
    *(databuff + 2*buffsize) = '\0';

    return(databuff);
}

    //converts an ASCII HEX string to binary
char *CCrypto::Hex2Binary(char *buffer, int buffsize)
{
    char *databuff, s[3];
    unsigned int i, len, tmp;

    len = buffsize/2;
    if ((databuff = (char *)calloc(len+1,1)) != NULL) {
        memset(s,0,3);
        for (i = 0; i < len; i++) {
            strncpy(s,buffer+2*i,2);
            sscanf(s,"%X",&tmp);
            *(databuff+i) = tmp;
        }
    }

    return(databuff);
}
