package main

import (
	"fmt"
	"net/http"
)

func main() {
	httpServer := http.Server{
		Addr:    ":1999",
		Handler: http.HandlerFunc(serve),
	}

	fmt.Println("Listening on port 1999...")
	httpServer.ListenAndServe()
}

func serve(w http.ResponseWriter, req *http.Request) {
	fmt.Println("request received!")
	http.ServeFile(w, req, "index.html")
}
