package main

import (
	"fmt"
	"text/template"
)

type TemplateKind uint
type TemplateMapping struct {
	Paths    []string
	Template *template.Template
}

const (
	TemplateIndex TemplateKind = iota
	TemplateFeaturePage
	TemplateWatchlist
	TemplateConfirmDialogFeatureList
)

var Templates map[TemplateKind]*TemplateMapping = map[TemplateKind]*TemplateMapping{
	TemplateIndex: {
		[]string{
			"html/index.html",
			"html/watchlist-pane.html",
			"html/watch-edit-dialog.html",
			"html/confirm-dialog.html",
		},
		nil,
	},
	TemplateFeaturePage: {
		[]string{
			"html/feature-page.html",
			"html/watch-dialog.html",
		},
		nil,
	},
	TemplateWatchlist: {
		[]string{
			"html/watchlist.html",
		},
		nil,
	},
	TemplateConfirmDialogFeatureList: {
		[]string{
			"html/confirm-dialog-feature-list.html",
		},
		nil,
	},
}

func InitTemplates() {
	var err error
	for _, mapping := range Templates {
		mapping.Template, err = template.ParseFiles(mapping.Paths...)
		if err != nil {
			fmt.Println("ERROR: could not parse", mapping.Paths)
			return
		}
	}
}
