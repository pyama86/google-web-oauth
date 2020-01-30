package main

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"net/url"
	"os"
	"os/user"
	"path/filepath"

	"github.com/sirupsen/logrus"
	"golang.org/x/oauth2"
	"golang.org/x/oauth2/google"
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
	)

	flags := flag.NewFlagSet(Name, flag.ContinueOnError)
	flags.SetOutput(cli.errStream)

	flags.StringVar(&config, "config", "/etc/pam-google-web-oauth/client_secret.json", "Config file path")
	flags.StringVar(&config, "c", "/etc/pam-google-web-oauth/client_secret.json", "Config file path(Short)")

	flags.BoolVar(&version, "version", false, "Print version information and quit.")

	// Parse commandline flag
	if err := flags.Parse(args[1:]); err != nil {
		return ExitCodeError
	}

	// Show version
	if version {
		fmt.Fprintf(cli.errStream, "%s version %s\n", Name, Version)
		return ExitCodeOK
	}
	if err := cli.run(config); err != nil {
		logrus.Error(err)
		return ExitCodeError
	}
	return ExitCodeOK

}
func (cli *CLI) run(config string) error {
	ctx := context.Background()
	b, err := ioutil.ReadFile(config)
	if err != nil {
		log.Fatalf("Unable to read client secret file: %s", config)
	}

	gconfig, err := google.ConfigFromJSON(b, "profile")
	if err != nil {
		fmt.Errorf("Unable to parse client secret file to config: %v", err)
	}
	err = auth(ctx, gconfig)
	if err != nil {
		return err
	}

	return nil

}

func auth(ctx context.Context, c *oauth2.Config) error {
	cacheFile, err := tokenCacheFile()
	if err != nil {
		return err
	}
	tok, err := tokenFromFile(cacheFile)
	if err != nil || !tok.Valid() {
		tok, err := getTokenFromWeb(c)
		if err != nil {
			return err
		}
		return saveToken(cacheFile, tok)
	}
	return nil
}

func getTokenFromWeb(c *oauth2.Config) (*oauth2.Token, error) {
	authURL := c.AuthCodeURL("state-token", oauth2.AccessTypeOffline)
	fmt.Printf("Go to the following link in your browser then type the "+
		"authorization code: \n\n%v\n\nPlease type code:", authURL)

	var code string
	if _, err := fmt.Scan(&code); err != nil {
		return nil, fmt.Errorf("Unable to read authorization code %v", err)
	}

	tok, err := c.Exchange(oauth2.NoContext, code)
	if err != nil {
		return nil, fmt.Errorf("Unable to retrieve token from web %v", err)
	}
	return tok, nil
}

func tokenCacheFile() (string, error) {
	userInfo, err := user.Lookup(os.Getenv("PAM_USER"))
	if err != nil {
		return "", err
	}
	tokenCacheDir := filepath.Join(userInfo.HomeDir, ".credentials")
	err = os.MkdirAll(tokenCacheDir, 0700)
	if err != nil {
		return "", err
	}
	return filepath.Join(tokenCacheDir, url.QueryEscape("google_oauth.json")), nil
}

func tokenFromFile(file string) (*oauth2.Token, error) {
	f, err := os.Open(file)
	if err != nil {
		return nil, err
	}
	t := &oauth2.Token{}
	err = json.NewDecoder(f).Decode(t)
	defer f.Close()
	return t, err
}

func saveToken(file string, token *oauth2.Token) error {
	fmt.Printf("Saving credential file to: %s\n", file)
	f, err := os.OpenFile(file, os.O_RDWR|os.O_CREATE|os.O_TRUNC, 0600)
	if err != nil {
		log.Fatalf("Unable to cache oauth token: %v", err)
	}
	defer f.Close()
	return json.NewEncoder(f).Encode(token)
}
