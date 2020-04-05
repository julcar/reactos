// Copyright (c) 2004-2013 Sergey Lyubka
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#undef UNICODE                    // Use ANSI WinAPI functions
#undef _UNICODE                   // Use multibyte encoding on Windows
#define _MBCS                     // Use multibyte encoding on Windows

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>

#include "mongoose.h"

#include <windows.h>
#include <direct.h>  // For chdir()
#include <winsvc.h>
#include <shlobj.h>

#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

#ifndef S_ISDIR
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif

#define DIRSEP '\\'
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define sleep(x) Sleep((x) * 1000)
#define abs_path(rel, abs, abs_size) _fullpath((abs), (rel), (abs_size))
#define SIGCHLD 0
typedef struct _stat file_stat_t;
#define stat(x, y) my_stat((x), (y))

#define MAX_OPTIONS 100
#define MAX_CONF_FILE_LINE_SIZE (8 * 1024)

static int exit_flag;
static char server_name[40];        // Set by init_server_name()
static char config_file[PATH_MAX];  // Set by process_command_line_arguments()
static struct mg_server *server;    // Set by start_mongoose()
static const char static_user_name[64] = "#__u__#";
static const char static_user_email[64] = "#__e__#";

#if !defined(CONFIG_FILE)
#define CONFIG_FILE "mongoose.conf"
#endif /* !CONFIG_FILE */

static void __cdecl signal_handler(int sig_num) {
  // Reinstantiate signal handler
  signal(sig_num, signal_handler);
  { exit_flag = sig_num; }
}

static void die(const char *fmt, ...) {
  va_list ap;
  char msg[200];

  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  MessageBox(NULL, msg, "Error", MB_OK);

  exit(EXIT_FAILURE);
}

static void show_usage_and_exit(void) {
  const char **names;
  int i;

  fprintf(stderr, "Mongoose version %s (c) Sergey Lyubka, built on %s\n",
          MONGOOSE_VERSION, __DATE__);
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "  mongoose [config_file]\n");
  fprintf(stderr, "  mongoose [-option value ...]\n");
  fprintf(stderr, "\nOPTIONS:\n");

  names = mg_get_valid_option_names();
  for (i = 0; names[i] != NULL; i += 2) {
    fprintf(stderr, "  -%s %s\n",
            names[i], names[i + 1] == NULL ? "<empty>" : names[i + 1]);
  }
  exit(EXIT_FAILURE);
}

static const char *config_file_top_comment =
"# Mongoose web server configuration file.\n"
"# For detailed description of every option, visit\n"
"# https://github.com/cesanta/mongoose\n"
"# Lines starting with '#' and empty lines are ignored.\n"
"# To make a change, remove leading '#', modify option's value,\n"
"# save this file and then restart Mongoose.\n\n";

static const char *get_url_to_me(const struct mg_server *server) {
  static char url[100];
  const char *s = mg_get_option(server, "listening_port");
  const char *cert = mg_get_option(server, "ssl_certificate");

  snprintf(url, sizeof(url), "%s://%s%s",
           cert != NULL && cert[0] != '\0' ? "https" : "http",
           s == NULL || strchr(s, ':') == NULL ? "127.0.0.1:" : "", s);

  return url;
}

#if 0
static void create_config_file(const char *path) {
  const char **names, *value;
  FILE *fp;
  int i;

  // Create config file if it is not present yet
  if ((fp = fopen(path, "r")) != NULL) {
    fclose(fp);
  } else if ((fp = fopen(path, "a+")) != NULL) {
    fprintf(fp, "%s", config_file_top_comment);
    names = mg_get_valid_option_names();
    for (i = 0; names[i * 2] != NULL; i++) {
      value = mg_get_option(server, names[i * 2]);
      fprintf(fp, "# %s %s\n", names[i * 2], value ? value : "<value>");
    }
    fclose(fp);
  }
}
#endif

