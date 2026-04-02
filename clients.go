package main

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
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

// Debug
func PrintClients() {
	fmt.Println("\nClients:")
	for _, client := range Clients {
		fmt.Println("ID:", client.Id, "\n Time of first data access:", client.TimeOfFirstDataAccess, "\n Watch list:", client.Watchlist)
	}
}
