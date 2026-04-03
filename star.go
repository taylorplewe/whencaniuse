package main

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"slices"
	"strings"
	"time"

	"github.com/starfederation/datastar-go/datastar"
)

func HandleDatastarRequests(w http.ResponseWriter, req *http.Request, url string) {
	before := time.Now()
	trimmedUrl := strings.TrimSuffix(strings.TrimPrefix(req.URL.Path, "/"), "/")

	switch trimmedUrl {
	case "watchlist-add":
		type WatchlistAddParams struct {
			ClientId         ClientId `json:"clientId"`
			FeatureIndex     uint     `json:"index"`
			SupportThreshold uint     `json:"thresh"`
		}
		params := &WatchlistAddParams{}
		if err := getDatastarCustomPayload(req, params); err != nil {
			fmt.Println("Could not get params from 'watchlist-add' request body!", err)
		}

		fmt.Println("parsed body", params)

		sse := datastar.NewSSE(w, req)
		var html string
		if params.ClientId == 0 {
			fmt.Println("new client!")
			watchlist := Watchlist{{params.FeatureIndex: params.SupportThreshold}}
			newId := AddNewClient(watchlist)
			sse.MarshalAndPatchSignals(map[string]ClientId{"clientId": newId})
			html = GetWatchlistFeaturesHtml(&watchlist)
		} else {
			client, exists := Clients[params.ClientId]
			if !exists {
				fmt.Printf("ERROR: client with ID %d not found!\n", params.ClientId)
				w.WriteHeader(400)
				w.Write([]byte("Client with your client ID was not found!"))
				return
			}
			client.Watchlist = append(client.Watchlist, map[uint]uint{params.FeatureIndex: params.SupportThreshold})
			html = GetWatchlistFeaturesHtml(&client.Watchlist)
		}
		sse.PatchElementf(`<ul id="watchlist-feature-list">%s</ul>`, html)
		// send back updated watchlist HTML
		PrintClients()
	case "watchlist-remove":
		type WatchlistRemoveParams struct {
			ClientId     ClientId `json:"clientId"`
			FeatureIndex uint     `json:"featureIndex"`
		}
		params := &WatchlistRemoveParams{}
		if err := getDatastarCustomPayload(req, params); err != nil {
			fmt.Println("Could not get params from 'watchlist-add' request body!", err)
		}
		client, exists := Clients[params.ClientId]
		if !exists {
			fmt.Printf("ERROR: client with ID %d not found!\n", params.ClientId)
			w.WriteHeader(400)
			w.Write([]byte("Client with your client ID was not found!"))
			return
		}
		client.Watchlist = slices.DeleteFunc(client.Watchlist, func(mapping map[uint]uint) bool {
			for id := range mapping {
				if id == params.FeatureIndex {
					return true
				}
			}
			return false
		})
		var html string
		if len(client.Watchlist) > 0 {
			html = GetWatchlistFeaturesHtml(&client.Watchlist)
		}
		sse := datastar.NewSSE(w, req)
		sse.PatchElementf(`<ul id="watchlist-feature-list">%s</ul>`, html)
	case "feature-search":
		type FeatureSearchParams struct {
			SearchValue string `json:"searchValue"`
		}
		sse := datastar.NewSSE(w, req)
		params := &FeatureSearchParams{}
		if err := datastar.ReadSignals(req, &params); err != nil {
			w.WriteHeader(400)
			fmt.Println("ERROR: could not unmarshall datastar signals into params object:", err)
			return
		}

		if len(params.SearchValue) == 0 {
			sse.PatchElements(`<ul id="feature-search-results"></ul>`)
			return
		}

		featureListHtml := GetFeatureListHtmlFromSearchString(params.SearchValue)

		sse.PatchElementf(`<ul id="feature-search-results">%s</ul>`, featureListHtml)
		fmt.Println("time took from request to response:", time.Since(before))
	case "submit":
		bodyBytes, err := io.ReadAll(req.Body)
		if err != nil {
			fmt.Println("ERROR processing submit:", err)
		}
		fmt.Println("submit request:", string(bodyBytes))
	}
}

func getDatastarCustomPayload(req *http.Request, params any) error {
	bodyBytes, err := io.ReadAll(req.Body)
	if err != nil {
		return err
	}
	if err = json.Unmarshal(bodyBytes, &params); err != nil {
		return err
	}
	return nil
}
