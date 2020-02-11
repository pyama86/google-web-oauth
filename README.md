# google-web-oauth
## Description
google-web-oauth is ssh authentication software.
this provides you with multi-factor authentication.


![demo](https://github.com/pyama86/google-web-oauth/blob/master/media/demo.gif)
## Usage
### USE PAM
for ubuntu

1. Get the oAuth client ID on google.
2. Please place the secret file to `/etc/google-web-oauth/client_secret.json`
3. set binary.
   - /lib/x86_64-linux-gnu/security/google-web-oauth.so
   - /usr/bin/google-web-oauth
4. Write the following in /etc/pam.d/sshd
```
auth    required google-web-oauth.so
#@include common-auth # must comment out.
```

5. Write the following in sshd_config and restart sshd process.

```
KbdInteractiveAuthentication yes
UsePAM yes
AuthenticationMethods publickey,keyboard-interactive
```

### USE SSH

> In this case, they skip ForceCommand when use ProxyCommand, it is vulnerable...

1. Get the oAuth client ID on google.
2. Please place the secret file to `/etc/google-web-oauth/client_secret.json`
3. set binary.
   - /usr/bin/google-web-oauth
4. Write the following in sshd_config and restart sshd process.

```
ForceCommand sudo SSH_CONNECTION="$SSH_CONNECTION" /usr/bin/google-web-oauth && eval ${SSH_ORIGINAL_COMMAND:-/bin/bash}
```

## blog
- [SSHログイン時に公開鍵認証とGoogle OAuthで多要素認証する](https://ten-snapon.com/archives/2306)

## Install

To install, use `go get`:

```bash
$ go get -d github.com/pyama86/google-web-oauth
```

## Contribution

1. Fork ([https://github.com/pyama86/google-web-oauth/fork](https://github.com/pyama86/google-web-oauth/fork))
1. Create a feature branch
1. Commit your changes
1. Rebase your local changes against the master branch
1. Run test suite with the `go test ./...` command and confirm that it passes
1. Run `gofmt -s`
1. Create a new Pull Request

## Author

[pyama86](https://github.com/pyama86)
