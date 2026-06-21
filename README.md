# LO-RAT

Advanced Remote Access Toolkit (RAT) with modular C++ payload, Go C2 server, and C# operator GUI.

Designed for authorized red team engagements, penetration testing, and security research.

---


---

## Features

| Module | Description | Status |
|---|---|---|
| **Keylogger** | Global WH_KEYBOARD_LL hook with window tracking | ✅ Complete |
| **Screen Recorder** | GDI+ JPEG capture to frame archive | ✅ Complete |
| **Webcam Recorder** | MediaFoundation video capture | ✅ Complete |
| **Cookie Stealer** | Chrome/Edge SQLite + DPAPI decryption | ✅ Complete |
| **Password Stealer** | Browser vaults + Windows Credential Manager | ✅ Complete |
| **Live Screen** | MJPEG HTTP stream server | ✅ Complete |
| **WiFi Info** | SSID, BSSID, signal, profile extraction | ✅ Complete |
| **Network Spread** | SMB/ARP lateral movement (optional) | 🟡 Stub |
| **System Control** | Shutdown, restart, logoff | ✅ Complete |

---

## Prerequisites

### Development Host (Linux recommended)

| Tool | Version | Purpose |
|---|---|---|
| `mingw-w64` | 10.0+ | Cross-compile Windows payload |
| `go` | 1.21+ | C2 server |
| `dotnet` | 6.0+ | GUI panel |
| `openssl` | 3.0+ | TLS certificates |
| `make` | 4.3+ | Build automation |

```bash
# Debian/Ubuntu/Kali
sudo apt update
sudo apt install -y mingw-w64 golang-go dotnet-sdk-6.0 openssl make
```

Target Environment
```
Windows 10/11 (x64)
Visual C++ Redistributable 2015-2022 (usually pre-installed)
```
