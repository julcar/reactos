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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>

#include "core/Sock.h"
#include "core/Thr.h"
#include "core/Timer.h"
#include "core/CmdLine.h"
#include "core/StrUtils.h"
#include "core/FSUtils.h"
//#include "core/Crypto.h"
#include "IndiFtps.h"
#include "IndiFileUtils.h"
#include "IndiSiteInfo.h"

#define _MAINLOOPFREQ 2     //number of seconds the main loop runs at
#define _USERLOOPFREQ 1     //the freq of the loop in the user thread

#define _PROGNAME "IndiFTPD"
#define _PROGVERSION "1.0.9_beta"

#define _LOGINSTR "Connected to IndiFTPD"


    //structure used to pass information to the listening threads
typedef struct {
    SOCKET sd;
    CIndiSiteInfo *psiteinfo;
} _ListenInfo_t;
    //functions used to listen for FTP connections in a separate thread
#ifdef WIN32    //WINDOWS must return "void" to run as a new thread
  static void _listenthread(void *vplisteninfo);    //listen for a normal FTP connection
  static void _listensslthread(void *vplisteninfo); //listen for an implicit SSL FTP connection
#else           //UNIX must return "void *" to run as a new thread
  static void *_listenthread(void *vplisteninfo);
  static void *_listensslthread(void *vplisteninfo);
#endif
    //flags used to check if the listening threads are running
static int _flaglisten = 0;     //0 = _listenthread is not active
static int _flaglistenssl = 0;  //0 = _listensslthread is not active

    //displays the usage screen
static void _usage(char type);
    //initialize the FTP server (based on the command line inputs)
static int _initserver(CIndiSiteInfo *psiteinfo, int argc, char **argv, char *bindip, char *port, char *sslport, int *loglevel);
    //standard signal handler used to catch signals such as SIGINT (Ctrl-C)
static void _sighandler(int sig);
    //Used to check if the user entered a transfer command.
static int _iscommandxfer(char *command);

    //the main function that handles the client connections
#ifdef WIN32
  static void _connect(void *vpftps);   //WINDOWS must return "void" to run as a new thread
#else
  static void *_connect(void *vpftps);  //UNIX must return "void *" to run as a new thread
#endif

    //Used to signal the program to refresh (this is set in _sighandler())
static int _flaghup = 0;

    //stores the number of current client connections
static int _nconnections = 0;


int main(int argc, char **argv)
{
    SOCKET sd, sslsd = SOCK_INVALID;
    CIndiSiteInfo *psiteinfo;
    CSock sock;
    CSSLSock sslsock;
    CThr thr;
    CTimer timer;
    char bindip[SOCK_IPADDRLEN] = ""; //IP address the server should bind to
    char port[SOCK_PORTLEN] = "21";   //port the FTP server should run on
    char sslport[SOCK_PORTLEN] = "0"; //port to use for implicit ssl connections (0 = don't listen for implicit SSL)
    int loglevel = INDISITEINFO_LOGLEVELDISPLAY;   //default to display on the screen only
    _ListenInfo_t *listeninfo;  //used to pass information to the listening threads
    int retval;

        //Set the signal handlers
    signal(SIGINT,_sighandler);     //Ctrl-C
    signal(SIGTERM,_sighandler);
    signal(SIGABRT,_sighandler);
    #ifdef WIN32
      signal(SIGBREAK,_sighandler); //Ctrl-Break
    #else
      signal(SIGQUIT,_sighandler);
      signal(SIGKILL,_sighandler);
      signal(SIGSTOP,_sighandler);
      signal(SIGALRM,_sighandler);
      signal(SIGHUP,_sighandler);
      signal(SIGPIPE,_sighandler);
    #endif

    if (argc >= 2) {
        if (strncmp(argv[1],"-h",2) == 0) {
            _usage(*(argv[1]+2));
            return(0);
        } else if (strncmp(argv[1],"-v",2) == 0 || strncmp(argv[1],"-V",2) == 0) {
            printf("%s version %s (%s)\n",_PROGNAME,_PROGVERSION,FTPS_COREVERSION);
            return(0);
        }
    }

        //initialize the network sockets
    sock.Initialize();
        //initialize SSL
    sslsock.Initialize();

        //Create the class to store/access the site information
    if ((psiteinfo = new CIndiSiteInfo(_PROGNAME,_PROGVERSION,FTPS_COREVERSION,"")) == NULL) {
        printf("ERROR: unable to allocate site information structure\n");
        return(1);
    }

        //Always start the program log in display only logging mode.
    psiteinfo->SetProgLoggingLevel(INDISITEINFO_LOGLEVELDISPLAY);

        //initialize the FTP server
    retval = _initserver(psiteinfo,argc,argv,bindip,port,sslport,&loglevel);
    if (retval == 0 || retval == 2) {
        delete psiteinfo;
        return((retval==2)?0:1);
    }

        //Set the logging level for the program log file
    psiteinfo->SetProgLoggingLevel(loglevel);
        //Set the logging level for the FTP access log file
    psiteinfo->SetAccessLoggingLevel(loglevel);
    
        //create the primary listening socket for the server
        //this is the socket that listens on the FTP control port
    psiteinfo->WriteToProgLog("MAIN","Use \"indiftpd -h\" for help.  Exit using \"Ctrl-C\".");
    if (*bindip == '\0') {
        psiteinfo->WriteToProgLog("MAIN","Starting server on port %s...",port);
        sd = sock.OpenServer(port);
    } else {
        psiteinfo->WriteToProgLog("MAIN","Starting server on %s:%s...",bindip,port);
        sd = sock.OpenServer(port,bindip);
    }
    if (sd == SOCK_INVALID) {
        psiteinfo->WriteToProgLog("MAIN","ERROR: Unable to start the server (sd = %d).",sd);
        delete psiteinfo;
        return(1);
    }
        //Start the main listening thread
    if ((listeninfo = (_ListenInfo_t *)malloc(sizeof(_ListenInfo_t))) != NULL) {
        listeninfo->sd = sd; listeninfo->psiteinfo = psiteinfo;
        if (thr.Create(_listenthread,(void *)listeninfo) == 0)
            psiteinfo->WriteToProgLog("MAIN","WARNING: unable to create the listening thread for port %s.",port);
        else
            psiteinfo->WriteToProgLog("MAIN","Listening for FTP connections on port %s.",port);
    }
    
        //create the listening socket for the implicit SSL connection
    if (*sslport != '0') {  //if an implicit SSL port was specified
        if (*bindip == '\0')
            sslsd = sock.OpenServer(sslport);
        else
            sslsd = sock.OpenServer(sslport,bindip);
        if (sslsd != SOCK_INVALID) {
                //Start the implicit SSL listening thread
            if ((listeninfo = (_ListenInfo_t *)malloc(sizeof(_ListenInfo_t))) != NULL) {
                listeninfo->sd = sslsd; listeninfo->psiteinfo = psiteinfo;
                if (thr.Create(_listensslthread,(void *)listeninfo) == 0)
                    psiteinfo->WriteToProgLog("MAIN","WARNING: unable to create the SSL listening thread for port %s.",sslport);
                else
                    psiteinfo->WriteToProgLog("MAIN","Listening for implicit SSL connections on port %s.",sslport);
            }
        } else {
            psiteinfo->WriteToProgLog("MAIN","WARNING: unable to listen for SSL connections on port %s.",sslport);
        }
    }

    while (siteinfoFlagQuit == 0) {
            //check if a refresh is necessary (if SIGHUP was caught)
        if (_flaghup != 0) {
            psiteinfo->WriteToProgLog("MAIN","SIGHUP caught: refreshing server");
            _initserver(psiteinfo,argc,argv,bindip,port,sslport,&loglevel);
            psiteinfo->WriteToProgLog("MAIN","Done refreshing server");
            _flaghup = 0;   //reset the HUP flag
        }
            //sleep for _MAINLOOPFREQ seconds
        timer.Sleep(_MAINLOOPFREQ * 1000);
    }

        //wait for the SSL listening thread to exit
    if (*sslport != '0') {
        while (_flaglistenssl != 0) timer.Sleep(100);
        psiteinfo->WriteToProgLog("MAIN","SSL server stopped on port %s",sslport);
    }
        //wait for the main listening thread to exit
    while (_flaglisten != 0) timer.Sleep(100);
    psiteinfo->WriteToProgLog("MAIN","Server stopped on port %s",port);

        //close the listening socket
    sock.Close(sd);
    if (sslsd != SOCK_INVALID) sock.Close(sslsd);

        //wait for all the client connection threads to exit
    while (_nconnections > 0) timer.Sleep(100);
    psiteinfo->WriteToProgLog("MAIN","Server shutting down.");
    delete psiteinfo;

        //uninitialize the network sockets
    sock.Uninitialize();
        //uninitialize SSL
    sslsock.Uninitialize();

    return(0);  //return success
}

    //listen for a normal FTP connection (specified by the -p option)
