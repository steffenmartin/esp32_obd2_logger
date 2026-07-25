# Architecture

## Purpose

`esp32_obd2_logger` autonomously sends configured OBD commands over a BLE
ELM327-compatible dongle and logs the raw command/response exchanges. The
firmware does not decode PIDs, assign units, or derive vehicle values; external
tools own all interpretation.

The web interface is an operational tool for selecting a dongle, monitoring
diagnostics, issuing manual troubleshooting commands, and downloading logs.

## Current structure

```text
main.cpp
 ├─ web_server       HTTP routes and response streaming
 │   └─ web_pages    browser HTML/CSS/JavaScript content
 ├─ ble_gateway      BLE scan, connection, FFE1 transport and notifications
 ├─ obd_poller       autonomous command schedule and timeout checks
 ├─ obd_response_assembler
 │                  combines fragmented BLE notifications into one exchange
 ├─ obd_log          bounded in-RAM raw exchange ring buffer
 └─ diagnostic_log   bounded human-readable diagnostic output
```

`main.cpp` is the composition root only: it connects Wi-Fi, starts BLE and the
web server, and gives each service a chance to run during `loop()`.

## Data flow

```text
poller / manual terminal
          │ raw command
          ▼
      BLE gateway ─────► ELM327 FFE1 characteristic
          ▲                       │ notification fragments
          │                       ▼
          └──────── response assembler
                            │ `>` prompt or timeout
                            ▼
                      RAM raw-log ring
                            │
                            ▼
                     /raw-log.csv export
```

Each `ObdLogRecord` contains a `millis()` timestamp, sequence number, raw
command, raw response, and `complete` flag. `complete` is true when an ELM327
prompt (`>`) ends the response. A response that does not finish within three
seconds is retained as incomplete instead of being discarded.

## Current defaults

- Poll command: `010C`
- Poll interval: one second
- Response timeout: three seconds
- RAM capacity: 32 exchanges, overwriting the oldest record when full

These values live in `src/app_config.h`; configuration persistence is a later
step. Only one command may be in flight, so autonomous and manual commands
cannot be associated with the same response.

## HTTP routes

| Route | Purpose |
| --- | --- |
| `/` | BLE-device discovery dashboard and log download link. |
| `/devices` | Device table fragment, refreshed by the dashboard. |
| `/terminal?addr=…` | Select a dongle and open the diagnostic terminal. |
| `/connect` | Establish the selected BLE connection; invoked when the terminal opens. |
| `/send?cmd=…` | Send a raw terminal command. |
| `/diagnostics` | Bounded diagnostic text output. |
| `/raw-log.csv` | Download the current raw RAM log. |

## Extension points

1. **Configuration:** Persist adapter selection, command list, intervals, and
   timeouts in NVS. The poller should then read a validated profile rather than
   its current fixed command.
2. **SD storage:** Add a file sink fed by `ObdLogRecord`. Prefer append-safe
   NDJSON for durable storage; retain CSV as a convenient export format.
3. **Upload:** Process completed SD files asynchronously once Wi-Fi is
   available. Upload retries must never block BLE callbacks or polling.
4. **Web UI:** The current pages are separated from route handling in
   `web_pages.cpp`. They can later be embedded as standalone assets and moved
   toward a JSON configuration/status API.

## Boundaries to preserve

- Keep raw OBD values raw; parsing belongs outside the device.
- Keep BLE notification callbacks short; do not write files or perform network
  uploads inside them.
- Keep storage and upload code independent of the BLE transport.
- Keep diagnostic text separate from data records so exports remain machine
  readable.
