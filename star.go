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

func HandleDatastarRequests(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	switch trimmedUrl {
	case "feature-search":
		before := time.Now()
		params := &FeatureSearchParams{}
		if err := getDatastarParams(req, &params); err != nil {
			w.WriteHeader(500)
			return
		}

		fmt.Println("search value:", params.SearchValue)

		featureListHtml := GetFeatureListHtmlFromSearchString(params.SearchValue)

		sse := datastar.NewSSE(w, req)
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