static char *sdup(const char *str) {
  char *p;
  if ((p = (char *) malloc(strlen(str) + 1)) != NULL) {
    strcpy(p, str);
  }
  return p;
}

static void set_option(char **options, const char *name, const char *value) {
  int i;

  for (i = 0; i < MAX_OPTIONS - 3; i++) {
    if (options[i] == NULL) {
      options[i] = sdup(name);
      options[i + 1] = sdup(value);
      options[i + 2] = NULL;
      break;
    } else if (!strcmp(options[i], name)) {
      free(options[i + 1]);
      options[i + 1] = sdup(value);
      break;
    }
  }

  if (i == MAX_OPTIONS - 3) {
    die("%s", "Too many options specified");
  }
}

static int my_argc;
static char **my_argv;

static void to_utf8(wchar_t *src, char *dst, size_t dst_len) {
  WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, dst_len, 0, 0);
}

static void to_wchar(const char *src, wchar_t *dst, size_t dst_len) {
  MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dst_len);
}

static void init_utf8_argc_argv(void) {
  wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &my_argc);
  if (wargv != NULL) {
    char buf[1024 * 8];
    int i;
    // TODO(lsm): free that at some point.
    my_argv = calloc(my_argc + 1, sizeof(my_argv[0]));
    for (i = 0; i < my_argc; i++) {
      to_utf8(wargv[i], buf, sizeof(buf));
      my_argv[i] = _strdup(buf);
    }
    LocalFree(wargv);
  }
}

static void process_command_line_arguments(char *argv[], char **options) {
  char line[MAX_CONF_FILE_LINE_SIZE], opt[sizeof(line)], val[sizeof(line)], *p;
  FILE *fp = NULL;
  size_t i, cmd_line_opts_start = 1, line_no = 0;

  // Should we use a config file ?
  if (argv[1] != NULL && argv[1][0] != '-') {
    snprintf(config_file, sizeof(config_file), "%s", argv[1]);
    cmd_line_opts_start = 2;
  } else if ((p = strrchr(argv[0], DIRSEP)) == NULL) {
    // No command line flags specified. Look where binary lives
    snprintf(config_file, sizeof(config_file), "%s", CONFIG_FILE);
  } else {
    snprintf(config_file, sizeof(config_file), "%.*s%c%s",
             (int) (p - argv[0]), argv[0], DIRSEP, CONFIG_FILE);
  }

  {
    wchar_t path[PATH_MAX];
    to_wchar(config_file, path, sizeof(path) / sizeof(path[0]));
    fp = _wfopen(path, L"r");
  }

  // If config file was set in command line and open failed, die
  if (cmd_line_opts_start == 2 && fp == NULL) {
    die("Cannot open config file %s: %s", config_file, strerror(errno));
  }

  // Load config file settings first
  if (fp != NULL) {
    fprintf(stderr, "Loading config file %s\n", config_file);

    // Loop over the lines in config file
    while (fgets(line, sizeof(line), fp) != NULL) {
      line_no++;

      // Ignore empty lines and comments
      for (i = 0; isspace(* (unsigned char *) &line[i]); ) i++;
      if (line[i] == '#' || line[i] == '\0') {
        continue;
      }

      if (sscanf(line, "%s %[^\r\n#]", opt, val) != 2) {
        printf("%s: line %d is invalid, ignoring it:\n %s",
               config_file, (int) line_no, line);
      } else {
        set_option(options, opt, val);
      }
    }

    fclose(fp);
  }
}

static void init_server_name(void) {
  snprintf(server_name, sizeof(server_name), "Mongoose web server v.%s",
           MONGOOSE_VERSION);
}

static int is_path_absolute(const char *path) {
  return path != NULL &&
    ((path[0] == '\\' && path[1] == '\\') ||  // UNC path, e.g. \\server\dir
     (isalpha(path[0]) && path[1] == ':' && path[2] == '\\'));  // E.g. X:\dir
}

