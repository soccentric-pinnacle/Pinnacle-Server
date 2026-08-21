# Pinnacle-Server Build and Deployment

## Purpose and scope

Pinnacle-Server is the C++20 server and test application built with CMake/Ninja. It generates C++ protobuf/gRPC sources from sibling `Pinnacle-HAL/protos`, builds `pinnacle-server`, unit/integration tests when present, and supports native x86 builds, ARM64 client-oriented Docker builds, x86 Ubuntu Docker server builds, SSH deployment, runtime execution, static analysis, and Doxygen.

## 1. Prerequisites

- Linux host with GNU Make, CMake 3.22+, Ninja, a C++20 compiler, and `nproc`.
- System development packages discoverable by CMake: gRPC, Protobuf/protoc, `grpc_cpp_plugin`, Drogon, spdlog, yaml-cpp, GTest, OpenSSL, Argon2, and optionally prometheus-cpp. CMake also fetches cached `tomlplusplus` content when needed.
- A sibling `Pinnacle-HAL/protos` checkout is required for proto generation.
- Docker with ARM64 and AMD64 platform support for container targets.
- For deploy/run targets: `sshpass`, SSH/SCP connectivity, and target devices. Defaults are `192.168.1.3`, `192.168.1.13`, user/password `nvidia`/`nvidia`; override them and do not treat defaults as production credentials.

Run the prerequisite script before Docker work when present:

```bash
./docker/check-prereqs.sh
```

## 2. Setup

Inspect targets and choose a separate build directory:

```bash
make help
make configure
```

Native output is `build/native`; the Docker server output is `build/docker` inside the `pinnacle-dev` container. The CMake configure enables tests and exports compile commands.

## 3. Build and compile

Native build:

```bash
make configure
make build
```

Run the ARM64 Docker build path:

```bash
make docker-install       # conditional; may require sudo
make docker-build
```

Build the x86 Ubuntu 22.04 server container path:

```bash
make docker-image-server
make docker-build-server
```

Use `docker-shell` or `docker-shell-server` for interactive inspection. Docker targets mount the parent Pinnacle tree and use ccache volumes.

## 4. Test and lint

```bash
make test
make docker-test
make docker-test-server
```

Native and container tests invoke CTest with failure output. Quality/documentation targets are:

```bash
make format               # modifies C++ and CMake files
make cppcheck
make doc
```

`format`, `cppcheck`, and `doc` configure first; their required formatter/analyzer/documentation tools must be installed on the relevant host.

## 5. Packaging and artifacts

The root Server Makefile does not define a CPack package target. The main native artifact is `build/native/bin/pinnacle-server` plus generated libraries/protobuf outputs; Docker builds place outputs in the mounted `Pinnacle-Server/build/` tree. Installation metadata in `CMakeLists.txt` installs `pinnacle-server` to `bin`, but no root `make install` wrapper is defined. Use CMake installation deliberately and supply a target-specific prefix/service package if needed.

## 6. Deployment, install, and run

Deploy cross-built libraries and binaries to both configured targets:

```bash
make deploy-a IP_A=192.168.1.3 USER=serveruser PASS='<password>'
make deploy-b IP_B=192.168.1.13 USER=serveruser PASS='<password>'
make deploy
```

The Makefile's deployment helper clears `/tmp/acu-test` and copies `build/cross/lib` and `build/cross/bin`. Run all or one application remotely:

```bash
make run-a
make run-a APP_NAME arg1 arg2
make run-b APP_NAME arg1 arg2
```

Run native programs with the built library path:

```bash
make run
make run APP_NAME arg1 arg2
make list
```

The server-specific Docker build/test targets are:

```bash
make docker-build-server
make docker-test-server
```

No cloud provisioning, service registration, TLS secret deployment, or production process supervisor is implemented by this Makefile.

## 7. Cleanup

```bash
make clean
make docker-clean
make docker-clean-server
```

`clean` removes all local `build/`; Docker cleanup targets remove their respective mounted build directories and require their Docker images.

## 8. Hardware, cloud, and manual verification caveats

- CMake generation depends on sibling HAL protos and the generated contract must match Client/HAL/SDK versions.
- Unit/integration CTest coverage does not prove live device pairing, telemetry, gRPC streaming, authentication, database behavior, or attached hardware.
- Remote deployment uses `sshpass` and disables SSH host-key checking; assess this before any real rollout and avoid exposing passwords in command history.
- Validate service installation, runtime configuration, network/firewall/TLS, database or external service dependencies, client interoperability, and representative device traffic manually.
