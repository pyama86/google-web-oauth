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

int main(int argc, char *argv[]) {
  printf("Loading PAM module\n");
  pam_module = dlopen("./builds/pam_google_web_oauth.so.2.0", RTLD_NOW | RTLD_GLOBAL);
  if (pam_module == NULL) {
    fprintf(stderr, "dlopen(): %s\n", dlerror());
    exit(1);
  }

  int (*pam_sm_authenticate)(pam_handle_t *, int, int, const char **) =
      (int (*)(pam_handle_t *, int, int, const char **))
      dlsym(pam_module, "pam_sm_authenticate");

  assert(pam_sm_authenticate != NULL);

  printf("Done\n");
  return 0;
}
