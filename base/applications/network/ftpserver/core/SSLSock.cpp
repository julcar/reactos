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
// SSLSock.cpp: implementation of the CSSLSock class.
//
// This class is used for SSL connections.
//
//////////////////////////////////////////////////////////////////////


//-------------------------------------------------------------------
// Only define the class implementation is SSL is enabled
//-------------------------------------------------------------------
#ifdef ENABLE_SSL


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/asn1.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

    //for GetFileSize()
#include <sys/types.h>
#include <sys/stat.h>

#ifdef WIN32
  #include <fcntl.h>    //added for O_BINARY
#else
  #include <openssl/rand.h>
#endif

#include "Thr.h"
#include "SSLSock.h"

#if defined(MONOLITH) && !defined(OPENSSL_C)
  #define apps_startup() do_pipe_sig()
#else
  #ifdef WIN32
    #define apps_startup() _fmode=O_BINARY; CRYPTO_malloc_init(); SSLeay_add_all_algorithms()
  #else
    #define apps_startup() SSLeay_add_all_algorithms();
  #endif
#endif


//////////////////////////////////////////////////////////////////////
// OpenSSL threading callback functions
//////////////////////////////////////////////////////////////////////

    //Array to store all the mutexes available to OpenSSL
    //(used for static locking)
static thrSync_t *_mutexarray = NULL;

    //Structure used for dynamic locking
struct CRYPTO_dynlock_value {
    thrSync_t mutex;
};

    //Function used to lock/unlock the critical sections (static locking)
static void _SSLLockingFunction(int mode, int n, const char *srcfile, int line)
{
    CThr thr;

    if (mode & CRYPTO_LOCK)
        thr.P(&_mutexarray[n]);  //lock
    else
        thr.V(&_mutexarray[n]);  //unlock
}

    //Function to get the thread ID of the current thread
static unsigned long _SSLIDFunction(void)
{
    CThr thr;

    return((unsigned long)thr.GetCurrentThrID());
}

    //Dynamically creates a new mutex
static struct CRYPTO_dynlock_value *_SSLDynCreateFunction(const char *srcfile, int line)
{
    CThr thr;
    struct CRYPTO_dynlock_value *value;

    if ((value = (CRYPTO_dynlock_value *)malloc(sizeof(struct CRYPTO_dynlock_value))) == NULL)
        return(NULL);
    thr.InitializeCritSec(&(value->mutex));

    return(value);
}

    //Function used to lock/unlock the critical sections
static void _SSLDynLockFunction(int mode, struct CRYPTO_dynlock_value *l, const char *srcfile, int line)
{
    CThr thr;

    if (mode & CRYPTO_LOCK)
        thr.P(&(l->mutex)); //lock
    else
        thr.V(&(l->mutex)); //unlock
}

    //Destroy a dynamically created mutex