#ifdef WIN32    //WINDOWS must return "void" to run as a new thread
  static void _listenthread(void *vplisteninfo)
#else           //UNIX must return "void *" to run as a new thread
  static void *_listenthread(void *vplisteninfo)
#endif
{
    _ListenInfo_t *listeninfo = (_ListenInfo_t *)(vplisteninfo);
    CSock sock;
    CThr thr;
    SOCKET clientsd;
    CIndiFtps *pftps;

    _flaglisten = 1;    //indicate the listen thread is active

    while (siteinfoFlagQuit == 0) {
            //loop every _MAINLOOPFREQ sec
        if (sock.CheckStatus(listeninfo->sd,_MAINLOOPFREQ) > 0) {
            if ((clientsd = sock.Accept(listeninfo->sd)) != SOCK_INVALID) {
                    //Create the FTP server class to handle the client connection
                if ((pftps = new CIndiFtps(clientsd,listeninfo->psiteinfo)) != NULL) {
                    listeninfo->psiteinfo->WriteToProgLog("MAIN","Connection established from %s:%s -> %s (%u).",
                                    pftps->GetClientIP(),pftps->GetClientPort(),pftps->GetClientName(),clientsd);
                    listeninfo->psiteinfo->WriteToAccessLog(pftps->GetServerIP(),pftps->GetServerPort(),pftps->GetClientIP(),
                                                            pftps->GetClientPort(),pftps->GetLogin(),"CONNECT","","","");
                        //start a new thread to handle the client connection
                    if (thr.Create(_connect,(void *)pftps) == 0) {
                        listeninfo->psiteinfo->WriteToProgLog("MAIN","WARNING: unable to create a new client thread.");
                        delete pftps;
                    }
                }
            }
        }
    }

    free(listeninfo);

    _flaglisten = 0;    //indicate the listen thread is inactive
    
    #ifndef WIN32
      return(NULL);     //UNIX must return a value
    #endif
}

    //listen for an implicit SSL FTP connection (specified by the -i option)
#ifdef WIN32    //WINDOWS must return "void" to run as a new thread
  static void _listensslthread(void *vplisteninfo)
#else           //UNIX must return "void *" to run as a new thread
  static void *_listensslthread(void *vplisteninfo)
