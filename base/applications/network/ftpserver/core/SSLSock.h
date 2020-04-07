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
// SSLSock.h: interface for the CSSLSock class.
//
//////////////////////////////////////////////////////////////////////
#ifndef SSLSOCK_H
#define SSLSOCK_H

#ifdef WIN32
  #include <winsock.h>
#else
  typedef int SOCKET;   //Make compatible with MSVC.
#endif

#ifdef INVALID_SOCKET
  #define SSLSOCK_INVALID INVALID_SOCKET
#else
  #define SSLSOCK_INVALID -1
#endif

typedef struct {
    char country[3];        //Country Name (2 letter code)
    char state[80];         //State or Province Name (full name)
    char locality[80];      //Locality Name (eg, city)
    char organization[80];  //Organization Name (eg, company)
    char orgunit[80];       //Organizational Unit Name (eg, section)
    char commonname[65];    //Common Name (eg, your name or your server's hostname)
    char emailaddr[41];     //Email Address
} sslsockCertInfo_t;    //stores the information for the x509 certificate


//-------------------------------------------------------------------
// Use the real header if SSL is enabled
//-------------------------------------------------------------------
#ifdef ENABLE_SSL


#include <openssl/ssl.h>
#include <openssl/x509_vfy.h> //contains return codes for VerifyCert()

typedef struct {
    SSL *ssl;       //SSL connection object
    SSL_CTX *ctx;   //SSL context object
    SSL_METHOD *meth;   //SSL method
} sslsock_t;    //SSL connection information


class CSSLSock
{
public:
    CSSLSock();
    virtual ~CSSLSock();

        //SSL Initialization
    int Initialize();
    int Uninitialize();

        //Functions to generate des3 PEM RSA public and private keys
    int GenKeysFile(const char *privpass, const char *privfilename, const char *pubfilename, int nbits = 1024);
    int GenKeysMem(const char *privpass, char **privkey, int *privkeysize, char **pubkey, int *pubkeysize, int nbits = 1024);
    char *LoadPrivKeyMem(const char *privfilename, int *privkeysize);
    char *LoadPubKeyMem(const char *pubfilename, int *pubkeysize);
    void FreeKeyMem(char *keybuf);

        //Functions to generate X509 certificates
    int GenX509File(const char *certfilename, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo);
    char *GenX509Mem(int *certsize, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo);
    char *LoadX509Mem(const char *certfilename, int *certsize);
    void FreeX509Mem(char *certbuf);

        //Functions for opening and closing SSL connections
    sslsock_t *SSLServerNeg(SOCKET sd, const char *privkeybuf, int privkeysize, const char *privpass, const char *certbuf, int certsize, int *errcode);
    sslsock_t *SSLServerNeg(SOCKET sd, sslsock_t *sslinfo, int *errcode);   //uses pre-existing context (from above call)
    sslsock_t *SSLClientNeg(SOCKET sd, int *errcode, int sslver = 2);
    void SSLClose(sslsock_t *sslinfo);
    void FreeSSLInfo(sslsock_t *sslinfo);

        //Functions for transfering data
    int SSLRecvN(sslsock_t *sslinfo, char *ptr, int nbytes);
    int SSLRecvLn(sslsock_t *sslinfo, char *ptr, int nmax);
    int SSLSendN(sslsock_t *sslinfo, const char *ptr, int nbytes);
    int SSLSendLn(sslsock_t *sslinfo, const char *ptr);

        //Functions to get SSL state
    char *GetCipherName(sslsock_t *sslinfo);
    void FreeCipherName(char *ciphername);
    long VerifyCert(sslsock_t *sslinfo);

private:
    int CheckCertInfo(sslsockCertInfo_t *certinfo);
    char *LoadFileToMem(const char *filepath, int *filesize);
    long GetFileSize(const char *filepath);
    void InitRand();
};


//-------------------------------------------------------------------
// Use an empty stub class if SSL is not enabled
// (return failure for all functions)
//-------------------------------------------------------------------
#else   //ENABLE_SSL


typedef struct {
        //just define the struct
    void *ssl;  //SSL connection object
    void *ctx;  //SSL context object
    void *meth; //SSL method
} sslsock_t;    //SSL connection information


class CSSLSock
{
public:
    CSSLSock() { }
    virtual ~CSSLSock() { }

        //SSL Initialization
    int Initialize() { return(0); }
    int Uninitialize() { return(0); }

        //Functions to generate des3 PEM RSA public and private keys
    int GenKeysFile(const char *privpass, const char *privfilename, const char *pubfilename, int nbits = 1024) { return(0); }
    int GenKeysMem(const char *privpass, char **privkey, int *privkeysize, char **pubkey, int *pubkeysize, int nbits = 1024) { return(0); }
    char *LoadPrivKeyMem(const char *privfilename, int *privkeysize) { return(NULL); }
    char *LoadPubKeyMem(const char *pubfilename, int *pubkeysize) { return(NULL); }
    void FreeKeyMem(char *keybuf) { }

        //Functions to generate X509 certificates
    int GenX509File(const char *certfilename, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo) { return(0); }
    char *GenX509Mem(int *certsize, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo) { return(NULL); }
    char *LoadX509Mem(const char *certfilename, int *certsize) { return(NULL); }
    void FreeX509Mem(char *certbuf) { }

        //Functions for opening and closing SSL connections
    sslsock_t *SSLServerNeg(SOCKET sd, const char *privkeybuf, int privkeysize, const char *privpass, const char *certbuf, int certsize, int *errcode) { return(NULL); }
    sslsock_t *SSLServerNeg(SOCKET sd, sslsock_t *sslinfo, int *errcode) { return(NULL); }
    sslsock_t *SSLClientNeg(SOCKET sd, int *errcode, int sslver = 2) { return(NULL); }
    void SSLClose(sslsock_t *sslinfo) { }
    void FreeSSLInfo(sslsock_t *sslinfo) { }

        //Functions for transfering data
    int SSLRecvN(sslsock_t *sslinfo, char *ptr, int nbytes) { return(-1); }
    int SSLRecvLn(sslsock_t *sslinfo, char *ptr, int nmax) { return(-1); }
    int SSLSendN(sslsock_t *sslinfo, const char *ptr, int nbytes) { return(-1); }
    int SSLSendLn(sslsock_t *sslinfo, const char *ptr) { return(-1); }

        //Functions to get SSL state
    char *GetCipherName(sslsock_t *sslinfo) { return(NULL); }
    void FreeCipherName(char *ciphername) { }
    long VerifyCert(sslsock_t *sslinfo) { return(0); }
};


#endif  //ENABLE_SSL


#endif //SSLSOCK_H
