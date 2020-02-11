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

int exec_cmd(pam_handle_t *pamh, char *cmd, char *arg, char *res)
{
  const char *user;
  const void *void_from = NULL;
  const char *from;
  FILE *fp;
  char *c;
  char user_env[MAXBUF], host_env[MAXBUF];
  char buf[MAXBUF];
  if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || user == NULL || *user == '\0') {
    return PAM_USER_UNKNOWN;
  }
  sprintf(user_env, "USER=%s", user);
  putenv(user_env);

  if (pam_get_item(pamh, PAM_RHOST, &void_from) != PAM_SUCCESS) {
    return PAM_ABORT;
  }

  from = void_from;
  sprintf(host_env, "SSH_CONNECTION=%s", from);
  putenv(host_env);

  if (arg != NULL) {
    c = malloc(strlen(cmd) + strlen(arg) + 2);
    sprintf(c, "%s %s", cmd, arg);
  } else {
    c = cmd;
  }

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
  char buf[MAXBUF], res[MAXBUF];
  int ret = exec_cmd(pamh, "/usr/bin/google-web-oauth", "-only-url", res);

  if (ret != 0) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  char *code = NULL;
  code = request_pass(pamh, PAM_PROMPT_ECHO_OFF, res);
  if (!code) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  sprintf(buf, "-code %s", code);
  ret = exec_cmd(pamh, "/usr/bin/google-web-oauth", buf, res);
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