static void _SSLDynDestroyFunction(struct CRYPTO_dynlock_value *l, const char *srcfile, int line)
{
    CThr thr;

    thr.DestroyCritSec(&(l->mutex));
    free(l);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSSLSock::CSSLSock()
{

}

CSSLSock::~CSSLSock()
{

}

//////////////////////////////////////////////////////////////////////
// Public Methods
//////////////////////////////////////////////////////////////////////

////////////////////////////////////////
// Functions for initializing/
// uninitializing SSL.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Initialize SSL
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: This method should only be called once in the application.
//
int CSSLSock::Initialize()
{
    CThr thr;
    int i;

    apps_startup();

    InitRand();

    //////// Initialize Static Locking ////////
        //initialize the mutex array
    if ((_mutexarray = (thrSync_t *)malloc(CRYPTO_num_locks()*sizeof(thrSync_t))) == NULL)
        return(0);
    for (i = 0; i < CRYPTO_num_locks(); i++)
        thr.InitializeCritSec(&_mutexarray[i]);
        //set the callback functions
    CRYPTO_set_id_callback(_SSLIDFunction);
    CRYPTO_set_locking_callback(_SSLLockingFunction);

    //////// Initialize Dynamic Locking ////////
        //set the callback functions
    CRYPTO_set_dynlock_create_callback(_SSLDynCreateFunction);
    CRYPTO_set_dynlock_lock_callback(_SSLDynLockFunction);
    CRYPTO_set_dynlock_destroy_callback(_SSLDynDestroyFunction);

    return(1);
}

//////////////////////////////////////////////////////////////////////
// Uninitialize SSL
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: This method should only be called once in the application.
//
int CSSLSock::Uninitialize()
{
    CThr thr;
    int i;

    //////// Cleanup Static Locking ////////
    if (_mutexarray != NULL) {
        CRYPTO_set_id_callback(NULL);
        CRYPTO_set_locking_callback(NULL);
        for (i = 0; i < CRYPTO_num_locks(); i++)
            thr.DestroyCritSec(&_mutexarray[i]);
        free(_mutexarray);
        _mutexarray = NULL;
    }

    //////// Cleanup Dynamic Locking ////////
    CRYPTO_set_dynlock_create_callback(NULL);
    CRYPTO_set_dynlock_lock_callback(NULL);
    CRYPTO_set_dynlock_destroy_callback(NULL);

    return(1);
}

////////////////////////////////////////
// Functions for creating RSA public 
// and private keys.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// GenKeysFile() Generates a des3 PEM encrypted RSA private and
// public key.  The keys are then written to the files specified
// by "privfilename" and "pubfilename".
//
// [in] privpass     : Password used to encrypt the private key.
// [in] privfilename : Filename to write the private key into.
// [in] pubfilename  : Filename to write the public key into.
// [in] nbits        : Length in bits desired for the modulus.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSSLSock::GenKeysFile(const char *privpass, const char *privfilename, const char *pubfilename, int nbits /*=1024*/)
{
    FILE *fdw;
    char *privkey, *pubkey;
    int privlen, publen, nwritten = 0;

    if (privpass == NULL || privfilename == NULL || pubfilename == NULL || nbits == 0)
        return(0);

        //get the private and public keys
    if (GenKeysMem(privpass,&privkey,&privlen,&pubkey,&publen,nbits) == 0)
        return(0);

        //write the private key to the file
    if ((fdw = fopen(privfilename,"wb")) != NULL) {
        fwrite(privkey,1,privlen,fdw);
        fclose(fdw);
        nwritten++;
    }
        //write the public key to the file
    if ((fdw = fopen(pubfilename,"wb")) != NULL) {
        fwrite(pubkey,1,publen,fdw);
        fclose(fdw);
        nwritten++;
    }

        //free the private and public keys
    FreeKeyMem(privkey); FreeKeyMem(pubkey);

    if (nwritten == 2)
        return(1);  //return success if both keys were successfully written
    else
        return(0);  //return failure if one or both keys failed to be written
}

//////////////////////////////////////////////////////////////////////
// GenKeysMem() Generates a des3 PEM encrypted RSA private and
// public key.  The keys are stored in the buffers specified by
// "privkey" and "pubkey".
//
// [in] privpass     : Password used to encrypt the private key.
// [out] privkey     : Pointer to store the private key buffer to.
// [out] privkeysize : Length in bytes of the private key.
// [out] pubkey      : Pointer to store the public key buffer to.
// [out] pubkeysize  : Length in bytes of the public key.
// [in] nbits        : Length in bits desired for the modulus.
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
// NOTE: The buffers returned in "privkey" and "pubkey" must be free().
//
int CSSLSock::GenKeysMem(const char *privpass, char **privkey, int *privkeysize, char **pubkey, int *pubkeysize, int nbits /*=1024*/)
{
	RSA *rsa = NULL;
	const EVP_CIPHER *enc = NULL;
    BIO *outpriv = NULL, *outpub = NULL;
    char *tmpbuf;
    int tmplen, ngenerated = 0;

    if (privpass == NULL || privkey == NULL || privkeysize == NULL || pubkey == NULL || pubkeysize == NULL || nbits == 0)
        return(0);

        //initialize the returned parameters
    *privkey = NULL; *privkeysize = 0; *pubkey = NULL; *pubkeysize = 0;

    SSLeay_add_ssl_algorithms();    //add the SSL algorithms (Ex. des3)
    enc = EVP_des_ede3_cbc();       //set to use des3 encryption

        //generate the RSA key
    if ((rsa = RSA_generate_key(nbits,3,NULL,NULL)) == NULL)
        return(0);

        //generate the private key
    if ((outpriv = BIO_new(BIO_s_mem())) != NULL) {
        if (PEM_write_bio_RSAPrivateKey(outpriv,rsa,enc,(unsigned char *)privpass,strlen(privpass),NULL,NULL)) {
        	tmplen = BIO_get_mem_data(outpriv,&tmpbuf);
            if ((*privkey = (char *)calloc(tmplen+1,1)) != NULL) {
                memcpy(*privkey,tmpbuf,tmplen);
                *privkeysize = tmplen;
                ngenerated++;
            }
        }
    }
    BIO_free_all(outpriv);

        //generate the public key
    if ((outpub = BIO_new(BIO_s_mem())) != NULL) {
        if (PEM_write_bio_RSA_PUBKEY(outpub,rsa)) {
        	tmplen = BIO_get_mem_data(outpub,&tmpbuf);
            if ((*pubkey = (char *)calloc(tmplen+1,1)) != NULL) {
                memcpy(*pubkey,tmpbuf,tmplen);
                *pubkeysize = tmplen;
                ngenerated++;
            }
        }
    }
    BIO_free_all(outpub);

    RSA_free(rsa);  //free the RSA key

    if (ngenerated == 2)
        return(1);  //return success if both keys were successfully generated
    else
        return(0);  //return failure if one or both keys failed to be generated
}

//////////////////////////////////////////////////////////////////////
// LoadPrivKeyMem() Loads the private key from the specified file 
// into a buffer in memory.
//
// [in] privfilename : Filename to read the private key from.
// [out] privkeysize : Length in bytes of the private key.
//
// Return : On success the buffer containing the contents of the
//          private key file is returned.  On failure NULL is
//          returned.
//
char *CSSLSock::LoadPrivKeyMem(const char *privfilename, int *privkeysize)
{

    return(LoadFileToMem(privfilename,privkeysize));
}

//////////////////////////////////////////////////////////////////////
// LoadPubKeyMem() Loads the public key from the specified file 
// into a buffer in memory.
//
// [in] pubfilename : Filename to read the public key from.
// [out] pubkeysize : Length in bytes of the public key.
//
// Return : On success the buffer containing the contents of the
//          public key file is returned.  On failure NULL is returned.
//
char *CSSLSock::LoadPubKeyMem(const char *pubfilename, int *pubkeysize)
{

    return(LoadFileToMem(pubfilename,pubkeysize));
}

//////////////////////////////////////////////////////////////////////
// FreeKeyMem() Frees the private and public key buffers returned by
// GenKeysMem(), LoadPrivKeyMem(), or LoadPubKeyMem(). 
//
// [in] keybuf : Buffer containing the key to be free().
//
// Return : VOID
//
void CSSLSock::FreeKeyMem(char *keybuf)
{

    if (keybuf != NULL)
        free(keybuf);
}

////////////////////////////////////////
// Functions for creating X509  
// certificates.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// GenX509File() Generates a des3 PEM encrypted X509 certificate.
// The certificate is then written to the file specified by
// "certfilename".
//
// [in] certfilename : Filename to write the certificate into.
// [in] privkeybuf   : Buffer containing the PEM encrypted private key.
// [in] privkeysize  : Size in bytes of the PEM private key.
// [in] privpass     : Password to the private key.
// [in] certinfo     : Certificate information (Ex, country, state, etc).
//
// Return : On success 1 is returned.  On failure 0 is returned.
//
int CSSLSock::GenX509File(const char *certfilename, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo)
{
    FILE *fdw;
    char *certbuf;
    int len, retval = 0;

    if (certfilename == NULL || privkeybuf == NULL || privpass == NULL || certinfo == NULL)
        return(0);

    if ((fdw = fopen(certfilename,"wb")) == NULL)
        return(0);

    if ((certbuf = GenX509Mem(&len,privkeybuf,privkeysize,privpass,certinfo)) != NULL) {
        fwrite(certbuf,1,len,fdw);
        FreeX509Mem(certbuf);
        retval = 1;
    }

    fclose(fdw);

    return(retval);
}

//////////////////////////////////////////////////////////////////////
// GenX509Mem() Generates a des3 PEM encrypted X509 certificate.
// The certificate is returned on success.
//
// [out] certsize   : Length in bytes of the returned certificate.
// [in] privkeybuf  : Buffer containing the PEM encrypted private key.
// [in] privkeysize : Size in bytes of the PEM private key.
// [in] privpass    : Password to the private key.
// [in] certinfo    : Certificate information (Ex, country, state, etc).
//
// Return : On success the buffer containing the X509 certificate
//          is returned.  On failure NULL is returned.
//
char *CSSLSock::GenX509Mem(int *certsize, const char *privkeybuf, int privkeysize, const char *privpass, sslsockCertInfo_t *certinfo)
{
    X509 *x509 = NULL;          //x509 certificate structure
    X509_REQ *req = NULL;       //x509 certificate request structure
    struct X509_name_st *reqsubj;       //pointer to the subject part of the cert request
    BIO *key = NULL, *outcert = NULL;   //I/O pointers
    EVP_PKEY *privkey = NULL;   //private key
    EVP_PKEY *tmppkey;          //temporary copy of the private key
    X509V3_CTX ext_ctx;         //X509 V3 context struct
    char *certbuf = NULL;       //certificate string output
    char *tmpbuf;
    int tmplen, days = 365;

    if (certsize == NULL || privkeybuf == NULL || privpass == NULL || certinfo == NULL)
        return(NULL);

    if (CheckCertInfo(certinfo) == 0)
        return(NULL);   //the certificate information is not valid

    SSLeay_add_ssl_algorithms();    //add the SSL algorithms (Ex. des3)

        //load the private key from the file
    if ((key = BIO_new(BIO_s_mem())) != NULL) {
        if (BIO_write(key,privkeybuf,privkeysize) > 0)
            privkey = PEM_read_bio_PrivateKey(key,NULL,NULL,(void *)privpass);
        BIO_free(key);
    }
    if (privkey == NULL)
        return(NULL);   //unable to read the private key

        //allocate a new certificate request structure
    if ((req = X509_REQ_new()) == NULL) goto END;
        //set the version for the certificate request (version 1)
    if (!X509_REQ_set_version(req,0L)) goto END;
        //set the req_distinguished_name settings
    if ((reqsubj = X509_REQ_get_subject_name(req)) == NULL) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_countryName,MBSTRING_ASC,(unsigned char *)(certinfo->country),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_stateOrProvinceName,MBSTRING_ASC,(unsigned char *)(certinfo->state),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_localityName,MBSTRING_ASC,(unsigned char *)(certinfo->locality),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_organizationName,MBSTRING_ASC,(unsigned char *)(certinfo->organization),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_organizationalUnitName,MBSTRING_ASC,(unsigned char *)(certinfo->orgunit),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_commonName,MBSTRING_ASC,(unsigned char *)(certinfo->commonname),-1,-1,0)) goto END;
    if (!X509_NAME_add_entry_by_NID(reqsubj,NID_pkcs9_emailAddress,MBSTRING_ASC,(unsigned char *)(certinfo->emailaddr),-1,-1,0)) goto END;
        //set the req_attributes settings
    if(!X509_REQ_add1_attr_by_NID(req,NID_pkcs9_challengePassword,MBSTRING_ASC,(unsigned char *)privpass,-1)) goto END;
    //if(!X509_REQ_add1_attr_by_NID(req,NID_pkcs9_unstructuredName,MBSTRING_ASC,(unsigned char *)"NoOptCompany",-1)) goto END;
        //set the key for the request
    if (!X509_REQ_set_pubkey(req,privkey)) goto END;

        //allocate a new X509 certificate
    if ((x509 = X509_new()) == NULL) goto END;
        //set the x509 certificate parameters
    if (!X509_set_version(x509,2)) goto END;  //set to version v3
    if (!ASN1_INTEGER_set(X509_get_serialNumber(x509),0L)) goto END;
    if (!X509_set_issuer_name(x509,X509_REQ_get_subject_name(req))) goto END;
    if (!X509_gmtime_adj(X509_get_notBefore(x509),0)) goto END;
    if (!X509_gmtime_adj(X509_get_notAfter(x509),(long)60*60*24*days)) goto END;
    if (!X509_set_subject_name(x509,X509_REQ_get_subject_name(req))) goto END;
    tmppkey = X509_REQ_get_pubkey(req);
    if (!tmppkey || !X509_set_pubkey(x509,tmppkey)) goto END;
    EVP_PKEY_free(tmppkey);
        //Set up V3 context struct
    X509V3_set_ctx(&ext_ctx,x509,x509,NULL,NULL,0);

        //sign the x509 certificate using the md5 digest
    if (!X509_sign(x509,privkey,EVP_md5())) goto END;

        //write the x509 certificate to the buffer
    if ((outcert = BIO_new(BIO_s_mem())) != NULL) {
        if (PEM_write_bio_X509(outcert,x509)) {
        	tmplen = BIO_get_mem_data(outcert,&tmpbuf);
            if ((certbuf = (char *)calloc(tmplen+1,1)) != NULL) {
                memcpy(certbuf,tmpbuf,tmplen);
                *certsize = tmplen;
            }
        }
    }
    BIO_free_all(outcert);

