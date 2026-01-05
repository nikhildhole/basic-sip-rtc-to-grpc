# SIP to gRPC Audio Gateway

A C++ server that bridges SIP/RTP calls to a gRPC audio stream.

## Features
- **SIP Server**: Listens on UDP 5060. Handles INVITE, ACK, BYE, OPTIONS.
- **RTP Receiver**: Listens on UDP 10000. Echos audio back and broadcasts to gRPC.
- **gRPC Service**: Streams received audio to subscribers via `SubscribeAudio`.
- **CI/CD**: GitHub Actions for multi-platform releases (Windows, Debian, macOS).

## Prerequisites
- CMake 3.15+
- C++17 Compiler
- gRPC and Protobuf installed (e.g., via `vcpkg` or system package manager).

## Build

### Windows (using vcpkg)
```powershell
vcpkg install grpc protobuf --triplet x64-windows
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### Linux (Ubuntu/Debian)
```bash
sudo apt install protobuf-compiler libprotobuf-dev libgrpc++-dev protobuf-compiler-grpc build-essential cmake
cmake -B build -S .
cmake --build build
```

## Usage
1. Run the server:
   ```bash
   ./build/sip_gateway (or sip_gateway.exe)
   ```
2. Connect a SIP client (e.g., Zoiper) to `sip:127.0.0.1:5060`.
3. Connect a gRPC client to `127.0.0.1:50051`.

## CI/CD & Versioning
- Commits with `feat:` trigger minor version bumps.
- Commits with `fix:` trigger patch version bumps.
- Tags are auto-created on push to `main` (v*) and `dev` (dev*).
- Artifacts (.zip, .deb, etc.) are attached to GitHub Releases.
