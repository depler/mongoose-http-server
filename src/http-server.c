// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved

#include <signal.h>
#include "mongoose.h"

static const char *s_debug_level = "2";
static const char *s_root_dir = ".";  // Attention: avoid double-dots, `..` !
static const char *s_listening_address = "http://localhost:8000";
static const char *s_enable_hexdump = "no";
static const char* s_user = NULL;
static const char* s_pass = NULL;

static bool auth_user(struct mg_http_message* hm) 
{
    char user[256], pass[256];
    mg_http_creds(hm, user, sizeof(user), pass, sizeof(pass));

    if (s_user != NULL && strcmp(user, s_user) != 0)
        return false;

    if (s_pass != NULL && strcmp(pass, s_pass) != 0)
        return false;

    return true;
}

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo) {
  s_signo = signo;
}

// Event handler for the listening connection.
// Simply serve static files from `s_root_dir`
static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) 
{
  if (ev == MG_EV_HTTP_MSG) 
  {
    if (auth_user((struct mg_http_message*)ev_data))
    {
      struct mg_http_serve_opts opts = {0};
      opts.root_dir = s_root_dir;
      mg_http_serve_dir(c, ev_data, &opts);
    }
    else
    {
      mg_printf(c, "%s", "HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic\r\nContent-Length: 0\r\n\r\n");
    }
  }
  (void)fn_data;
}

static void usage(const char *prog) {
  fprintf(stderr,
          "Mongoose v.%s\n"
          "Usage: %s OPTIONS\n"
          "  -H yes|no     - enable traffic hexdump, default: '%s'\n"
          "  -d DIR        - directory to serve, default: '%s'\n"
          "  -u USER       - username to authenticate, default: <empty>\n"
          "  -p PASSWORD   - password to authenticate, default: <empty>\n"
          "  -l ADDR       - listening address, default: '%s'\n"
          "  -v LEVEL      - debug level, from 0 to 4, default: '%s'\n",
          MG_VERSION, prog, s_enable_hexdump, s_root_dir, s_listening_address,
          s_debug_level);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  struct mg_connection *c;
  int i;

  // Parse command-line flags
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      s_root_dir = argv[++i];
    } else if (strcmp(argv[i], "-u") == 0) {
      s_user = argv[++i];
    }else if (strcmp(argv[i], "-p") == 0) {
      s_pass = argv[++i];
    } else if (strcmp(argv[i], "-H") == 0) {
      s_enable_hexdump = argv[++i];
    } else if (strcmp(argv[i], "-l") == 0) {
      s_listening_address = argv[++i];
    } else if (strcmp(argv[i], "-v") == 0) {
      s_debug_level = argv[++i];
    } else {
      usage(argv[0]);
    }
  }

  // Initialise stuff
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  mg_log_set(s_debug_level);
  mg_mgr_init(&mgr);
  if ((c = mg_http_listen(&mgr, s_listening_address, cb, &mgr)) == NULL) {
    LOG(LL_ERROR, ("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
                   s_listening_address));
    exit(EXIT_FAILURE);
  }
  if (mg_casecmp(s_enable_hexdump, "yes") == 0) c->is_hexdumping = 1;

  // Start infinite event loop
  LOG(LL_INFO, ("Mongoose version : v%s", MG_VERSION));
  LOG(LL_INFO, ("Listening on     : %s", s_listening_address));
  LOG(LL_INFO, ("Web root         : [%s]", s_root_dir));
  while (s_signo == 0) mg_mgr_poll(&mgr, 1000);
  mg_mgr_free(&mgr);
  LOG(LL_INFO, ("Exiting on signal %d", s_signo));
  return 0;
}
