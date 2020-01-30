VERSION  := $(shell git tag | tail -n1 | sed 's/v//g')
REVISION := $(shell git rev-parse --short HEAD)
INFO_COLOR=\033[1;34m
RESET=\033[0m
BOLD=\033[1m
ifeq ("$(shell uname)","Darwin")
GO ?= GO111MODULE=on go
else
GO ?= GO111MODULE=on /usr/local/go/bin/go
endif

TEST ?= $(shell $(GO) list ./... | grep -v -e vendor -e keys -e tmp)
build:
	$(GO) build -o nke  -ldflags "-X main.Version=$(VERSION)-$(REVISION)"

deps:
	go get -u golang.org/x/lint/golint

git-semv:
	brew tap linyows/git-semv
	brew install git-semv

goreleaser:
	which goreleaser || (brew install goreleaser/tap/goreleaser && brew install goreleaser)

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