static char *get_option(char **options, const char *option_name) {
  int i;

  for (i = 0; options[i] != NULL; i++)
    if (!strcmp(options[i], option_name))
      return options[i + 1];

  return NULL;
}

static int my_stat(const char *path, file_stat_t *st) {
  wchar_t buf[PATH_MAX];
  MultiByteToWideChar(CP_UTF8, 0, path, -1, buf, sizeof(buf) / sizeof(buf[0]));
  return _wstat(buf, st);
}

static void verify_existence(char **options, const char *option_name,
                             int must_be_dir) {
  file_stat_t st;
  const char *path = get_option(options, option_name);

  if (path != NULL && (stat(path, &st) != 0 ||
                       ((S_ISDIR(st.st_mode) ? 1 : 0) != must_be_dir))) {
    die("Invalid path for %s: [%s]: (%s). Make sure that path is either "
        "absolute, or it is relative to mongoose executable.",
        option_name, path, strerror(errno));
  }
}

static void set_absolute_path(char *options[], const char *option_name,
                              const char *path_to_mongoose_exe) {
  char path[PATH_MAX], abs[PATH_MAX], *option_value;
  const char *p;

  // Check whether option is already set
  option_value = get_option(options, option_name);

  // If option is already set and it is an absolute path,
  // leave it as it is -- it's already absolute.
  if (option_value != NULL && !is_path_absolute(option_value)) {
    // Not absolute. Use the directory where mongoose executable lives
    // be the relative directory for everything.
    // Extract mongoose executable directory into path.
    if ((p = strrchr(path_to_mongoose_exe, DIRSEP)) == NULL) {
      wchar_t buf[PATH_MAX];
      GetCurrentDirectoryW(sizeof(buf) / sizeof(buf[0]), buf);
      to_utf8(buf, path, sizeof(path));
    } else {
      snprintf(path, sizeof(path), "%.*s", (int) (p - path_to_mongoose_exe),
               path_to_mongoose_exe);
    }

    strncat(path, "/", sizeof(path) - 1);
    strncat(path, option_value, sizeof(path) - 1);

    // Absolutize the path, and set the option
    abs_path(path, abs, sizeof(abs));
    set_option(options, option_name, abs);
  }
}

static void start_mongoose(int argc, char *argv[]) {
  char *options[MAX_OPTIONS];
  int i;

  if ((server = mg_create_server(NULL)) == NULL) {
    die("%s", "Failed to start Mongoose.");
  }

  // Show usage if -h or --help options are specified
  if (argc == 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
    show_usage_and_exit();
  }

  options[0] = NULL;
  set_option(options, "document_root", ".");
  set_option(options, "listening_port", "8080");

  // Update config based on command line arguments
  process_command_line_arguments(argv, options);

  // Make sure we have absolute paths for files and directories
  // https://github.com/valenok/mongoose/issues/181
  set_absolute_path(options, "document_root", argv[0]);
  set_absolute_path(options, "put_delete_auth_file", argv[0]);
  set_absolute_path(options, "cgi_interpreter", argv[0]);
  set_absolute_path(options, "access_log_file", argv[0]);
  set_absolute_path(options, "error_log_file", argv[0]);
  set_absolute_path(options, "global_auth_file", argv[0]);
  set_absolute_path(options, "ssl_certificate", argv[0]);

  // Make extra verification for certain options
  verify_existence(options, "document_root", 1);
  verify_existence(options, "cgi_interpreter", 0);
  verify_existence(options, "ssl_certificate", 0);

  for (i = 0; options[i] != NULL; i += 2) {
    const char *msg = mg_set_option(server, options[i], options[i + 1]);
    if (msg != NULL) die("Failed to set option [%s]: %s", options[i], msg);
    free(options[i]);
    free(options[i + 1]);
  }

  // Change current working directory to document root. This way,
  // scripts can use relative paths.
  chdir(mg_get_option(server, "document_root"));

  // Add an ability to pass listening socket to mongoose
  {
    const char *env = getenv("MONGOOSE_LISTENING_SOCKET");
    if (env != NULL && atoi(env) > 0 ) {
      mg_set_listening_socket(server, atoi(env));
    }
  }

  // Setup signal handler: quit on Ctrl-C
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
}

