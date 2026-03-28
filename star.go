package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/starfederation/datastar-go/datastar"
)

type FeatureSearchParams struct {
	SearchValue string `json:"searchValue"`
}

func HandleDatastarRequests(w http.ResponseWriter, req *http.Request, url string) {
	before := time.Now()
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	switch trimmedUrl {
	case "feature-search":
		sse := datastar.NewSSE(w, req)
		params := &FeatureSearchParams{}
		if err := getDatastarParams(req, &params); err != nil {
			w.WriteHeader(500)
			return
		}

		if len(params.SearchValue) == 0 {
			sse.PatchElements(`<ul id="feature-search-results"></ul>`)
			return
		}

		featureListHtml := GetFeatureListHtmlFromSearchString(params.SearchValue)

		sse.PatchElementf(`<ul id="feature-search-results">%s</ul>`, featureListHtml)
		fmt.Println("time took from request to response:", time.Since(before))
	}
}

func getDatastarParams(req *http.Request, params any) error {
	dsParam := req.URL.Query().Get("datastar")
	if err := json.Unmarshal([]byte(dsParam), &params); err != nil {
		return errors.New("Could not unmarshal JSON into FeatureSearchParams!")
	}
	return nil
}