#endif
{
    _ListenInfo_t *listeninfo = (_ListenInfo_t *)(vplisteninfo);
    CSock sock;
    CSSLSock sslsock;
    sslsock_t *sslinfo;
    CThr thr;
    SOCKET clientsd;
    CIndiFtps *pftps;
    char *privkeybuf = NULL, *certbuf = NULL, privpw[32];
    int errcode, privkeysize = 0, certsize = 0;
    
    _flaglistenssl = 1;     //indicate the SSL listen thread is active

        //initialize the certificate and keys for the SSL connection
    privkeybuf = listeninfo->psiteinfo->GetPrivKey(&privkeysize);   //get the private key
    certbuf = listeninfo->psiteinfo->GetCert(&certsize);            //get the x509 certificate
    listeninfo->psiteinfo->GetPrivKeyPW(privpw,sizeof(privpw));     //get the private key password
    if (privkeybuf == NULL || certbuf == NULL) {
        listeninfo->psiteinfo->WriteToProgLog("MAIN","WARNING: unable to initialize the SSL information.");
        _flaglistenssl = 0;     //indicate the SSL listen thread is inactive
        #ifndef WIN32
          return(NULL); //UNIX must return a value
        #else
          return;       //WINDOWS returns void
        #endif
    }

    while (siteinfoFlagQuit == 0) {
            //loop every _MAINLOOPFREQ sec
        if (sock.CheckStatus(listeninfo->sd,_MAINLOOPFREQ) > 0) {
            if ((clientsd = sock.Accept(listeninfo->sd)) != SOCK_INVALID) {
                    //setup the SSL connection
                sslinfo = sslsock.SSLServerNeg(clientsd,privkeybuf,privkeysize,privpw,certbuf,certsize,&errcode);
                    //Create the FTP server class to handle the client connection
                if ((pftps = new CIndiFtps(clientsd,listeninfo->psiteinfo)) != NULL) {
                    if (sslinfo == NULL) {  //SSL negotiation failed
                        listeninfo->psiteinfo->WriteToProgLog("MAIN","SSL negotiation failed from %s:%s -> %s (%u).",
                            pftps->GetClientIP(),pftps->GetClientPort(),pftps->GetClientName(),clientsd);
                        sock.Close(clientsd); delete pftps;
                        continue;
                    } else {                //SSL negotiation was successful
                        pftps->SetSSLInfo(sslinfo);
                    }
                    listeninfo->psiteinfo->WriteToProgLog("MAIN","SSL connection established from %s:%s -> %s (%u).",
                                pftps->GetClientIP(),pftps->GetClientPort(),pftps->GetClientName(),clientsd);
                    listeninfo->psiteinfo->WriteToAccessLog(pftps->GetServerIP(),pftps->GetServerPort(),pftps->GetClientIP(),
                                                            pftps->GetClientPort(),pftps->GetLogin(),"CONNECTSSL","","","");
                        //start a new thread to handle the client connection
                    if (thr.Create(_connect,(void *)pftps) == 0) {
                        listeninfo->psiteinfo->WriteToProgLog("MAIN","WARNING: unable to create a new client thread.");
                        sslsock.SSLClose(sslinfo); sock.Close(clientsd); delete pftps;
                    }
                    sslsock.FreeSSLInfo(sslinfo);
                } else {
                    if (sslinfo != NULL) {
                        sslsock.SSLClose(sslinfo); sslsock.FreeSSLInfo(sslinfo);
                    }
                    sock.Close(clientsd);
                }
            }
        }
    }

    listeninfo->psiteinfo->FreeCert(certbuf);
    listeninfo->psiteinfo->FreePrivKey(privkeybuf);

    free(listeninfo);

    _flaglistenssl = 0;     //indicate the SSL listen thread is inactive
      
    #ifndef WIN32
      return(NULL);   //UNIX must return a value
    #endif
}

    //Display the usage screen for the server.
