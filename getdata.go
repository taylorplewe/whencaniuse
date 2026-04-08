package main

/*
#cgo CXXFLAGS: -std=c++23
#include <stdlib.h>
#include <parse.h>
*/
import "C"
import (
	"fmt"
	"html"
	"strings"
	"text/template"
	"unsafe"
)

func ReloadCaniuseData() {
	fmt.Println("Reloading data...")
	C.reload_caniuse_data()
}

func GetFeatureListHtmlFromSearchString(query string) string {
	// fmt.Println("getting results for ", query, "...")
	name := C.CString(strings.ToLower(query))
	defer C.free(unsafe.Pointer(name))
	res := C.search(name)
	defer C.free_search_results(res.data, res.len)

	var html strings.Builder
	for _, feature := range unsafe.Slice(res.data, res.len) {
		id := C.GoString(feature.id)
		title := encodeMdText(C.GoString(feature.title), false)
		description := encodeMdText(C.GoString(feature.description), false)
		t, _ := template.ParseFiles("html/feature-result-card.html")
		type Data struct {
			Id          string
			Title       string
			Description string
		}
		d := &Data{id, title, description}
		_ = t.Execute(&html, d)
	}

	return html.String()
}

// TODO: use C.GoStringN() instead of C.GoString()
func GetFeaturePageHtmlFromId(id string, clientId ClientId) (string, error) {
	cId := C.CString(id)
	defer C.free(unsafe.Pointer(cId))
	cFeature := C.get_feature_by_id(cId)
	if cFeature != nil {
		defer C.free_feature(cFeature)
		title := encodeMdText(C.GoString(cFeature.title), true)
		description := encodeMdText(C.GoString(cFeature.description), true)
		source := SourceKindCaniuse
		if strings.HasPrefix(id, "mdn-") {
			source = SourceKindMdn
		} else if strings.HasPrefix(id, "wf-") {
			source = SourceKindWebFeatures
		}
		var links []Link
		cLinks := unsafe.Slice(cFeature.links.data, cFeature.links.len)
		for _, cLink := range cLinks {
			display := "MDN"
			fmt.Println("link kind:", cLink.kind)
			switch cLink.kind {
			case 0:
				display = C.GoString(cLink.display)
			case 1:
				display = "MDN"
			default:
				display = "Specification"
			}
			links = append(links, Link{
				Display: display,
				Href:    C.GoString(cLink.href),
			})
		}
		featureIndex := uint32(cFeature.index)
		isInWatchlist := IsFeatureInClientsWatchlist(clientId, featureIndex)
		feature := &Feature{
			featureIndex,
			id,
			title,
			description,
			source,
			isInWatchlist, // TODO set this properly
			links,
		}
		watchlistHtml, confirmDialogFeatureListHtml := GetWatchlistHtmlFromClientId(clientId)
		var featureContentHtml strings.Builder
		var html strings.Builder
		_ = Templates[TemplateFeaturePage].Template.Execute(&featureContentHtml, feature)
		_ = Templates[TemplateIndex].Template.Execute(&html, IndexData{
			featureContentHtml.String(),
			watchlistHtml,
			confirmDialogFeatureListHtml,
		})
		return html.String(), nil
	} else {
		return "", fmt.Errorf("Could not find feature with ID '%s'", id)
	}
}

// Returns both the HTML for the features in the watchlist pane, and the confirmation modal.
func GetWatchlistFeaturesHtml(list *Watchlist) (string, string) {
	// C++ side just wants a list of u32 IDs
	var featureIndexes []uint32
	for _, mapping := range *list {
		for id := range mapping {
			featureIndexes = append(featureIndexes, uint32(id))
		}
	}

	cWatchlistTitles := C.get_watchlist_titles((*C.uint)(unsafe.Pointer(&featureIndexes[0])), C.int(len(featureIndexes)))
	defer C.free_watchlist_titles(cWatchlistTitles.data)
	var features []WatchlistFeature
	for i, title := range unsafe.Slice(cWatchlistTitles.data, cWatchlistTitles.len) {
		var desiredSupportThreshold uint
		for _, thresh := range (*list)[i] {
			desiredSupportThreshold = thresh
		}
		features = append(features, WatchlistFeature{
			featureIndexes[i],
			encodeMdText(C.GoString(title), false),
			desiredSupportThreshold,
		})
		// watchlistTitles = append(watchlistTitles, C.GoString(title))
	}

	var watchlistFeaturesHtml strings.Builder
	var confirmationModalFeaturesHtml strings.Builder

	_ = Templates[TemplateWatchlist].Template.Execute(&watchlistFeaturesHtml, &features)
	_ = Templates[TemplateConfirmDialogFeatureList].Template.Execute(&confirmationModalFeaturesHtml, &features)

	return watchlistFeaturesHtml.String(), confirmationModalFeaturesHtml.String()
}

func encodeMdText(text string, shouldRenderATags bool) string {
	htmlEscapedText := html.EscapeString(text)
	return encodeLinks(encodeCodeBlocks(htmlEscapedText), shouldRenderATags)
}

func encodeCodeBlocks(text string) string {
	isInCodeBlock := false
	var newText strings.Builder
	var codeBlockText strings.Builder
	for _, r := range text {
		if r == '`' {
			if isInCodeBlock {
				isInCodeBlock = false
				fmt.Fprintf(&newText, "<code>%s</code>", codeBlockText.String())
				codeBlockText.Reset()
			} else {
				isInCodeBlock = true
			}
		} else {
			if isInCodeBlock {
				codeBlockText.WriteRune(r)
			} else {
				newText.WriteRune(r)
			}
		}
	}
	if isInCodeBlock {
		fmt.Fprintf(&newText, "`%s", codeBlockText.String())
	}
	return newText.String()
}

func encodeLinks(text string, shouldRenderATags bool) string {
	type State uint
	const (
		StateNormal State = iota
		StateInLinkText
		StateExpectingLinkHref
		StateInLinkHref
	)
	state := StateNormal
	var newText strings.Builder
	var linkText strings.Builder
	var linkHref strings.Builder
	for _, r := range text {
		switch state {
		case StateNormal:
			switch r {
			case '[':
				state = StateInLinkText
			default:
				newText.WriteRune(r)
			}
		case StateInLinkText:
			switch r {
			case ']':
				state = StateExpectingLinkHref
			default:
				linkText.WriteRune(r)
			}
		case StateExpectingLinkHref:
			switch r {
			case '(':
				state = StateInLinkHref
			default:
				state = StateNormal
				fmt.Fprintf(&newText, "[%s]%c", linkText.String(), r)
				linkText.Reset()
			}
		case StateInLinkHref:
			switch r {
			case ')':
				state = StateNormal
				if shouldRenderATags {
					fmt.Fprintf(&newText, `<a href="%s" target="_blank">%s</a>`, linkHref.String(), linkText.String())
				} else {
					newText.WriteString(linkText.String())
				}
				linkText.Reset()
				linkHref.Reset()
			default:
				linkHref.WriteRune(r)
			}
		}
	}

	// flush if ended in unfinished parsing state
	switch state {
	case StateInLinkText:
		fmt.Fprintf(&newText, "[%s", linkText.String())
	case StateInLinkHref:
		fmt.Fprintf(&newText, "[%s](%s", linkText.String(), linkHref.String())
	case StateExpectingLinkHref:
		fmt.Fprintf(&newText, "[%s]", linkText.String())
	}

	return newText.String()
}
