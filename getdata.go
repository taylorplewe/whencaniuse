package main

/*
#cgo CXXFLAGS: -std=c++23
#cgo LDFLAGS: -l curl -l simdjson
#include <stdlib.h>
#include <parse.h>
*/
import "C"
import (
	"fmt"
	"strings"
	"unsafe"
)

func GetFeatureListHtmlFromSearchString(query string) string {
	fmt.Println("getting results for ", query, "...")
	name := C.CString(query)
	defer C.free(unsafe.Pointer(name))
	res := C.search(name)
	defer C.free_search_results(res.features, res.len)

	var html strings.Builder
	for _, feature := range unsafe.Slice(res.features, res.len) {
		id := C.GoString(feature.id)
		title := C.GoString(feature.title)
		description := C.GoString(feature.description)
		fmt.Fprintf(&html, `
			<li>(%s) <strong>%s</strong> - %s</li>
		`, id, title, description)
	}

	// cPercentage := C.search(name)
	// percentage := float32(cPercentage)
	return html.String()
}

func ReloadCaniuseData() {
	fmt.Println("Reloading data...")
	C.reload_caniuse_data()
}