static void _usage(char type)
{
    char d = INDIFILEUTILS_USERFILEDELIMITER;

    printf("%s version %s (%s)\n",_PROGNAME,_PROGVERSION,FTPS_COREVERSION);
    printf("\n");

    if (type == 'r') {
        printf("This option is used to specify a range of ports that can be used when\n");
        printf("then the server creates a data connection in passive mode.  This option\n");
        printf("would typically be used when the server is running behind a firewall.  In\n");
        printf("this case, the range of ports used by the server must be known to be able\n");
        printf("to open a hole in the firewall for the server\n");
        printf("\n");
        printf("Example: \"indiftpd -p21 -r10000-10099\" will start a server that listens\n");
        printf("         for a control connection on port 21 and will only open a data\n");
        printf("         connection on a port between (and including) 10000 and 10099\n");
        printf("         when the client uses passive (PASV) mode.\n");
        printf("\n");
        printf("NOTE: When a range of ports is specified with the \"-r<low>-<high>\" option,\n");
        printf("      only <high>-<low>+1 data connections will be able to be made at one\n");
        printf("      time.  Also, <low> should be at least greater than 1024 to avoid\n");
        printf("      conflicts with standard services (in general make <low> > 5000).\n");
    } else if (type == 'f') {
        printf("The user file contains the list of users allowed to login to the server.\n");
        printf("The file is formatted as follows:\n");
        printf("<username>%c<encrypted password>%c<home directory>%c<permissions>\n",d,d,d);
        printf("\n");
        printf("Use \"indiftpd -a<filename>\" to add a user to the user file.\n");
        printf("The default filename for the user file is \"%s\".\n",INDIFILEUTILS_DEFUSERFILENAME);
        printf("On UNIX systems the user file can be reloaded without restarting the server\n");
        printf("by sending the indiftpd process a SIGHUP signal (Ex. kill -HUP <PID>).\n");
        printf("NOTE: If the -f option is used, the -U, -P, -H, and -R flags are ignored.  If\n");
        printf("      the -f option is not used, the user file will not be checked.\n");
    } else if (type == 'U') {
        printf("The \"-U\" option is used to specify a user on the command line.  This option\n");
        printf("is usually used with the \"-P\", \"-H\", and \"-R\" options.  These options are\n");
        printf("designed for the situation when a user file (specified with the \"-f\" option) is\n");
        printf("not used.  The \"-U\" option enables a server to be setup quickly for a specific\n");
        printf("user without the need to create a user file.\n");
        printf("\n");
        printf("NOTE: The -U<user> option must always come before the -P<password> option.\n");
    } else if (type == 'R') {
        printf("The following permissions are available:\n");
        printf("l = list directory contents\n");
        printf("v = recursive directory listing\n");
        printf("c = change current working directory (CWD)\n");
        printf("p = print working dir (PWD)\n");
        printf("s = system info (SYST and STAT)\n");
        printf("t = SITE command\n");
        printf("d = download files\n");
        printf("u = upload files\n");
        printf("o = remove files\n");
        printf("a = rename files/directories\n");
        printf("m = make directories\n");
        printf("n = remove directories\n");
        printf("r = \"read\" -> d\n");
        printf("w = \"write\" -> uoamn\n");
        printf("x = \"execute\" -> lvcpst\n");
    } else if (type == 'L') {
        printf("The following logging levels are available:\n");
        printf("0 = no logging\n");
        printf("1 = reduced display only (display the reduced logs to the screen only)\n");
        printf("2 = display only (default - the logs are only displayed on the screen)\n");
        printf("3 = reduced logging (only write logins and transfers to the access log file)\n");
        printf("4 = normal logging (logs are only written to the log file)\n");
        printf("5 = reduced debug logging (reduced logs are also displayed)\n");
        printf("6 = debug logging (logs are also displayed on the screen)\n");
        printf("\n");
        printf("To specify a path for the log files use -L<level>:<path> --where <path> is\n");
        printf("the path the log files will be written to (the default path is the CWD).\n");
        printf("Example: \"indiftpd -L4:/var/log\" will use normal logging and write the\n");
        printf("         log files into the directory \"/var/log\".\n");
        printf("The program log file is \"%s\" and the access log file is \"%s\".\n",SITEINFO_DEFPROGLOG,SITEINFO_DEFACCESSLOG);
        printf("The program log file is used to log events and activities related to the\n");
        printf("operation of the FTP server.  The access log file is used to log the FTP\n");
        printf("transactions between the server and the connected FTP clients.  When reduced\n");
        printf("logging is selected, only the access log file is affected --the program log\n");
        printf("file will continue to log everything.\n");
    } else if (type == 'F') {
        printf("The following types are available:\n");
        printf("p = specify the format for the program log\n");
        printf("a = specify the format for the access log\n");
        printf("d = specify the format for the date field\n");
        printf("\n");
        printf("The following specifiers are available for the program log:\n");
        printf("%%D = date (format is specified using the \"-Fd:<format>\" option)\n");
        printf("%%S = sub-system (where in the prog the log is coming from)\n");
        printf("%%M = message (the message body)\n");
        printf("%%%% = A literal `%%' character\n");
        printf("The default program log format is \"%s\".\n",SITEINFO_DEFPROGFORMAT);
        printf("Example: \"indiftpd -Fp:%%D;%%S;%%M\" will create a log line such as\n");
        printf("         17/03/2002:20:19:33-0300;MAIN;Server started on port 8000\n");
        printf("\n");
        printf("The following specifiers are available for the access log:\n");
        printf("%%d = date (format is specified using the \"-Fd:<format>\" option)\n");
        printf("%%s = server IP\n");
        printf("%%l = server listening port\n");
        printf("%%c = client IP\n");
        printf("%%p = client port\n");
        printf("%%u = user name\n");
            //wait for the user to press enter
        printf("-- Press ENTER to continue --\n"); getchar(); fflush(0);
        printf("%%m = FTP command (method)\n");
        printf("%%%% = A literal `%%' character\n");
        printf("%%a = argument to the command\n");
        printf("%%r = 3-digit response code from the server\n");
        printf("%%t = response text from the server\n");
        printf("The default access log format is \"%s\".\n",SITEINFO_DEFACCESSFORMAT);
        printf("\n");
        printf("The following specifiers are available for the date field:\n");
        printf("%%a = abbreviated weekday name according to the current locale\n");
        printf("%%A = full weekday name according to the current locale\n");
        printf("%%b = abbreviated month name according to the current locale\n");
        printf("%%B = full month name according to the current locale\n");
        printf("%%d = day of the month as a decimal number (range 01 to 31)\n");
        printf("%%H = hour as a decimal number using a 24-hour clock (range 00 to 23)\n");
        printf("%%I = hour as a decimal number using a 12-hour clock (range 01 to 12)\n");
        printf("%%j = day of the year as a decimal number (range 001 to 366)\n");
        printf("%%m = month as a decimal number (range 01 to 12)\n");
        printf("%%p = current locale's AM/PM. indicator for 12-hour clock\n");
        printf("%%S = second as a decimal number (range 00 to 61)\n");
        printf("%%w = day of the week as a decimal, range 0 to 6, Sunday being 0\n");
        printf("%%y = year as a decimal number without a century (range 00 to 99)\n");
        printf("%%Y = year as a decimal number including the century\n");
        printf("%%z = time-zone as hour offset from GMT\n");
            //wait for the user to press enter
        printf("-- Press ENTER to continue --\n"); getchar(); fflush(0);
        printf("%%Z = time zone or name or abbreviation\n");
        printf("%%O = display the 5-digit (w/ + or -) UTC offset in minutes (Ex. EST -> -0300)\n");
        printf("%%%% = A literal `%%' character\n");
        printf("The default date format is \"%s\".\n",SITEINFO_DEFDATEFORMAT);
        printf("Note: The specifiers for the date field are the same as the C-language\n");
        printf("      function strftime() except for the added specifier \"%%O\".\n");
        printf("\n");
        printf("The -F<type>:<format> option can be specified more than once on the command\n");
        printf("line to specify the formatting for each of the log types.\n");
        printf("Example: \"indiftpd -Fp:%%D,%%M -Fd:\"%%Y-%%m-%%d %%H:%%M:%%S\"\" ->\n");
        printf("         2002-03-17 22:18:41,Server started on port 8000\n");
    } else {
        printf("usage:  indiftpd [options]\n");
        printf("\n");
        printf("options:\n");
        printf("-h or -help       displays this message\n");
        printf("    -hr           displays help on data port ranges\n");
        printf("    -hf           displays help on user files\n");
        printf("    -hU           displays help on specifying a user\n");
        printf("    -hR           displays help on permissions\n");
        printf("    -hL           displays help on logging\n");
        printf("    -hF           displays help on log formatting\n");
        printf("-p<port>          port number the server will run on (default = 21)\n");
        printf("-r<low>-<high>    range of ports to use for data connections (default = any)\n");
        printf("-f<userfile>      file containing user info (def userfile = %s)\n",INDIFILEUTILS_DEFUSERFILENAME);
        printf("-a<userfile>      add a user to the user info file (doesn't start server)\n");
        printf("-b<IP>            IP address to bind to (must be in x.x.x.x form)\n");
        printf("-e                enable explicit SSL (AUTH SSL)\n");
        printf("-i<port>          enable implicit SSL on the specified port (default = 990)\n");
        printf("-U<user>          specify a user for the site (default = anonymous)\n");
        printf("-P<password>      password for the user (default = [no password])\n");
        printf("-H<homedir>       home directory for the user (default = CWD)\n");
        printf("-R<flags>         permissions for the user (default = %s)\n",SITEINFO_DEFAULTPERMISSIONS);
        printf("-L<level>:<path>  logging level (default level = 2, path = CWD)\n");
        printf("-F<type>:<format> set the format for the program/access (type = p/a) logs");
    }
    
    printf("\n");
}

    //Initializes the FTP server (based on the command line inputs).
    //Returns 0 if there was an error initializing the server
    //Returns 1 if its ok for the server to start
    //Returns 2 if a user was added and the server shouldn't start