static void *serving_thread_func(void *param) {
  struct mg_server *srv = (struct mg_server *) param;
  while (exit_flag == 0) {
    mg_poll_server(srv, 1000);
  }
  return NULL;
}

enum {
  ID_ICON = 100, ID_QUIT, ID_SETTINGS, ID_SEPARATOR, ID_INSTALL_SERVICE,
  ID_REMOVE_SERVICE, ID_STATIC, ID_GROUP, ID_SAVE, ID_RESET_DEFAULTS,
  ID_STATUS, ID_CONNECT,

  // All dynamically created text boxes for options have IDs starting from
  // ID_CONTROLS, incremented by one.
  ID_CONTROLS = 200,

  // Text boxes for files have "..." buttons to open file browser. These
  // buttons have IDs that are ID_FILE_BUTTONS_DELTA higher than associated
  // text box ID.
  ID_FILE_BUTTONS_DELTA = 1000
};
static HICON hIcon;
static HANDLE hThread;  // Serving thread
static SERVICE_STATUS ss;
static SERVICE_STATUS_HANDLE hStatus;
static const wchar_t *service_name = L"Mongoose";
static NOTIFYICONDATA TrayIcon;

static void WINAPI ControlHandler(DWORD code) {
  ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  ss.dwServiceType = SERVICE_WIN32;
  ss.dwWin32ExitCode = NO_ERROR;
  ss.dwCurrentState = SERVICE_RUNNING;

  if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN) {
    ss.dwCurrentState = SERVICE_STOPPED;
  }
  SetServiceStatus(hStatus, &ss);
}

static void WINAPI ServiceMain(void) {
  hStatus = RegisterServiceCtrlHandlerW(service_name, ControlHandler);
  ControlHandler(SERVICE_CONTROL_INTERROGATE);

  while (ss.dwCurrentState == SERVICE_RUNNING) {
    Sleep(1000);
  }
  mg_destroy_server(&server);
}

static void show_error(void) {
  char buf[256];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buf, sizeof(buf), NULL);
  MessageBox(NULL, buf, "Error", MB_OK);
}

static void *align(void *ptr, DWORD alig) {
  ULONG ul = (ULONG) ptr;
  ul += alig;
  ul &= ~alig;
  return ((void *) ul);
}

static int is_boolean_option(const char *option_name) {
  return !strcmp(option_name, "enable_directory_listing") ||
    !strcmp(option_name, "enable_keep_alive");
}

static int is_filename_option(const char *option_name) {
  return !strcmp(option_name, "cgi_interpreter") ||
    !strcmp(option_name, "global_auth_file") ||
    !strcmp(option_name, "dav_auth_file") ||
    !strcmp(option_name, "access_log_file") ||
    !strcmp(option_name, "error_log_file") ||
    !strcmp(option_name, "ssl_certificate");
}

static int is_directory_option(const char *option_name) {
  return !strcmp(option_name, "document_root");
}

static int is_numeric_options(const char *option_name) {
  return !strcmp(option_name, "num_threads");
}

static void save_config(HWND hDlg, FILE *fp) {
  char value[2000];
  const char **options, *name, *default_value;
  int i, id;

  fprintf(fp, "%s", config_file_top_comment);
  options = mg_get_valid_option_names();
  for (i = 0; options[i * 2] != NULL; i++) {
    name = options[i * 2];
    id = ID_CONTROLS + i;
    if (is_boolean_option(name)) {
      snprintf(value, sizeof(value), "%s",
               IsDlgButtonChecked(hDlg, id) ? "yes" : "no");
    } else {
      GetDlgItemText(hDlg, id, value, sizeof(value));
    }
    default_value = options[i * 2 + 1] == NULL ? "" : options[i * 2 + 1];
    // If value is the same as default, skip it
    if (strcmp(value, default_value) != 0) {
      fprintf(fp, "%s %s\n", name, value);
    }
  }
}

