#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <stdlib.h>
#include <string.h>
// debug
#include <stdio.h>
#include <syslog.h>

#ifdef sun
#define PAM_CONST
#else
#define PAM_CONST const
#endif
#define MAXBUF 1024
// source from: github.com/google/google-authenticator-libpam
static int converse(pam_handle_t *pamh, int nargs, PAM_CONST struct pam_message **message,
                    struct pam_response **response)
{
  struct pam_conv *conv;
  int retval = pam_get_item(pamh, PAM_CONV, (void *)&conv);
  if (retval != PAM_SUCCESS) {
    return retval;
  }
  return conv->conv(nargs, message, response, conv->appdata_ptr);
}
static char *request_pass(pam_handle_t *pamh, int echocode, PAM_CONST char *prompt)
{
  // Query user for verification code
  PAM_CONST struct pam_message msg = {.msg_style = echocode, .msg = prompt};
  PAM_CONST struct pam_message *msgs = &msg;
  struct pam_response *resp = NULL;
  int retval = converse(pamh, 1, &msgs, &resp);
  char *ret = NULL;
  if (retval != PAM_SUCCESS || resp == NULL || resp->resp == NULL || *resp->resp == '\000') {
    if (retval == PAM_SUCCESS && resp && resp->resp) {
      ret = resp->resp;
    }
  } else {
    ret = resp->resp;
  }

  // Deallocate temporary storage
  if (resp) {
    if (!ret) {
      free(resp->resp);
    }
    free(resp);
  }

  return ret;
}

int exec_cmd(const char *user, char *cmd, char *arg, char *res)
{
  FILE *fp;
  char *c;
  char buf[MAXBUF];
  if (arg != NULL) {
    c = malloc(strlen(cmd) + strlen(arg) + 2);
    sprintf(c, "%s %s", cmd, arg);
  } else {
    c = cmd;
  }

  sprintf(buf, "USER=%s", user);
  putenv(buf);

  if ((fp = popen(c, "r")) == NULL) {
    goto err;
  }

  int total_len = 0;
  int len = 0;

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    len = strlen(buf);
    strcpy(res + total_len, buf);
    total_len += len;
  }

  if (arg != NULL)
    free(c);

  return pclose(fp);
err:
  if (arg != NULL)
    free(c);
  return 1;
}

int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  const char *user;
  int retval;
  char buf[MAXBUF], res[MAXBUF];
  retval = pam_get_item(pamh, PAM_USER, (void *)&user);

  if (retval != PAM_SUCCESS) {
    return retval;
  }

  int ret = exec_cmd(user, "/usr/bin/google-web-oauth", "-only-url", res);
  if (ret != 0) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  char *code = NULL;
  code = request_pass(pamh, PAM_PROMPT_ECHO_OFF, res);
  if (!code) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  sprintf(buf, "-code %s", code);
  ret = exec_cmd(user, "/usr/bin/google-web-oauth", buf, res);
  if (ret != 0) {
    goto err;
  }

  return PAM_SUCCESS;
err:
  return PAM_AUTHINFO_UNAVAIL;
}

int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
  return PAM_SUCCESS;
}
