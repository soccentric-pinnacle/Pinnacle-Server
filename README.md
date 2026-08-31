# Pinnacle-Server

## Project Summary

Pinnacle-Server is the central C++ backend for Pinnacle hardware-validation runs.

`cmd/pinnacle-server/main.cpp` starts the service. `src` and `internal` organize:

- Configuration, logging, and database access.
- Migration execution, repositories, services, and orchestration.
- REST controllers and gRPC servers.

CMake generates protobuf/gRPC sources, links Drogon, spdlog, OpenSSL, YAML, Argon2, and database dependencies, and builds the server plus unit and integration executables.

`ClientServiceImpl` handles Client uplink and streamed run/result traffic. `HalServiceImpl` exposes direct HAL operations. REST routes serve the dashboard and external tooling, with SSE paths for live updates.

Migrations cover devices, runs, results, logs, OTA, audit records, notifications, alerts, application settings, and result fields.

Configuration uses `etc/pinnacle/server.toml` and `.env.example`; Docker Compose, devcontainer, local-install, and dependency documents describe supported environments.

CMake discovers GoogleTest unit and integration tests and retains a compatibility `config-test`. The checked-in `AUDIT.md` reports 65 CTest entries, 64 passing and one migration test skipped when `PINNACLE_TEST_DSN` is absent, plus 63 unit tests passing. It identifies live REST/SSE and persistence-invariant validation as environment-dependent.

The server has a broad implemented surface, but REST mode normalization handles wire strings and aliases rather than being a numeric HAL enum map. Database persistence differs between server-launched runs and the Client-side HAL path.

These are limitations, not reasons to overstate verification. Run the native build and CTest. Then exercise REST, SSE, gRPC, migrations, authentication, and database behavior with isolated services and a real Client.

## Feature Status

| # | Name / Description | Implemented? | Test Coverage? | Manual Test |
|---:|---|---|---|---|
| 1 | C++ server startup, configuration, and logging | Yes | Native build and compatibility config test are wired | [ ] Manual verification required |
| 2 | REST API controllers and SSE streams | Yes | Unit/integration targets exist; live stream emission is deferred | [ ] Manual verification required |
| 3 | Bidirectional Client gRPC and HAL services | Yes | Backpressure and service tests exist; real Client link is unverified | [ ] Manual verification required |
| 4 | Run orchestration, results, logs, and repositories | Yes | Service/repository unit coverage exists; deployment persistence needs testing | [ ] Manual verification required |
| 5 | SQL migrations and database configuration | Yes | Migration runner skips without `PINNACLE_TEST_DSN` | [ ] Manual verification required |
| 6 | OTA, alerts, notifications, settings, and audit records | Yes | Schema and service code exist; workflow coverage is partial | [ ] Manual verification required |
| 7 | Docker, devcontainer, local install, and CI paths | Partial | Definitions and workflows exist; environment parity is unverified | [ ] Manual verification required |

## Tests and Commands

```bash
make build          # Docker x86 build (server image)
make test           # ctest inside Docker
```

## Limitations

- The baseline includes one skipped DSN-dependent migration test.
- Live REST/SSE, migrations, authentication, and Client/Server interoperability require configured services.
