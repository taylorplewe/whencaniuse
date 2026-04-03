package main

import (
	"fmt"
	"text/template"
)

type TemplateKind uint
type TemplateMapping struct {
	Path     string
	Template *template.Template
}

const (
	TemplateIndex TemplateKind = iota
	TemplateFeaturePage
	TemplateWatchlist
)

var Templates map[TemplateKind]*TemplateMapping = map[TemplateKind]*TemplateMapping{
	TemplateIndex:       {"html/index.html", nil},
	TemplateFeaturePage: {"html/feature-page.html", nil},
	TemplateWatchlist:   {"html/watchlist.html", nil},
}

func InitTemplates() {
	var err error
	for _, mapping := range Templates {
		mapping.Template, err = template.ParseFiles(mapping.Path)
		if err != nil {
			fmt.Println("ERROR: could not parse", mapping.Path)
			return
		}
	}
}