static int _initserver(CIndiSiteInfo *psiteinfo, int argc, char **argv, char *bindip, char *port, char *sslport, int *loglevel)
{
    CIndiFileUtils fileutils;
    CStrUtils strutils;     //String utilities class
    CCrypto crypto;         //Encryption/Decryption class
    char *ptr, *pwenc, passwordbuffer[SITEINFO_MAXPWLINE];
    CFSUtils fsutils;       //File System Utilities class
    indisiteinfoUserProp_t userprop;
    int i, retval, startport, endport;
    int flagadd = 0, flagfile = 0;

    if (psiteinfo == NULL || port == NULL || sslport == NULL)
        return(0);

        //set the default server port to 21
    strncpy(port,"21",SOCK_PORTLEN-1);
    port[SOCK_PORTLEN-1] = '\0';

        //set the default user properties
    memset(&userprop,0,sizeof(indisiteinfoUserProp_t));
        //set the default username
    strncpy(userprop.pwinfo.username,"anonymous",sizeof(userprop.pwinfo.username)-1);
    userprop.pwinfo.username[sizeof(userprop.pwinfo.username)-1] = '\0';
        //set the default password (users in the anonymous group don't need a password)
    sprintf(userprop.pwinfo.groupnumber,"%d",SITEINFO_ANONYMOUSGROUPNUM);
        //set the default home directory for the user (use CWD)
    fsutils.GetCWD(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir)-1);
    fsutils.CheckSlashEnd(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir)); //make sure the path ends in a slash
        //set the default permissions
    strncpy(userprop.permissions,SITEINFO_DEFAULTPERMISSIONS,sizeof(userprop.permissions)-1);
    userprop.permissions[sizeof(userprop.permissions)-1] = '\0';

    for (i = 1; i < argc; i++) {
        if (*(argv[i]) == '-') {
            strutils.RemoveEOL(argv[i]);
            switch (*(argv[i]+1)) {

                case 'p': { //set the port for the server to listen on
                    if (isdigit(*(argv[i]+2))) {    //if a port number was specified
                        strncpy(port,argv[i]+2,SOCK_PORTLEN-1);
                        port[SOCK_PORTLEN-1] = '\0';
                    }
                } break;

                case 'r': { //set the range of data ports for PASV mode
                    if ((ptr = strchr(argv[i]+2,'-')) != NULL) {
                        startport = atoi(argv[i]+2);
                        endport = atoi(ptr+1);
                        if ((*(argv[i]+2) != '0' && startport == 0) || (*(ptr+1) != '0' && endport == 0)) {
                            psiteinfo->WriteToProgLog("MAIN","WARNING: invalid port range \"%s\" (using default = any).",argv[i]+2);
                        } else {
                            if (startport > 65534 || startport <= 0 || endport > 65534 || endport <= 0) {
                                psiteinfo->WriteToProgLog("MAIN","WARNING: port numbers must be between 1 and 65534 (using default = any).");
                            } else {
                                if (psiteinfo->SetDataPortRange(startport,endport) == 0)
                                    psiteinfo->WriteToProgLog("MAIN","WARNING: invalid port range \"%s\" (using default = any).",argv[i]+2);
                            }
                        }
                    } else {
                        psiteinfo->WriteToProgLog("MAIN","WARNING: invalid port range, no \"-\" was found (using default = any).");
                    }
                } break;

                case 'f': { //load the user information file
                    psiteinfo->ClearUsers();    //clear any pre-existing user lists
                    if (*(argv[i]+2) == '\0') {
                        retval = psiteinfo->LoadUsers(INDIFILEUTILS_DEFUSERFILENAME);
                        psiteinfo->WriteToProgLog("MAIN","%d user(s) were loaded from the file %s.",retval,INDIFILEUTILS_DEFUSERFILENAME);
                    } else {
                        retval = psiteinfo->LoadUsers(argv[i]+2);
                        psiteinfo->WriteToProgLog("MAIN","%d user(s) were loaded from the file %s.",retval,argv[i]+2);
                    }
                    flagfile = 1;   //at least 1 user was added
                } break;

                case 'a': { //add a user to the user information file
                    printf("Enter the user's login: ");
                    fgets(userprop.pwinfo.username,sizeof(userprop.pwinfo.username),stdin);
                    strutils.RemoveEOL(userprop.pwinfo.username);
                        //make sure the username is not too long
                    if (strlen(userprop.pwinfo.username) > FTPS_MAXLOGINSIZE)
                        userprop.pwinfo.username[FTPS_MAXLOGINSIZE] = '\0';
                    printf("Enter the user's password: ");
                    strutils.GetStrHidden(passwordbuffer,sizeof(passwordbuffer));
                    printf("Retype the user's password: ");
                    strutils.GetStrHidden(userprop.pwinfo.passwordraw,sizeof(userprop.pwinfo.passwordraw));
                    if (strcmp(passwordbuffer,userprop.pwinfo.passwordraw) != 0) {
                        printf("Passwords do not match.\n");
                        return(0);
                    }
                    strutils.RemoveEOL(userprop.pwinfo.passwordraw);
                        //make sure the password is not too long
                    if (strlen(userprop.pwinfo.passwordraw) > FTPS_MAXPWSIZE)
                        userprop.pwinfo.passwordraw[FTPS_MAXPWSIZE] = '\0';
#if 0
                        //store the encrypted version of the password
                    if ((pwenc = crypto.Encrypt2HexStr(userprop.pwinfo.passwordraw,userprop.pwinfo.username,userprop.pwinfo.passwordraw,FTPS_MAXPWSIZE)) != NULL) {
                        strncpy(userprop.pwinfo.passwordraw, pwenc, sizeof(userprop.pwinfo.passwordraw)-1);
                        userprop.pwinfo.passwordraw[sizeof(userprop.pwinfo.passwordraw)-1] = '\0';
                        crypto.FreeDataBuffer(pwenc);
                    }
#else
                    //Store the password as plain temporaly
                    userprop.pwinfo.passwordraw[sizeof(userprop.pwinfo.passwordraw)-1] = '\0';
#endif
                    printf("Enter the user's home directory [%s]: \n> ",userprop.pwinfo.homedir);
                    fgets(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir),stdin);
                    strutils.RemoveEOL(userprop.pwinfo.homedir);
                    if (*(userprop.pwinfo.homedir) == '\0') {   //if empty, use the default
                        fsutils.GetCWD(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir)-1);
                        fsutils.CheckSlashEnd(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir)); //make sure the path ends in a slash
                    }
                    printf("Enter the user's permissions [%s]: \n> ",userprop.permissions);
                    fgets(userprop.permissions,sizeof(userprop.permissions),stdin);
                    strutils.RemoveEOL(userprop.permissions);
                    if (*(userprop.permissions) == '\0') {   //if empty, use the default
                        strncpy(userprop.permissions,SITEINFO_DEFAULTPERMISSIONS,sizeof(userprop.permissions)-1);
                        userprop.permissions[sizeof(userprop.permissions)-1] = '\0';
                    }
                    printf("\n");
                    if (*(argv[i]+2) == '\0')
                        retval = fileutils.AddUserLine(INDIFILEUTILS_DEFUSERFILENAME,&userprop);
                    else
                        retval = fileutils.AddUserLine(argv[i]+2,&userprop);
                    if (retval != 0) {
                        psiteinfo->WriteToProgLog("MAIN","User %s was successfully added.",userprop.pwinfo.username);
                        flagadd = 1; //exit after adding the user line (don't start the server)
                    }
                } break;

                case 'b': { //set the IP address to bind to
                    strncpy(bindip,argv[i]+2,SOCK_IPADDRLEN-1);
                    bindip[SOCK_IPADDRLEN-1] = '\0';
                } break;

                case 'e': { //enable explicit SSL
                    if (psiteinfo->InitSSLInfo() == 0)  //initialize the certificate and keys
                        psiteinfo->WriteToProgLog("MAIN","WARNING: unable to initialize the SSL information.");
                } break;
                    
                case 'i': { //enable implicit SSL using the specified port (default = 990)
                    if (psiteinfo->InitSSLInfo() == 0) {    //initialize the certificate and keys
                        psiteinfo->WriteToProgLog("MAIN","WARNING: unable to initialize the SSL information.");
                    } else {
                        if (isdigit(*(argv[i]+2))) {    //if a port number was specified
                            strncpy(sslport,argv[i]+2,SOCK_PORTLEN-1);
                            sslport[SOCK_PORTLEN-1] = '\0';
                        } else {
                            strncpy(sslport,"990",SOCK_PORTLEN-1);   //default to port 990
                            sslport[SOCK_PORTLEN-1] = '\0';
                        }
                    }
                } break;
                    
                case 'U': { //set the username
                    strncpy(userprop.pwinfo.username,argv[i]+2,sizeof(userprop.pwinfo.username)-1);
                    userprop.pwinfo.username[sizeof(userprop.pwinfo.username)-1] = '\0';
                } break;

                case 'P': { //set the password
                    strncpy(userprop.pwinfo.passwordraw,argv[i]+2,sizeof(userprop.pwinfo.passwordraw)-1);
                    userprop.pwinfo.passwordraw[sizeof(userprop.pwinfo.passwordraw)-1] = '\0';
                        //make sure the password is not too long
                    if (strlen(userprop.pwinfo.passwordraw) > FTPS_MAXPWSIZE)
                        userprop.pwinfo.passwordraw[FTPS_MAXPWSIZE] = '\0';
#if 0
                        //store the encrypted version of the password
                    if ((pwenc = crypto.Encrypt2HexStr(userprop.pwinfo.passwordraw,userprop.pwinfo.username,userprop.pwinfo.passwordraw)) != NULL) {
                        strncpy(userprop.pwinfo.passwordraw,pwenc,sizeof(userprop.pwinfo.passwordraw)-1);
                        userprop.pwinfo.passwordraw[sizeof(userprop.pwinfo.passwordraw)-1] = '\0';
                        crypto.FreeDataBuffer(pwenc);
                    }
#else
                    userprop.pwinfo.passwordraw[sizeof(userprop.pwinfo.passwordraw)-1] = '\0';
#endif
                        //change the group to no longer be anonymous
                    strcpy(userprop.pwinfo.groupnumber,"1");
                } break;

                case 'H': { //set the home directory
                        //make sure the specified homedir is a valid directory
                    if (fsutils.ValidatePath(argv[i]+2) == 1) {
                        strncpy(userprop.pwinfo.homedir,argv[i]+2,sizeof(userprop.pwinfo.homedir)-2);
                        userprop.pwinfo.homedir[sizeof(userprop.pwinfo.homedir)-2] = '\0';
                        fsutils.CheckSlash(userprop.pwinfo.homedir);    //make sure the slashed are correct
                        fsutils.CheckSlashEnd(userprop.pwinfo.homedir,sizeof(userprop.pwinfo.homedir)); //make sure the path ends in a slash
                    } else {
                        psiteinfo->WriteToProgLog("MAIN","WARNING: invalid home directory \"%s\".",argv[i]+2);
                    }
                } break;

                case 'R': { //set the permissions
                    strncpy(userprop.permissions,argv[i]+2,sizeof(userprop.permissions)-1);
                    userprop.permissions[sizeof(userprop.permissions)-1] = '\0';
                } break;

                case 'L': { //set logging options
                    if (*(argv[i]+2) == '0')
                        *loglevel = INDISITEINFO_LOGLEVELNONE;
                    else if (*(argv[i]+2) == '1')
                        *loglevel = INDISITEINFO_LOGLEVELREDDISPLAY;
                    else if (*(argv[i]+2) == '2')
                        *loglevel = INDISITEINFO_LOGLEVELDISPLAY;
                    else if (*(argv[i]+2) == '3')
                        *loglevel = INDISITEINFO_LOGLEVELREDUCED;
                    else if (*(argv[i]+2) == '4')
                        *loglevel = INDISITEINFO_LOGLEVELNORMAL;
                    else if (*(argv[i]+2) == '5')
                        *loglevel = INDISITEINFO_LOGLEVELREDDEBUG;
                    else if (*(argv[i]+2) == '6')
                        *loglevel = INDISITEINFO_LOGLEVELDEBUG;
                        //check if a log path was specified
                    if ((ptr = strchr(argv[i],':')) != NULL) {
                        ptr++;
                        if (*ptr != '\0') {
                            if (psiteinfo->SetProgLogName(SITEINFO_DEFPROGLOG,ptr) == 0)
                                psiteinfo->WriteToProgLog("MAIN","WARNING: unable to set the program log path.");
                            if (psiteinfo->SetAccessLogName(SITEINFO_DEFACCESSLOG,ptr) == 0)
                                psiteinfo->WriteToProgLog("MAIN","WARNING: unable to set the access log path.");
                        }
                    }
                } break;

                case 'F': { //set the log format
                    if ((ptr = strchr(argv[i],':')) != NULL) {
                        ptr++;
                        if (*(argv[i]+2) == 'p')
                            psiteinfo->SetProgLogFormatStr(ptr);
                        else if (*(argv[i]+2) == 'a')
                            psiteinfo->SetAccessLogFormatStr(ptr);
                        else if (*(argv[i]+2) == 'd')
                            psiteinfo->SetDateFormatStr(ptr);
                    }
                }  break;

                default: {
                } break;

            } // end switch
        }
    }

        //do not try to readd a user if this is being called because of a
        //refresh (HUP) signal.
    if (_flaghup != 0)
        return(1);

        //always return without starting the server if a user was added
    if (flagadd != 0)
        return(2);  //do not start the server

        //if users were added from the config file, return success
    if (flagfile != 0)
        return(1);

        //if a configuration file was not used, add the user specified
        //on the command line (or the default user)
    if (psiteinfo->AddUser(&userprop) == 0) {
        psiteinfo->WriteToProgLog("MAIN","ERROR: unable to initialize the server.");
        return(0);
    } else {
        return(1);
    }
}

    //standard signal handler used to catch signals such as SIGINT