END:
    if (x509 != NULL) X509_free(x509);      //free the X509 certificate
    if (req != NULL) X509_REQ_free(req);    //free the certificate request struct
    if (privkey != NULL) EVP_PKEY_free(privkey);    //free the private key

    return(certbuf);
}

//////////////////////////////////////////////////////////////////////
// LoadX509Mem() Loads the x509 certificate from the specified file 
// into a buffer in memory.
//
// [in] certfilename : Filename to load the certificate from.
// [out] certsize    : Length in bytes of the returned certificate.
//
// Return : On success the buffer containing the X509 certificate
//          is returned.  On failure NULL is returned.
//
char *CSSLSock::LoadX509Mem(const char *certfilename, int *certsize)
{

    return(LoadFileToMem(certfilename,certsize));
}

//////////////////////////////////////////////////////////////////////
// FreeX509Mem() Frees the x509 certificate buffer returned by
// GenX509Mem() or LoadX509Mem(). 
//
// [in] certbuf : Certificate buffer to be free().
//
// Return : VOID
//
void CSSLSock::FreeX509Mem(char *certbuf)
{

    if (certbuf != NULL)
        free(certbuf);
}

////////////////////////////////////////
// Functions for opening and closing
// SSL connections.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// SSLServerNeg() Performs the server side SSL negotiation given an
// active socket descriptor.
//
// [in] sd          : Socket descriptor to use for the SSL connection.
// [in] privkeybuf  : Buffer containing the PEM encrypted private key.
// [in] privkeysize : Size in bytes of the PEM private key.
// [in] privpass    : Password to the private key.
// [in] certbuf     : Buffer containing the PEM x509 certificate.
// [in] certsize    : Size in bytes of the PEM x509 certificate.
// [out] errcode    : Code that identifies any errors (=0 on success).
//
// Return : On success the information for the SSL connection is
//          returned.  On failure NULL is returned.
//
sslsock_t *CSSLSock::SSLServerNeg(SOCKET sd, const char *privkeybuf, int privkeysize, const char *privpass, const char *certbuf, int certsize, int *errcode)
{
	BIO *bio = NULL;            //I/O pointer
    EVP_PKEY *privkey = NULL;   //private key
    X509 *x509 = NULL;          //x509 certificate structure
    sslsock_t *sslinfo;

    if (sd == SSLSOCK_INVALID || privkeybuf == NULL || privpass == NULL || certbuf == NULL || errcode == NULL)
        return(NULL);

        //allocate the SSL information structure (initialize to all 0's)
    if ((sslinfo = (sslsock_t *)calloc(sizeof(sslsock_t),1)) == NULL) {
        *errcode = 1;   //memory allocation error
        return(NULL);
    }

    SSLeay_add_ssl_algorithms();    //add the SSL algorithms (Ex. des3)

        //set the SSL server method
    if ((sslinfo->meth = SSLv23_server_method()) == NULL) {
        free(sslinfo);
        *errcode = 2;   //unable to set the SSL method
        return(NULL);
    }

        //create a new SSL context
    if ((sslinfo->ctx = SSL_CTX_new(sslinfo->meth)) == NULL) {
        free(sslinfo);
        *errcode = 3;   //unable to create a new SSL context
        return(NULL);
    }

        //Set the private key for the SSL context
    if ((bio = BIO_new(BIO_s_mem())) != NULL) {
        if (BIO_write(bio,privkeybuf,privkeysize) > 0)
            privkey = PEM_read_bio_PrivateKey(bio,NULL,NULL,(void *)privpass);
        BIO_free(bio);
    }
    if (privkey == NULL) {
        SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 4;   //unable to read the private key
        return(NULL);
    }
    if (SSL_CTX_use_PrivateKey(sslinfo->ctx,privkey) <= 0) {
        EVP_PKEY_free(privkey); SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 5;   //unable to use the private key
        return(NULL);
    }
    EVP_PKEY_free(privkey);

        //Set the certificate for the SSL context
    if ((bio = BIO_new(BIO_s_mem())) != NULL) {
        if (BIO_write(bio,certbuf,certsize) > 0)
            x509 = PEM_read_bio_X509(bio,NULL,NULL,NULL);
        BIO_free(bio);
    }
    if (x509 == NULL) {
        SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 6;   //unable to read the certificate
        return(NULL);
    }
    if (SSL_CTX_use_certificate(sslinfo->ctx,x509) <= 0) {
        X509_free(x509); SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 7;   //unable to use the certificate
        return(NULL);
    }
    X509_free(x509);

        //verify that the private key matches the certificate public key
    if (!SSL_CTX_check_private_key(sslinfo->ctx)) {
        SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 8;   //private key does not match the certificate public key
        return(NULL);
    }

        //try to set the connection to auto-negotiate without bothering
        //the calling application
    SSL_CTX_set_mode(sslinfo->ctx,SSL_MODE_AUTO_RETRY);

        //do server side SSL.
    if ((sslinfo->ssl = SSL_new(sslinfo->ctx)) == NULL) {
        SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 9;   //unable to create a new SSL object
        return(NULL);
    }
    SSL_set_fd(sslinfo->ssl,sd); //set the socket descriptor to use for the SSL connection
    if (SSL_accept(sslinfo->ssl) == -1) {
        SSL_free(sslinfo->ssl); SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 10;  //unable to accept the SSL connection
        return(NULL);
    }

    *errcode = 0;   //success
    return(sslinfo);
}

