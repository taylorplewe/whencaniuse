package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strings"

	"github.com/starfederation/datastar-go/datastar"
)

type FeatureSearchParams struct {
	SearchValue string `json:"searchValue"`
}

func HandleDatastarRequests(w http.ResponseWriter, req *http.Request) {
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	switch trimmedUrl {
	case "swappy":
		sse := datastar.NewSSE(w, req)
		sse.PatchElements(`<p id="apples">a p p l e s</p>`)
	case "feature-search":
		params := &FeatureSearchParams{}
		if err := getDatastarParams(req, &params); err != nil {
			w.WriteHeader(500)
			return
		}

		fmt.Println("search value:", params.SearchValue)

		percentage := GetSupportPercentageForFeature(params.SearchValue)

		sse := datastar.NewSSE(w, req)
		if percentage == 0.0 {
			sse.PatchElements(`<span id="result-percentage">Feature not found!</span>`)
		} else {
			sse.PatchElementf(`<span id="result-percentage">%f</span>`, percentage)
		}
	}
}

func getDatastarParams(req *http.Request, params any) error {
	dsParam := req.URL.Query().Get("datastar")
	if err := json.Unmarshal([]byte(dsParam), &params); err != nil {
		return errors.New("Could not unmarshal JSON into FeatureSearchParams!")
	}
	return nil
}
