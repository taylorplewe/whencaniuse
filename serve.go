package main

import (
	"fmt"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
)

var ShouldReloadCaniuseData chan os.Signal = make(chan os.Signal, 1)

type SourceKind uint

const (
	SourceKindCaniuse SourceKind = iota
	SourceKindMdn
	SourceKindWebFeatures
)

type IndexData struct {
	MainHtml                     string
	WatchlistHtml                string
	ConfirmDialogFeatureListHtml string
}
type Link struct {
	Display string
	Href    string
}
type Feature struct {
	Index         uint32
	Id            string
	Title         string
	Description   string
	Source        SourceKind
	IsInWatchlist bool
	Links         []Link
}
type WatchlistFeature struct {
	Index                   uint32
	Title                   string
	DesiredSupportThreshold uint
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

	InitTemplates()

	fmt.Println("Listening on port 1999...")
	err := httpServer.ListenAndServe()
	if err != nil {
		fmt.Println("An error occurred:", err)
	}
}

func serve(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	fmt.Println()

	if req.Header.Get("Datastar-Request") == "true" {
		HandleDatastarRequests(w, req, trimmedUrl)
		return
	}

	switch trimmedUrl {
	case "":
		Templates[TemplateIndex].Template.Execute(w, IndexData{
			`<ul id="feature-search-results"></ul>`,
			"",
			"",
		})
		// http.ServeFile(w, req, "index.html")
	default:
		if strings.ContainsRune(trimmedUrl, '.') {
			http.ServeFile(w, req, trimmedUrl)
		} else {
			query := req.URL.Query()
			clientId, _ := strconv.Atoi(query.Get("cid"))
			html, err := GetFeaturePageHtmlFromId(trimmedUrl, ClientId(clientId))
			if err != nil {
				Templates[TemplateIndex].Template.Execute(w, fmt.Sprintf(`<p class="error">No feature found with ID '%s'`, IndexData{
					trimmedUrl,
					"",
					"",
				}))
			} else {
				w.Write([]byte(html))
			}
			// http.ServeFile(w, req, "index.html")
		}
	}
}
