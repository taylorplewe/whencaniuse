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
	"html/template"
	"strings"
	"unsafe"
)

func ReloadCaniuseData() {
	fmt.Println("Reloading data...")
	C.reload_caniuse_data()
}

func GetFeatureListHtmlFromSearchString(query string) string {
	// fmt.Println("getting results for ", query, "...")
	name := C.CString(query)
	defer C.free(unsafe.Pointer(name))
	res := C.search(name)
	defer C.free_search_results(res.features, res.len)

	var html strings.Builder
	for _, feature := range unsafe.Slice(res.features, res.len) {
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

func GetFeatureHtmlFromId(id string) (string, error) {
	c_id := C.CString(id)
	defer C.free(unsafe.Pointer(c_id))
	c_feature := C.get_feature_by_id(c_id)
	if c_feature != nil {
		defer C.free_feature(c_feature)
		title := encodeMdText(C.GoString(c_feature.title), true)
		description := encodeMdText(C.GoString(c_feature.description), true)
		feature := &Feature{
			Id:          id,
			Title:       title,
			Description: description,
		}
		var featureContentStr strings.Builder
		var html strings.Builder
		_ = templateFeaturePage.Execute(&featureContentStr, feature)
		_ = templateIndex.Execute(&html, featureContentStr.String())
		return html.String(), nil
	} else {
		return "", fmt.Errorf("Could not find feature with ID '%s'", id)
	}
}

func encodeMdText(text string, shouldEncodeLinks bool) string {
	htmlEscapedText := html.EscapeString(text)
	if shouldEncodeLinks {
		return encodeLinks(encodeCodeBlocks(htmlEscapedText))
	} else {
		return encodeLinksAsRegularText(encodeCodeBlocks(htmlEscapedText))
	}
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

func encodeLinksAsRegularText(text string) string {
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
				newText.WriteString(linkText.String())
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

// Leaving this here for later; links should NOT be rendered as <a> tags within the search result feature cards
func encodeLinks(text string) string {
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
				fmt.Fprintf(&newText, `<a href="%s" target="_blank">%s</a>`, linkHref.String(), linkText.String())
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
