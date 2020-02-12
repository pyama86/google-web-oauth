#include <security/pam_ext.h>
#include <security/pam_modules.h>
#include <security/pam_modutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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

int exec_cmd(char *user_env, char *from_env, char *argv[])
{
  char *envp[] = {user_env, from_env, NULL};
  pid_t pid = fork();
  if (pid == -1) {
    return PAM_AUTHINFO_UNAVAIL;
  } else if (pid == 0) {
    execve(argv[0], argv, envp);
    exit(-1);
  } else {
    int status;
    if (wait(&status) == -1) {
      return PAM_AUTHINFO_UNAVAIL;
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      return PAM_SUCCESS;
    }
  }
  return PAM_AUTHINFO_UNAVAIL;
}

int popen_cmd(char *user_env, char *from_env, char *cmd, char *arg, char *res)
{
  FILE *fp;
  char *c;
  char buf[MAXBUF];
  putenv(user_env);
  putenv(from_env);

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
  char res[MAXBUF];
  const char *user;
  const char *from;
  char user_env[MAXBUF], from_env[MAXBUF];

  if (pam_get_user(pamh, &user, NULL) != PAM_SUCCESS || user == NULL || *user == '\0') {
    return PAM_USER_UNKNOWN;
  }

  if (pam_get_item(pamh, PAM_RHOST, (void *)&from) != PAM_SUCCESS) {
    return PAM_ABORT;
  }

  snprintf(user_env, MAXBUF, "USER=%s", user);
  snprintf(from_env, MAXBUF, "SSH_CONNECTION=%s", from);

  int ret = popen_cmd(user_env, from_env, "/usr/bin/google-web-oauth", "-only-url", res);
  if (ret != 0) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  char *code = NULL;
  code = request_pass(pamh, PAM_PROMPT_ECHO_OFF, res);
  if (!code) {
    return PAM_AUTHINFO_UNAVAIL;
  }

  char *arg[] = {"/usr/bin/google-web-oauth", "-code", code, NULL};
  ret = exec_cmd(user_env, from_env, arg);
  if (ret != PAM_SUCCESS) {
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