static void _sighandler(int sig)
{

    #ifndef WIN32
      if (sig == SIGPIPE)   //ignore SIGPIPE
          return;
      if (sig == SIGHUP) {
          _flaghup = 1;
          return;
      }
    #endif

    siteinfoFlagQuit = 1;  //exit the program
}

    //Used to check if the user entered a transfer command.
    //Returns 0 if the command is not a transfer command
    //Returns 1 if download, and 2 if upload
static int _iscommandxfer(char *command)
{
    CStrUtils strutils;
    char commandbuffer[5];

        //Extract the FTP command
    strncpy(commandbuffer,command,sizeof(commandbuffer)-1);
    commandbuffer[sizeof(commandbuffer)-1] = '\0';

        //check for download commands
    if (strutils.CaseCmp(commandbuffer,"RETR") == 0)
        return(1);

        //check for upload commands
    if (strutils.CaseCmp(commandbuffer,"STOR") == 0 ||
        strutils.CaseCmp(commandbuffer,"STOU") == 0 ||
        strutils.CaseCmp(commandbuffer,"APPE") == 0)
        return(2);

    return(0);
}

    //This is the main function that handles each client.
    //This function is run in a new thread for each client connection.
#ifdef WIN32
  static void _connect(void *vpftps)    //WINDOWS must return "void" to run as a new thread
