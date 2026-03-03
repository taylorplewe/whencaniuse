package main

import (
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
)

var ShouldReloadCaniuseData chan os.Signal = make(chan os.Signal, 1)

func main() {
	httpServer := http.Server{
		Addr:    ":1999",
		Handler: http.HandlerFunc(serve),
	}

	// whenever a SIGUSR1 is received, reload the caniuse.json into the C++'s internal data buffer.
	// this happens regularly via a cronjob on this machine, immediately after downloading the newest version of the data.
	signal.Notify(ShouldReloadCaniuseData, syscall.SIGUSR1)
	go func() {
		for range ShouldReloadCaniuseData {
			ReloadCaniuseData()
		}
	}()
	ReloadCaniuseData()

	fmt.Println("Listening on port 1999...")
	httpServer.ListenAndServe()
}

func serve(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	fmt.Println()
	fmt.Printf("req.URL: '%s'\n", req.URL)
	fmt.Printf("req.URL trimmed: '%s'\n", trimmedUrl)

	if req.Header.Get("Datastar-Request") == "true" {
		HandleDatastarRequests(w, req)
		return
	}

	switch trimmedUrl {
	case "":
		http.ServeFile(w, req, "index.html")
	default:
		http.ServeFile(w, req, trimmedUrl)
	}
}