//////////////////////////////////////////////////////////////////////
// SSLServerNeg() Performs the server side SSL negotiation given an
// active socket descriptor and a pre-existing SSL server context.
//
// [in] sd       : Socket descriptor to use for the SSL connection.
// [in] sslinfo  : The pre-existing SSL information (context info).
// [out] errcode : Code that identifies any errors (=0 on success).
//
// Return : On success the information for the SSL connection is
//          returned.  On failure NULL is returned.
//
sslsock_t *CSSLSock::SSLServerNeg(SOCKET sd, sslsock_t *sslinfo, int *errcode)
{
    sslsock_t *sslinfoout;

    if (sd == SSLSOCK_INVALID || sslinfo == NULL || errcode == NULL) {
        *errcode = 11;  //invalid SSL information
        return(NULL);
    }

    SSLeay_add_ssl_algorithms();    //add the SSL algorithms (Ex. des3)

    if (sslinfo->ctx == NULL) {
        *errcode = 12;  //invalid SSL context
        return(NULL);
    }

        //allocate the SSL information structure (initialize to all 0's)
    if ((sslinfoout = (sslsock_t *)calloc(sizeof(sslsock_t),1)) == NULL) {
        *errcode = 1;   //memory allocation error
        return(NULL);
    }

        //do server side SSL (reusing existing context).
    if ((sslinfoout->ssl = SSL_new(sslinfo->ctx)) == NULL) {
        free(sslinfoout);
        *errcode = 9;   //unable to create a new SSL object
        return(NULL);
    }
    SSL_set_fd(sslinfoout->ssl,sd); //set the socket descriptor to use for the SSL connection
    if (SSL_accept(sslinfoout->ssl) == -1) {
        SSL_free(sslinfoout->ssl); free(sslinfoout);
        *errcode = 10;  //unable to accept the SSL connection
        return(NULL);
    }

    *errcode = 0;

    return(sslinfoout);
}