#else
  static void *_connect(void *vpftps)   //UNIX must return "void *" to run as a new thread
#endif
{
    CIndiFtps *pftps = (CIndiFtps *)vpftps; //FTP server class
    CTermsrv ftpscmd;       //Terminal command class (used to send and recv FTP commands)
    CSock sock;             //Sockets class (contains all the network func)
    CSSLSock sslsock;       //SSL connection class (contains all the SSL func)
    CCmdLine cmdline;       //Stores the FTP received command line
    CStrUtils strutils;     //String utilities class
    CIndiSiteInfo *psiteinfo;   //pointer to the site information class (used for logging)
    CTimer timer;           //used for Sleep() and other time related functions

        //set the site information pointer
    psiteinfo = (CIndiSiteInfo *)pftps->GetSiteInfoPtr();

    char **argv;        //array of pointers to the individual command line arguments
    int argc = 0;       //the number of command line arguments

    int sockstatus;     //the status of the socket (has data been received)

    int flaguser = 0;   //is the user allowed access (1 = user is allowed)
    int flagauth = 0;   //is the user authenticated  (1 = user if authenticated)

    _nconnections++;    //increment the number of current client connections

        //Set the socket descriptor for the FTP command class
    ftpscmd.SetSocketDesc(pftps->GetSocketDesc());
    if (pftps->GetUseSSL() != 0) {  //set the SSL info (using implicit SSL)
        ftpscmd.SetSSLInfo(pftps->GetSSLInfo());    //set the SSL info
        ftpscmd.SetUseSSL(1);                       //set to use SSL
    }

        //Set the socket to keepalive to detect a dead socket
    if (sock.SetKeepAlive(pftps->GetSocketDesc()) == 0)
        psiteinfo->WriteToProgLog("MAIN","Failed to set socket to KeepAlive.");

        //Set the socket to receive OOB data (for ABOR)
    if (sock.SetRecvOOB(pftps->GetSocketDesc()) == 0)
        psiteinfo->WriteToProgLog("MAIN","Failed to set socket to receive OOB data.");

        //Send the Login string to the client
    pftps->EventHandler(0,NULL,_LOGINSTR,"220",1,1,0);

    while (pftps->GetFlagQuit() == 0 && siteinfoFlagQuit == 0) {
        if ((sockstatus = sock.CheckStatus(pftps->GetSocketDesc(),_USERLOOPFREQ)) > 0) {
                //receive the FTP command and check if the connection was broken
            if (ftpscmd.CommandRecv(cmdline.m_cmdline,cmdline.GetCmdLineSize()) == 0) {
                psiteinfo->WriteToProgLog("MAIN","Error connecting to client (%s disconnected).",pftps->GetLogin());
                pftps->DoQUIT(0,NULL);
                break;
            }
                //update the user's current information
            pftps->SetLastActive();             //update the last active time
            //TODO: update last command
        } else {
            strcpy(cmdline.m_cmdline,"~");    //used to indicate no activity
        }

            //parse the input from the user
        argv = cmdline.ParseCmdLine(&argc);
        //if (*(cmdline.m_cmdline) != '~') {
        //    for (int i= 0; i < argc; i++) printf("argv%d=%s, ",i,argv[i]); printf("\n"); //DEBUG!!!!
        //}

            //make sure the user is logged in
            //if the user is not logged in only allow the commands
            //USER, PASS, or QUIT
        if (argc > 0 && flagauth == 0) {        //user is not authenticated

            switch (*(argv[0])) {

                case '~': {
                    if (sockstatus > 0) //if a command was received
                        pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                    //else Do Nothing (idle indicator)
                } break;

                case 'A': case 'a': {
                    if (strutils.CaseCmp(argv[0],"AUTH") == 0) {
                        if (pftps->DoAUTH(argc,argv) != 0) {
                            ftpscmd.SetSSLInfo(pftps->GetSSLInfo());    //set the SSL info
                            ftpscmd.SetUseSSL(1);                       //set to use SSL
                        }
                    } else {
                        pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                    }
                } break;

                case 'U': case 'u': {
                    if (strutils.CaseCmp(argv[0],"USER") == 0) {
                        flaguser = pftps->DoUSER(argc,argv);
                    } else {
                        pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                    }
                } break;

                case 'P': case 'p': {
                    if (flaguser != 0 && strutils.CaseCmp(argv[0],"PASS") == 0) {
                        if (pftps->DoPASS(argc,argv) == 0) {
                            flaguser = 0;
                            flagauth = 0;  //the user was not authenticated
                        } else {
                            flagauth = 1;  //the user was authenticated
                        }
                    } else if (strutils.CaseCmp(argv[0],"PBSZ") == 0) {
                        pftps->DoPBSZ(argc,argv);
                    } else if (strutils.CaseCmp(argv[0],"PROT") == 0) {
                        pftps->DoPROT(argc,argv);
                    } else {
                        pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                    }
                } break;

                case 'Q': case 'q': {
                    if (strutils.CaseCmp(argv[0],"QUIT") == 0) {
                        pftps->DoQUIT(argc,argv);
                    } else {
                        pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                    }
                } break;

                default: {
                    pftps->EventHandler(argc,argv,"Please login with USER and PASS.","530",1,1,0);
                } break;

            } // end switch

        } else if (argc > 0 && flagauth != 0) { //user is authenticated

            switch (*(argv[0])) {

                case '~': {
                    if (sockstatus > 0)     //if a command was received
                        pftps->ExecFTP(argc,argv);
                    //else Check timer values
                } break;

                case 'A': case 'a': {
                    if (strutils.CaseCmp(argv[0],"AUTH") == 0) {
                        if (pftps->DoAUTH(argc,argv) != 0) {
                            ftpscmd.SetSSLInfo(pftps->GetSSLInfo());    //set the SSL info
                            ftpscmd.SetUseSSL(1);                       //set to use SSL
                        }
                    } else {
                        pftps->ExecFTP(argc,argv);
                    }
                } break;

                case 'P': case 'p': {
                    if (strutils.CaseCmp(argv[0],"PASS") == 0) {
                        if (pftps->DoPASS(argc,argv) == 0) {
                            flaguser = 0;
                            flagauth = 0;  //the user was not authenticated
                        }
                    } else {
                        pftps->ExecFTP(argc,argv);
                    }
                } break;

                case 'U': case 'u': {
                    if (strutils.CaseCmp(argv[0],"USER") == 0) {
                        flagauth = 0;  //reset the authorization
                        flaguser = pftps->DoUSER(argc,argv);
                    } else {
                        pftps->ExecFTP(argc,argv);
                    }
                } break;

                default: {
                        //check if the user entered a data transfer command (Ex. RETR, STOR)
                    if (_iscommandxfer(cmdline.m_cmdline) != 0) {
                            //only allow 1 data transfer at a time
                        if (pftps->GetNumDataThreads() > 0) {
                            pftps->EventHandler(argc,argv,"Only 1 transfer per control connection is allowed.","550",1,1,0);
                            break;
                        }
                    }
                        //execute the FTP command
                    pftps->ExecFTP(argc,argv);
                } break;

            } // end switch

        } else {                                //nothing was entered
            pftps->EventHandler(argc,argv,"command not understood","500",1,1,0);
        }
    }

    if (ftpscmd.GetUseSSL() != 0) sslsock.SSLClose(pftps->GetSSLInfo());    //shutdown SSL
    sock.Close(pftps->GetSocketDesc()); //close the control connection
    psiteinfo->WriteToProgLog("MAIN","Connection closed (%u), %s disconnected.",pftps->GetSocketDesc(),pftps->GetLogin());
    psiteinfo->WriteToAccessLog(pftps->GetServerIP(),pftps->GetServerPort(),pftps->GetClientIP(),pftps->GetClientPort(),pftps->GetLogin(),"DISCONNECT","","","");

        //DON'T delete the CFtps class until ALL connection threads have exited
    pftps->SetFlagAbor(1); pftps->SetFlagQuit(1); //stop all threads
    while ((pftps->GetNumListThreads()+pftps->GetNumDataThreads()) > 0) timer.Sleep(100); //wait until all threads have stopped
    delete pftps;  //delete the FTP server class

    _nconnections--;    //decrement the number of current client connections

    #ifndef WIN32
      return(NULL);   //UNIX must return a value
    #endif
}
