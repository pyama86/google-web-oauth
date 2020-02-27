#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void *pam_module;

static const char *get_error_msg(void) {
  const char *(*get_error_msg)(void) =
    (const char *(*)(void))dlsym(pam_module, "get_error_msg");
  const char* msg = get_error_msg ? get_error_msg() : "";
  const char* p = strrchr(msg, '\n');
  if (p) {
    msg = p+1;
  }
  return msg;
}

static void print_diagnostics(int signo) {
  if (*get_error_msg()) {
    fprintf(stderr, "%s\n", get_error_msg());
  }
  _exit(1);
}

static int converse(pam_handle_t *pamh, int nargs, const struct pam_message **message,
                    struct pam_response **response)
{
  struct pam_conv *conv;
  int retval = pam_get_item(pamh, PAM_CONV, (void *)&conv);
  if (retval != PAM_SUCCESS) {
    return retval;
  }
  return conv->conv(nargs, message, response, conv->appdata_ptr);
}

int pam_get_user(pam_handle_t *pamh, const char **user, const char *prompt) {
  return pam_get_item(pamh, PAM_USER, (void *)user);
}

int pam_get_item(const pam_handle_t *pamh, int item_type, const void **item) {
  switch (item_type) {
    case PAM_SERVICE: {
      static const char service[] = "google-web-oauth-pam-test";
      *item = service;
      return PAM_SUCCESS;
    }
    case PAM_USER: {
      char *user = getenv("USER");
      *item = user;
      return PAM_SUCCESS;
    }
    case PAM_CONV: {
      static struct pam_conv conv = { .conv = converse }, *p_conv = &conv;
      *item = p_conv;
      return PAM_SUCCESS;
    }
    default:
      return PAM_BAD_ITEM;
  }
}

int main(int argc, char *argv[]) {
  printf("Loading PAM module\n");
  pam_module = dlopen("./builds/pam_google_web_oauth.so.2.0", RTLD_NOW | RTLD_GLOBAL);
  if (pam_module == NULL) {
    fprintf(stderr, "dlopen(): %s\n", dlerror());
    exit(1);
  }

  signal(SIGABRT, print_diagnostics);

  int (*pam_sm_authenticate)(pam_handle_t *, int, int, const char **) =
      (int (*)(pam_handle_t *, int, int, const char **))
      dlsym(pam_module, "pam_sm_authenticate");

  assert(pam_sm_authenticate != NULL);

  printf("Done\n");
  return 0;
}
