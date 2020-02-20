package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"net/url"
	"os"
	"os/user"
	"path/filepath"
	"strings"
	"syscall"

	"github.com/sirupsen/logrus"
	"golang.org/x/crypto/ssh/terminal"
	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
	oauthapi "google.golang.org/api/oauth2/v2"
)

// Exit codes are int values that represent an exit code for a particular error.
const (
	ExitCodeOK    int = 0
	ExitCodeError int = 1 + iota
)

// CLI is the command line object
type CLI struct {
	// outStream and errStream are the stdout and stderr
	// to write message from the CLI.
	outStream, errStream io.Writer
}

// Run invokes the CLI with the given arguments.
func (cli *CLI) Run(args []string) int {
	var (
		config  string
		version bool
		onlyURL bool
		code    string
	)

	flags := flag.NewFlagSet(Name, flag.ContinueOnError)
	flags.SetOutput(cli.errStream)

	flags.StringVar(&config, "config", "/etc/google-web-oauth/client_secret.json", "Config file path")
	flags.StringVar(&config, "c", "/etc/google-web-oauth/client_secret.json", "Config file path(Short)")

	flags.BoolVar(&version, "version", false, "Print version information and quit.")

	flags.BoolVar(&onlyURL, "only-url", false, "only show url")
	flags.StringVar(&code, "code", "", "auth code from web")

	// Parse commandline flag
	if err := flags.Parse(args[1:]); err != nil {
		return ExitCodeError
	}

	// Show version
	if version {
		fmt.Fprintf(cli.errStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}
	if err := cli.run(config, onlyURL, []byte(code)); err != nil {
		logrus.Error(err)
		return ExitCodeError
	}
	return ExitCodeOK

}
func (cli *CLI) run(config string, onlyURL bool, code []byte) error {
	b, err := ioutil.ReadFile(config)
	if err != nil {
		return fmt.Errorf("Unable to read client secret file: %s", config)
	}

	c, err := google.ConfigFromJSON(b, "profile")
	if err != nil {
		return fmt.Errorf("Unable to parse client secret file to config: %v", err)
	}

	cacheFile, err := tokenCacheFile()
	if err != nil {
		return err
	}

	tok, err := tokenFromFile(cacheFile)
	if err != nil {
		goto web
	}

	if tok.OAuthToken == nil || tok.LastIP != lastIP() {
		goto web
	} else {
		client := oauth2.NewClient(oauth2.NoContext, c.TokenSource(oauth2.NoContext, tok.OAuthToken))
		svr, err := oauthapi.New(client)
		if err != nil {
			goto web
		}

		_, err = svr.Userinfo.Get().Do()
		if err != nil {
			goto web
		}
		fmt.Println("auth ok with cache token")
	}

	return nil
web:
	return getTokenFromWebAndSaveFile(c, cacheFile, onlyURL, code)
}

func getTokenFromWebAndSaveFile(c *oauth2.Config, cacheFile string, onlyURL bool, code []byte) error {
	var err error
	if code == nil || len(code) == 0 {
		authURL := c.AuthCodeURL("state-token", oauth2.AccessTypeOffline)
		fmt.Printf("Go to the following link in your browser then type the "+
			"authorization code: \n\n%v\n\nPlease type code:", authURL)

		if onlyURL {
			return nil
		}

		code, err = terminal.ReadPassword(int(syscall.Stdin))
		if err != nil {
			return fmt.Errorf("Unable to read authorization code %v", err)
		}
	}

	tok, err := c.Exchange(oauth2.NoContext, string(code))
	if err != nil {
		return fmt.Errorf("Unable to retrieve token from web %v", err)
	}
	return saveToken(cacheFile, tok)
}

func tokenCacheFile() (string, error) {
	uname := os.Getenv("USER")
	if os.Getenv("SUDO_USER") != "" {
		uname = os.Getenv("SUDO_USER")
	}
	userInfo, err := user.Lookup(uname)
	if err != nil {
		return "", fmt.Errorf("user lookup error %s %s", uname, err.Error())
	}
	tokenCacheDir := filepath.Join(userInfo.HomeDir, ".credentials")
	err = os.MkdirAll(tokenCacheDir, 0700)
	if err != nil {
		return "", err
	}
	return filepath.Join(tokenCacheDir, url.QueryEscape("google_oauth.json")), nil
}

type tokenCache struct {
	OAuthToken *oauth2.Token
	LastIP     string
}

func tokenFromFile(file string) (*tokenCache, error) {
	f, err := os.Open(file)
	if err != nil {
		return nil, err
	}

	tc := &tokenCache{}
	err = json.NewDecoder(f).Decode(tc)
	defer f.Close()
	return tc, err
}

func saveToken(file string, token *oauth2.Token) error {
	f, err := os.OpenFile(file, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		return err
	}

	tc := tokenCache{
		OAuthToken: token,
		LastIP:     lastIP(),
	}
	defer f.Close()
	return json.NewEncoder(f).Encode(tc)
}
func lastIP() string {
	return strings.Split(os.Getenv("SSH_CONNECTION"), " ")[0]
}