//////////////////////////////////////////////////////////////////////
// SSLClientNeg() Performs the client side SSL negotiation given an
// active socket descriptor.
//
// [in] sd       : Socket descriptor to use for the SSL connection.
// [out] errcode : Code that identifies any errors (=0 on success).
// [in] sslver   : SSL version (currently only 2 and 3 are supported).
//
// Return : On success the information for the SSL connection is
//          returned.  On failure NULL is returned.
//
sslsock_t *CSSLSock::SSLClientNeg(SOCKET sd, int *errcode, int sslver /*=2*/)
{
    sslsock_t *sslinfo;

    if (sd == SSLSOCK_INVALID || errcode == NULL)
        return(NULL);

        //allocate the SSL information structure
    if ((sslinfo = (sslsock_t *)calloc(sizeof(sslsock_t),1)) == NULL) {
        *errcode = 1;   //memory allocation error
        return(NULL);
    }

    SSLeay_add_ssl_algorithms();    //add the SSL algorithms (Ex. des3)

        //set the SSL client method
    if (sslver <= 2)
        sslinfo->meth = SSLv2_client_method();
    else
        sslinfo->meth = SSLv3_client_method();
    if (sslinfo->meth == NULL) {
        free(sslinfo);
        *errcode = 2;   //unable to set the SSL method
        return(NULL);
    }

        //create a new SSL context
    if ((sslinfo->ctx = SSL_CTX_new(sslinfo->meth)) == NULL) {
        free(sslinfo);
        *errcode = 3;   //unable to create a new SSL context
        return(NULL);
    }

        //try to set the connection to auto-negotiate without bothering
        //the calling application
    SSL_CTX_set_mode(sslinfo->ctx,SSL_MODE_AUTO_RETRY);

        //do client side SSL.
    if ((sslinfo->ssl = SSL_new(sslinfo->ctx)) == NULL) {
        SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 4;   //unable to create a new SSL object
        return(NULL);
    }
    SSL_set_fd(sslinfo->ssl,sd); //set the socket descriptor to use for the SSL connection
    if (SSL_connect(sslinfo->ssl) == -1) {
        SSL_free(sslinfo->ssl); SSL_CTX_free(sslinfo->ctx); free(sslinfo);
        *errcode = 5;   //unable to create the SSL connection
        return(NULL);
    }

    *errcode = 0;   //success
    return(sslinfo);
}