static BOOL CALLBACK DlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lP) {
  FILE *fp;
  int i;
  const char *name, *value, **options = mg_get_valid_option_names();

  switch (msg) {
    case WM_CLOSE:
      DestroyWindow(hDlg);
      break;

    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case ID_SAVE:
          EnableWindow(GetDlgItem(hDlg, ID_SAVE), FALSE);
          if ((fp = fopen(config_file, "w+")) != NULL) {
            save_config(hDlg, fp);
            fclose(fp);
            TerminateThread(hThread, 0);
            mg_destroy_server(&server);
            start_mongoose(my_argc, my_argv);
            mg_start_thread(serving_thread_func, server);
          }
          EnableWindow(GetDlgItem(hDlg, ID_SAVE), TRUE);
          break;
        case ID_RESET_DEFAULTS:
          for (i = 0; options[i * 2] != NULL; i++) {
            name = options[i * 2];
            value = options[i * 2 + 1] == NULL ? "" : options[i * 2 + 1];
            if (is_boolean_option(name)) {
              CheckDlgButton(hDlg, ID_CONTROLS + i, !strcmp(value, "yes") ?
                             BST_CHECKED : BST_UNCHECKED);
            } else {
              SetWindowText(GetDlgItem(hDlg, ID_CONTROLS + i), value);
            }
          }
          break;
      }

      for (i = 0; options[i * 2] != NULL; i++) {
        name = options[i * 2];
        if ((is_filename_option(name) || is_directory_option(name)) &&
            LOWORD(wParam) == ID_CONTROLS + i + ID_FILE_BUTTONS_DELTA) {
          OPENFILENAME of;
          BROWSEINFO bi;
          char path[PATH_MAX] = "";

          memset(&of, 0, sizeof(of));
          of.lStructSize = sizeof(of);
          of.hwndOwner = (HWND) hDlg;
          of.lpstrFile = path;
          of.nMaxFile = sizeof(path);
          of.lpstrInitialDir = mg_get_option(server, "document_root");
          of.Flags = OFN_CREATEPROMPT | OFN_NOCHANGEDIR;

          memset(&bi, 0, sizeof(bi));
          bi.hwndOwner = (HWND) hDlg;
          bi.lpszTitle = "Choose WWW root directory:";
          bi.ulFlags = BIF_RETURNONLYFSDIRS;

          if (is_directory_option(name)) {
            SHGetPathFromIDList(SHBrowseForFolder(&bi), path);
          } else {
            GetOpenFileName(&of);
          }

          if (path[0] != '\0') {
            SetWindowText(GetDlgItem(hDlg, ID_CONTROLS + i), path);
          }
        }
      }

      break;

    case WM_INITDIALOG:
      SendMessage(hDlg, WM_SETICON,(WPARAM) ICON_SMALL, (LPARAM) hIcon);
      SendMessage(hDlg, WM_SETICON,(WPARAM) ICON_BIG, (LPARAM) hIcon);
      SetWindowText(hDlg, "Mongoose settings");
      SetFocus(GetDlgItem(hDlg, ID_SAVE));
      for (i = 0; options[i * 2] != NULL; i++) {
        name = options[i * 2];
        value = mg_get_option(server, name);
        if (is_boolean_option(name)) {
          CheckDlgButton(hDlg, ID_CONTROLS + i, !strcmp(value, "yes") ?
                         BST_CHECKED : BST_UNCHECKED);
        } else {
          SetDlgItemText(hDlg, ID_CONTROLS + i, value == NULL ? "" : value);
        }
      }
      break;
    default:
      break;
  }

  return FALSE;
}

