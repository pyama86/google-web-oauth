VERSION  := $(shell git tag | tail -n1 | sed 's/v//g')
REVISION := $(shell git rev-parse --short HEAD)
INFO_COLOR=\033[1;34m
RESET=\033[0m
BOLD=\033[1m

ifeq ("$(shell uname)","Darwin")
GO ?= go
else
GO ?= GO111MODULE=on /usr/local/go/bin/go
endif


TEST ?= $(shell $(GO) list ./... | grep -v -e vendor -e keys -e tmp)
linux_build:
	docker run -v `pwd`:/go/src/github.com/pyama86/google-web-oauth -v $(GOPATH):/go -w /go/src/github.com/pyama86/google-web-oauth golang:latest make build
build:
	$(GO) build -o builds/google-web-oauth -ldflags "-s -w -X main.Version=$(VERSION)-$(REVISION)"

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