//////////////////////////////////////////////////////////////////////
// SSLClose() Shuts down the SSL connection and frees the connection
// information.  This function should be called after SSLServerNeg()
// or SSLClientNeg().
//
// [in] sslinfo : The information for the SSL connection.
//
// Return : VOID
//
void CSSLSock::SSLClose(sslsock_t *sslinfo)
{

    if (sslinfo == NULL)
        return;

    if (sslinfo->ssl != NULL) {
        SSL_shutdown(sslinfo->ssl); //send SSL/TLS close_notify
        SSL_free(sslinfo->ssl);
    }
    if (sslinfo->ctx != NULL)
        SSL_CTX_free(sslinfo->ctx);
    sslinfo->ssl = NULL;
    sslinfo->ctx = NULL;
}


//////////////////////////////////////////////////////////////////////
// FreeSSLInfo() Frees the SSL connection information returned by
// SSLServerNeg() or SSLClientNeg().
//
// [in] sslinfo : The information for the SSL connection.
//
// Return : VOID
//
void CSSLSock::FreeSSLInfo(sslsock_t *sslinfo)
{

    if (sslinfo != NULL)
        free(sslinfo);
}

////////////////////////////////////////
// Functions for transfering data.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// SSLRecvN() Read "nbytes" bytes from the SSL connection.
//
// [in] sslinfo : The information for the SSL connection.
// [out] ptr    : Buffer to put the received data into.
// [in] nbytes  : Max size of the "ptr" buffer.
//
// Return : On success the number of bytes read is returned.  On
//          failure -1 is returned.
//
int CSSLSock::SSLRecvN(sslsock_t *sslinfo, char *ptr, int nbytes)
{
    int nleft, nread;

    if (sslinfo == NULL || ptr == NULL || nbytes == 0)
        return(-1);
    *ptr = '\0';    //initialize to an empty string

    if (sslinfo->ssl == NULL)
        return(-1);

    nleft = nbytes;
    while (nleft > 0)  {
        nread = SSL_read(sslinfo->ssl,ptr,nleft);
        if (nread < 0)  {
            return(nread);    // error, return < 0
        } else if (nread == 0) {
            break;    // EOF
        }
        nleft -= nread;
        ptr += nread;
    }

    return(nbytes - nleft);    // return >= 0
}

