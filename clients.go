package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"slices"
	"sync"
	"time"
)

type Watchlist []map[uint]uint
type ClientId uint32
type Client struct {
	Id                    ClientId
	TimeOfFirstDataAccess time.Time
	Watchlist             Watchlist
}

var Clients map[ClientId]*Client = make(map[ClientId]*Client)
var ClientsMu sync.Mutex

func AddNewClient(watchlist Watchlist) ClientId {
	var newId ClientId
	for {
		var newIdBuf [8]byte
		rand.Read(newIdBuf[:])
		newId = ClientId(binary.LittleEndian.Uint64(newIdBuf[:]))
		if _, exists := Clients[newId]; !exists {
			break
		}
	}

	ClientsMu.Lock()
	Clients[newId] = &Client{newId, time.Now(), watchlist}
	ClientsMu.Unlock()

	return newId
}

func IsFeatureInClientsWatchlist(clientId ClientId, featureIndex uint32) bool {
	// first get their watchlist
	client, exists := Clients[clientId]
	if !exists {
		return false
	}

	// look for feature index
	return slices.IndexFunc(client.Watchlist, func(mapping map[uint]uint) bool {
		for index := range mapping {
			return index == uint(featureIndex)
		}
		return false // unreachable
	}) != -1
}

// Debug
func PrintClients() {
	fmt.Println("\nClients:")
	for _, client := range Clients {
		fmt.Println("ID:", client.Id, "\n Time of first data access:", client.TimeOfFirstDataAccess, "\n Watch list:", client.Watchlist)
	}
}
