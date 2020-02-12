VERSION  := $(shell git tag -l --sort=-v:refname | head -1 | sed 's/v//g')
REVISION := $(shell git rev-parse --short HEAD)
INFO_COLOR=\033[1;34m
RESET=\033[0m
BOLD=\033[1m
BUILD=builds

ifeq ("$(shell uname)","Darwin")
GO ?= go
else
GO ?= GO111MODULE=on /usr/local/go/bin/go
endif

CC=gcc
CFLAGS=-Wall -Wstrict-prototypes -Werror -fPIC -std=c99 -D_GNU_SOURCE
LD_SONAME=-Wl,-soname,pam_google_web_oauth.so.2
LIBRARY=pam_google_web_oauth.so.2.0

TEST ?= $(shell $(GO) list ./... | grep -v -e vendor -e keys -e tmp)

.PHONY: clean

clean:
	rm  -rf ./builds

ssh_container:
	docker build -f SSHDockerfile -t ssh .
	docker run --privileged \
	  -v `pwd`/builds/pam_google_web_oauth.so.2.0:/lib/x86_64-linux-gnu/security/google-web-oauth.so \
	  -v `pwd`/builds/google-web-oauth:/usr/bin/google-web-oauth \
	  -v `pwd`/misc/secret.json:/etc/google-web-oauth/client_secret.json \
	  -v $(HOME)/.ssh/id_rsa.pub:/root/.ssh/authorized_keys \
	  -p 10022:22 ssh

linux_build:
	docker run -v `pwd`:/go/src/github.com/pyama86/google-web-oauth -v $(GOPATH):/go -w /go/src/github.com/pyama86/google-web-oauth golang:latest make build

linux_pam_build:
	docker build -t pam -f dockerfiles/Dockerfile.ubuntu16 .
	docker run -v `pwd`:/go/src/github.com/pyama86/google-web-oauth -v $(GOPATH):/go -w /go/src/github.com/pyama86/google-web-oauth pam make pam_build

build:
	$(GO) build -o builds/google-web-oauth -ldflags "-s -w -X main.Version=$(VERSION)-$(REVISION)"

pam_build: clean
	mkdir $(BUILD)
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Building nss_stns$(RESET)"
	$(CC) $(CFLAGS) -c pam/pam.c -o $(BUILD)/pam.o
	$(CC) $(LDFLAGS) -shared $(LD_SONAME) -o builds/$(LIBRARY) \
		$(BUILD)/pam.o

pam_test: pam_build
	$(CC) -rdynamic -o builds/pam_unittest tests/pam_unittest.c -ldl
	./builds/pam_unittest

deps:
	go get -u golang.org/x/lint/golint

git-semv:
	brew tap linyows/git-semv
	brew install git-semv

ci: unit_test lint
lint: deps
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Linting$(RESET)"
	golint -min_confidence 1.1 -set_exit_status $(TEST)

unit_test: ## Run test
	@echo "$(INFO_COLOR)==> $(RESET)$(BOLD)Testing$(RESET)"
	$(GO) test -v $(TEST) -timeout=30s -parallel=4 -coverprofile cover.out.tmp
	cat cover.out.tmp | grep -v -e "main.go" -e "cmd.go" -e "_mock.go" > cover.out
	$(GO) tool cover -func cover.out
	$(GO) test -race $(TEST)

linux_pam_test:
	docker build -t pam -f dockerfiles/Dockerfile.ubuntu16 .
	docker run -v `pwd`:/go/src/github.com/pyama86/google-web-oauth -v $(GOPATH):/go -w /go/src/github.com/pyama86/google-web-oauth pam make pam_test

release: goreleaser
	git semv patch --bump
	goreleaser --rm-dist
run:
	$(GO) run main.go version.go cli.go

github_release:
	git semv patch --bump
	ghr --replace v$(VERSION) builds/

docker:
	docker build -t pyama:google-web-oauth .