//////////////////////////////////////////////////////////////////////
// SSLRecvLn() Reads a line of data from a SSL connection -- reads 
// until a '\n' is found.  Same syntax as fgets.
//
// [in] sslinfo : The information for the SSL connection.
// [out] ptr    : Buffer to put the received data into.
// [in] nmax    : Max size of the "ptr" buffer.
//
// Return : On success the number of bytes read is returned.  On
//          failure -1 is returned.
//
int CSSLSock::SSLRecvLn(sslsock_t *sslinfo, char *ptr, int nmax)
{
    int i, rc;
    char c;

    if (sslinfo == NULL || ptr == NULL || nmax == 0)
        return(-1);
    *ptr = '\0';    //initialize to an empty string

    if (sslinfo->ssl == NULL)
        return(-1);

    for (i = 0;  i < (nmax - 1);  i++)  {
        if ((rc = SSL_read(sslinfo->ssl,&c,1)) == 1)  {
            ptr[i] = c;
            if (c == '\n')
                break;
        } else if (rc == 0) {
            if (i == 0)
                return(0);  // EOF no data read
            else
                break;      // EOF some data read
        } else {
            return(-1);     // error
        }
    }

    if (i >= (nmax - 1))
        ptr[i] = '\0';
    else
        ptr[i+1] = '\0';

    return(i);
}

//////////////////////////////////////////////////////////////////////
// SSLSendN() Write "nbytes" bytes to a SSL connection.
//
// [in] sslinfo : The information for the SSL connection.
// [in] ptr     : Buffer containing the data to write.
// [in] nbytes  : Number of bytes to write.
//
// Return : On success the number of bytes written is returned.  On
//          failure -1 is returned.
//
int CSSLSock::SSLSendN(sslsock_t *sslinfo, const char *ptr, int nbytes)
{
    int nleft;
    int nwritten;
    char *offsetptr;

    if (sslinfo == NULL || ptr == NULL)
        return(-1);

    if (sslinfo->ssl == NULL)
        return(-1);

    offsetptr = (char *)ptr;
    nleft = nbytes;
    while (nleft > 0) {
        nwritten = SSL_write(sslinfo->ssl,offsetptr,nleft);
        if (nwritten <= 0)
            return(nwritten);    // error
        nleft -= nwritten;
        offsetptr += nwritten;
    }

    return(nbytes - nleft);
}

//////////////////////////////////////////////////////////////////////
// SSLSendLn() Write until '\n' is found (inserts a '\n' if
// necessary).
//
// [in] sslinfo : The information for the SSL connection.
// [in] ptr     : Buffer containing the data to write.
//
// Return : On success the number of bytes written is returned.  On
//          failure -1 is returned.
//
// NOTE: "ptr" must be a NULL terminated string.
//
int CSSLSock::SSLSendLn(sslsock_t *sslinfo, const char *ptr)
{
    char *offsetptr, *line;
    int retval;

    if (sslinfo == NULL || ptr == NULL)
        return(-1);

    if (sslinfo->ssl == NULL)
        return(-1);

    if ((line = (char *)calloc(strlen(ptr)+2,1)) == NULL)
        return(0);
    strcpy(line,ptr);

        //make sure the line ends with '\n'
    if(*(line+strlen(line)-1) != '\n') {
        *(line+strlen(line)+1) = '\0';
        *(line+strlen(line)) = '\n';
    }            

        //terminate the line after any '\n'
    offsetptr = strchr(line,'\n');
    *(offsetptr+1) = '\0';

    retval = SSLSendN(sslinfo,line,strlen(line));

    free(line);
    return(retval);
}

