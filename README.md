# google-web-oauth
## Description
google-web-oauth is ssh authentication software.
this provides you with multi-factor authentication.


![demo](https://github.com/pyama86/google-web-oauth/blob/master/media/demo.gif)
## Usage

1. Get the oAuth client ID on google.
2. Please place the secret file to `/etc/google-web-oauth/client_secret.json`
3. Write the following in sshd_config and restart sshd process.

## blog
- [SSHログイン時に公開鍵認証とGoogle OAuthで多要素認証する](https://ten-snapon.com/archives/2306)

```
ForceCommand sudo /usr/bin/google-web-oauth && eval ${SSH_ORIGINAL_COMMAND:-/bin/bash}
```

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