static void add_control(unsigned char **mem, DLGTEMPLATE *dia, WORD type,
                        DWORD id, DWORD style, WORD x, WORD y,
                        WORD cx, WORD cy, const char *caption) {
  DLGITEMTEMPLATE *tp;
  LPWORD p;

  dia->cdit++;

  *mem = align(*mem, 3);
  tp = (DLGITEMTEMPLATE *) *mem;

  tp->id = (WORD)id;
  tp->style = style;
  tp->dwExtendedStyle = 0;
  tp->x = x;
  tp->y = y;
  tp->cx = cx;
  tp->cy = cy;

  p = align(*mem + sizeof(*tp), 1);
  *p++ = 0xffff;
  *p++ = type;

  while (*caption != '\0') {
    *p++ = (WCHAR) *caption++;
  }
  *p++ = 0;
  p = align(p, 1);

  *p++ = 0;
  *mem = (unsigned char *) p;
}

static void show_settings_dialog() {
#define HEIGHT 15
#define WIDTH 400
#define LABEL_WIDTH 80

  unsigned char mem[4096], *p;
  const char **option_names, *long_option_name;
  DWORD style;
  DLGTEMPLATE *dia = (DLGTEMPLATE *) mem;
  WORD i, cl, x, y, width, nelems = 0;
  static int guard;

  static struct {
    DLGTEMPLATE template; // 18 bytes
    WORD menu, class;
    wchar_t caption[1];
    WORD fontsiz;
    wchar_t fontface[7];
  } dialog_header = {{WS_CAPTION | WS_POPUP | WS_SYSMENU | WS_VISIBLE |
    DS_SETFONT | WS_DLGFRAME, WS_EX_TOOLWINDOW, 0, 200, 200, WIDTH, 0},
    0, 0, L"", 8, L"Tahoma"};

  if (guard == 0) {
    guard++;
  } else {
    return;
  }

  (void) memset(mem, 0, sizeof(mem));
  (void) memcpy(mem, &dialog_header, sizeof(dialog_header));
  p = mem + sizeof(dialog_header);

  option_names = mg_get_valid_option_names();
  for (i = 0; option_names[i * 2] != NULL; i++) {
    long_option_name = option_names[i * 2];
    style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    x = 10 + (WIDTH / 2) * (nelems % 2);
    y = (nelems/2 + 1) * HEIGHT + 5;
    width = WIDTH / 2 - 20 - LABEL_WIDTH;
    if (is_numeric_options(long_option_name)) {
      style |= ES_NUMBER;
      cl = 0x81;
      style |= WS_BORDER | ES_AUTOHSCROLL;
    } else if (is_boolean_option(long_option_name)) {
      cl = 0x80;
      style |= BS_AUTOCHECKBOX;
    } else if (is_filename_option(long_option_name) ||
               is_directory_option(long_option_name)) {
      style |= WS_BORDER | ES_AUTOHSCROLL;
      width -= 20;
      cl = 0x81;
      add_control(&p, dia, 0x80,
                  ID_CONTROLS + i + ID_FILE_BUTTONS_DELTA,
                  WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  (WORD) (x + width + LABEL_WIDTH + 5),
                  y, 15, 12, "...");
    } else {
      cl = 0x81;
      style |= WS_BORDER | ES_AUTOHSCROLL;
    }
    add_control(&p, dia, 0x82, ID_STATIC, WS_VISIBLE | WS_CHILD,
                x, y, LABEL_WIDTH, HEIGHT, long_option_name);
    add_control(&p, dia, cl, ID_CONTROLS + i, style,
                (WORD) (x + LABEL_WIDTH), y, width, 12, "");
    nelems++;
  }

  y = (WORD) (((nelems + 1) / 2 + 1) * HEIGHT + 5);
  add_control(&p, dia, 0x80, ID_GROUP, WS_CHILD | WS_VISIBLE |
              BS_GROUPBOX, 5, 5, WIDTH - 10, y, " Settings ");
  y += 10;
  add_control(&p, dia, 0x80, ID_SAVE,
              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
              WIDTH - 70, y, 65, 12, "Save Settings");
  add_control(&p, dia, 0x80, ID_RESET_DEFAULTS,
              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
              WIDTH - 140, y, 65, 12, "Reset to defaults");
  add_control(&p, dia, 0x82, ID_STATIC,
              WS_CHILD | WS_VISIBLE | WS_DISABLED,
              5, y, 180, 12, static_user_name);

  dia->cy = ((nelems + 1) / 2 + 1) * HEIGHT + 30;
  DialogBoxIndirectParam(NULL, dia, NULL, DlgProc, (LPARAM) NULL);
  guard--;
}

