package main

import (
	"log/syslog"
	"os"

	"github.com/sirupsen/logrus"
)

func init() {
	logger, err := syslog.New(syslog.LOG_ERR|syslog.LOG_USER, "google-web-oauth")
	if err == nil {
		logrus.SetOutput(logger)
	}
}
func main() {
	cli := &CLI{outStream: os.Stdout, errStream: os.Stderr}
	os.Exit(cli.Run(os.Args))
}
