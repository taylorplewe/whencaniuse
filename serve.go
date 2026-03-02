package main

import (
	"fmt"
	"net/http"
	"strings"

	"github.com/starfederation/datastar-go/datastar"
)

func main() {
	httpServer := http.Server{
		Addr:    ":1999",
		Handler: http.HandlerFunc(serve),
	}

	fmt.Println("Listening on port 1999...")
	httpServer.ListenAndServe()
}

func serve(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	fmt.Printf("req.URL: '%s'\n", req.URL)
	fmt.Printf("req.URL.Path: '%s'\n", req.URL.Path)
	fmt.Printf("req.URL trimmed: '%s'\n", trimmedUrl)

	switch trimmedUrl {
	case "":
		http.ServeFile(w, req, "index.html")
	case "swappy":
		sse := datastar.NewSSE(w, req)
		sse.PatchElements(`<p id="apples">a p p l e s</p>`)
	default:
		http.ServeFile(w, req, trimmedUrl)
	}
}
