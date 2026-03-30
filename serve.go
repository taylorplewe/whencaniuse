package main

import (
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"text/template"
)

var ShouldReloadCaniuseData chan os.Signal = make(chan os.Signal, 1)

var templateIndex *template.Template
var templateFeaturePage *template.Template

type SourceKind uint

const (
	SourceKindCaniuse SourceKind = iota
	SourceKindMdn
	SourceKindWebFeatures
)

type Feature struct {
	Id          string
	Title       string
	Description string
	Source      SourceKind
}

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

	var err error
	templateIndex, err = template.ParseFiles("html/index.html")
	if err != nil {
		fmt.Println("could not parse html/index.html")
		return
	}
	templateFeaturePage, err = template.ParseFiles("html/feature-page.html")
	if err != nil {
		fmt.Println("could not parse html/index.html")
		return
	}

	fmt.Println("Listening on port 1999...")
	err = httpServer.ListenAndServe()
	if err != nil {
		fmt.Println("An error occurred:", err)
	}
}

func serve(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	fmt.Println()
	// fmt.Printf("req.URL: '%s'\n", req.URL)
	// fmt.Printf("req.URL trimmed: '%s'\n", trimmedUrl)

	if req.Header.Get("Datastar-Request") == "true" {
		HandleDatastarRequests(w, req, trimmedUrl)
		return
	}

	switch trimmedUrl {
	case "":
		templateIndex.Execute(w, `<ul id="feature-search-results"></ul>`)
		// http.ServeFile(w, req, "index.html")
	default:
		if strings.ContainsRune(trimmedUrl, '.') {
			http.ServeFile(w, req, trimmedUrl)
		} else {
			html, err := GetFeatureHtmlFromId(trimmedUrl)
			if err != nil {
				templateIndex.Execute(w, fmt.Sprintf(`<p class="error">No feature found with ID '%s'`, trimmedUrl))
			} else {
				w.Write([]byte(html))
			}
			// http.ServeFile(w, req, "index.html")
		}
	}
}