////////////////////////////////////////
// Functions to get SSL state.
////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// GetCipherName() This function returns a buffer containing the SSL
// cipher string.
//
// [in] sslinfo : The information for the SSL connection.
//
// Return : On success the SSL cipher string is returned.  On
//          failure NULL is returned.
//
char *CSSLSock::GetCipherName(sslsock_t *sslinfo)
{
    char *ciphername = NULL, *ptr;

    if (sslinfo == NULL)
        return(NULL);

    if (sslinfo->ssl == NULL)
        return(NULL);

    if ((ptr = (char *)SSL_get_cipher(sslinfo->ssl)) == NULL)
        return(NULL);

    if ((ciphername = (char *)calloc(strlen(ptr)+1,1)) != NULL)
        strcpy(ciphername,ptr);

    return(ciphername);
}

//////////////////////////////////////////////////////////////////////
// FreeCipherName() Frees the cipher buffer returned by 
// GetCipherName(). 
//
// [in] ciphername : Cipher buffer to be free().
//
// Return : VOID
//
void CSSLSock::FreeCipherName(char *ciphername)
{

    if (ciphername != NULL)
        free(ciphername);
}

//////////////////////////////////////////////////////////////////////
// VerifyCert() Verifies the peer's certificate.
//
// [in] sslinfo : The information for the SSL connection.
//
// Return : X509_V_ERR_* code returned by SSL_get_verify_result().
//
long CSSLSock::VerifyCert(sslsock_t *sslinfo)
{
    X509 *pX509;
    long retcode = 0;

    if (sslinfo == NULL)
        return(0);

    if (sslinfo->ssl == NULL)
        return(0);

    if ((pX509 = SSL_get_peer_certificate(sslinfo->ssl)) != NULL) {
        retcode = SSL_get_verify_result(sslinfo->ssl);
        X509_free(pX509);
    }

    return(retcode);
}


//////////////////////////////////////////////////////////////////////
// Private Methods
//////////////////////////////////////////////////////////////////////

    //make sure the certificate information is valid
int CSSLSock::CheckCertInfo(sslsockCertInfo_t *certinfo)
{
    int retval = 1;

        //make sure all the strings are NULL terminated
    certinfo->commonname[sizeof(certinfo->commonname)-1] = '\0';
    certinfo->country[sizeof(certinfo->country)-1] = '\0';
    certinfo->emailaddr[sizeof(certinfo->emailaddr)-1] = '\0';
    certinfo->locality[sizeof(certinfo->locality)-1] = '\0';
    certinfo->organization[sizeof(certinfo->organization)-1] = '\0';
    certinfo->orgunit[sizeof(certinfo->orgunit)-1] = '\0';
    certinfo->state[sizeof(certinfo->state)-1] = '\0';

    return(retval);
}

    //loads the file specified by "filepath" into a buffer in memory.
    //"filesize" will be set to the size of the loaded file.
char *CSSLSock::LoadFileToMem(const char *filepath, int *filesize)
{
    FILE *fdr;
    char *filebuffer;
    int tmpfilesize;

    if ((tmpfilesize = GetFileSize(filepath)) == 0)
        return(NULL);

    if ((filebuffer = (char *)calloc(tmpfilesize+1,1)) == NULL)
        return(NULL);

    if ((fdr = fopen(filepath,"rb")) == NULL) {
        free(filebuffer);
        return(NULL);
    }

    fread(filebuffer,1,tmpfilesize,fdr);
    *filesize = tmpfilesize;

    fclose(fdr);

    return(filebuffer);
}

    //get the size in bytes of the specified file
long CSSLSock::GetFileSize(const char *filepath)
{
    int retval;

    if (filepath == NULL)
        return(0);

    #ifdef WIN32
      struct _stat filestatus;
      retval = _stat(filepath,&filestatus);
    #else
      struct stat filestatus;
      retval = stat(filepath,&filestatus);
    #endif

    if (retval != 0)
        return(0);  //unable to get the stats for the file

    return(filestatus.st_size); //return the size of the file
}

void CSSLSock::InitRand()
{

    #ifndef WIN32
      RAND_load_file("/dev/urandom",1024);
    #endif
}


#endif  //ENABLE_SSL