static int manage_service(int action) {
  SC_HANDLE hSCM = NULL, hService = NULL;
  SERVICE_DESCRIPTION descr = {server_name};
  wchar_t wpath[PATH_MAX + 20];  // Path to executable plus magic argument
  int success = 1;

  if ((hSCM = OpenSCManager(NULL, NULL, action == ID_INSTALL_SERVICE ?
                            GENERIC_WRITE : GENERIC_READ)) == NULL) {
    success = 0;
    show_error();
  } else if (action == ID_INSTALL_SERVICE) {
    GetModuleFileNameW(NULL, wpath, sizeof(wpath) / sizeof(wpath[0]));
    wcsncat(wpath, L" --", sizeof(wpath) / sizeof(wpath[0]));
    hService = CreateServiceW(hSCM, service_name, service_name,
                              SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                              SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                              wpath, NULL, NULL, NULL, NULL, NULL);
    if (hService) {
      ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &descr);
    } else {
      show_error();
    }
  } else if (action == ID_REMOVE_SERVICE) {
    if ((hService = OpenServiceW(hSCM, service_name, DELETE)) == NULL ||
        !DeleteService(hService)) {
      show_error();
    }
  } else if ((hService = OpenServiceW(hSCM, service_name,
                                      SERVICE_QUERY_STATUS)) == NULL) {
    success = 0;
  }

  CloseServiceHandle(hService);
  CloseServiceHandle(hSCM);

  return success;
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam) {
  static SERVICE_TABLE_ENTRY service_table[] = {
    {server_name, (LPSERVICE_MAIN_FUNCTION) ServiceMain},
    {NULL, NULL}
  };
  int service_installed;
  char buf[200], *service_argv[] = {NULL, NULL};
  POINT pt;
  HMENU hMenu;
  static UINT s_uTaskbarRestart; // for taskbar creation

  switch (msg) {
    case WM_CREATE:
      if (my_argv[1] != NULL && !strcmp(my_argv[1], "--")) {
        service_argv[0] = my_argv[0];
        start_mongoose(1, service_argv);
        hThread = mg_start_thread(serving_thread_func, server);
        StartServiceCtrlDispatcher(service_table);
        exit(EXIT_SUCCESS);
      } else {
        start_mongoose(my_argc, my_argv);
        hThread = mg_start_thread(serving_thread_func, server);
        s_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));
      }
      break;
    case WM_COMMAND:
      switch (LOWORD(wParam)) {
        case ID_QUIT:
          TerminateThread(hThread, 0);
          mg_destroy_server(&server);
          Shell_NotifyIcon(NIM_DELETE, &TrayIcon);
          PostQuitMessage(0);
          return 0;
        case ID_SETTINGS:
          show_settings_dialog();
          break;
        case ID_INSTALL_SERVICE:
        case ID_REMOVE_SERVICE:
          manage_service(LOWORD(wParam));
          break;
        case ID_CONNECT:
          printf("[%s]\n", get_url_to_me(server));
          ShellExecute(NULL, "open", get_url_to_me(server),
                       NULL, NULL, SW_SHOW);
          break;
      }
      break;
    case WM_USER:
      switch (lParam) {
        case WM_RBUTTONUP:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          hMenu = CreatePopupMenu();
          AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_SEPARATOR, server_name);
          AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");
          service_installed = manage_service(0);
          snprintf(buf, sizeof(buf), "NT service: %s installed",
                   service_installed ? "" : "not");
          AppendMenu(hMenu, MF_STRING | MF_GRAYED, ID_SEPARATOR, buf);
          AppendMenu(hMenu, MF_STRING | (service_installed ? MF_GRAYED : 0),
                     ID_INSTALL_SERVICE, "Install service");
          AppendMenu(hMenu, MF_STRING | (!service_installed ? MF_GRAYED : 0),
                     ID_REMOVE_SERVICE, "Deinstall service");
          AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");
          snprintf(buf, sizeof(buf), "Start browser on port %s",
                   mg_get_option(server, "listening_port"));
          AppendMenu(hMenu, MF_STRING, ID_CONNECT, buf);
          AppendMenu(hMenu, MF_STRING, ID_SETTINGS, "Edit Settings");
          AppendMenu(hMenu, MF_SEPARATOR, ID_SEPARATOR, "");
          AppendMenu(hMenu, MF_STRING, ID_QUIT, "Exit");
          GetCursorPos(&pt);
          SetForegroundWindow(hWnd);
          TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, hWnd, NULL);
          PostMessage(hWnd, WM_NULL, 0, 0);
          DestroyMenu(hMenu);
          break;
      }
      break;
    case WM_CLOSE:
      TerminateThread(hThread, 0);
      mg_destroy_server(&server);
      Shell_NotifyIcon(NIM_DELETE, &TrayIcon);
      PostQuitMessage(0);
      return 0;  // We've just sent our own quit message, with proper hwnd.
    default:
      if (msg==s_uTaskbarRestart)
        Shell_NotifyIcon(NIM_ADD, &TrayIcon);
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdline, int show) {
  WNDCLASS cls;
  HWND hWnd;
  MSG msg;

  init_utf8_argc_argv();
  init_server_name();
  memset(&cls, 0, sizeof(cls));
  cls.lpfnWndProc = (WNDPROC) WindowProc;
  cls.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  cls.lpszClassName = server_name;

  RegisterClass(&cls);
  hWnd = CreateWindow(cls.lpszClassName, server_name, WS_OVERLAPPEDWINDOW,
                      0, 0, 0, 0, NULL, NULL, NULL, NULL);
  ShowWindow(hWnd, SW_HIDE);

  TrayIcon.cbSize = sizeof(TrayIcon);
  TrayIcon.uID = ID_ICON;
  TrayIcon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  TrayIcon.hIcon = hIcon = LoadImage(GetModuleHandle(NULL),
                                     MAKEINTRESOURCE(ID_ICON),
                                     IMAGE_ICON, 16, 16, 0);
  TrayIcon.hWnd = hWnd;
  snprintf(TrayIcon.szTip, sizeof(TrayIcon.szTip), "%s", server_name);
  TrayIcon.uCallbackMessage = WM_USER;
  Shell_NotifyIcon(NIM_ADD, &TrayIcon);

  while (GetMessage(&msg, hWnd, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Return the WM_QUIT value.
  return msg.wParam;
}

int main(int argc, char *argv[]) {
  init_utf8_argc_argv();
  argc = my_argc;
  argv = my_argv;
  init_server_name();
  start_mongoose(argc, argv);
  printf("%s serving [%s] on port %s\n",
         server_name, mg_get_option(server, "document_root"),
         mg_get_option(server, "listening_port"));
  fflush(stdout);  // Needed, Windows terminals might not be line-buffered
  while (exit_flag == 0) {
    mg_poll_server(server, 1000);
  }
  printf("Exiting on signal %d ...", exit_flag);
  fflush(stdout);
  mg_destroy_server(&server);
  printf("%s\n", " done.");

  return EXIT_SUCCESS;
}