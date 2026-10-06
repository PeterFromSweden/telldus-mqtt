# telldus-mqtt plan

Date: 2026-10-06. Replaces ARCHITECTURE_REVIEW.md (2025-11-30).

## Done
- #6 Old retained topics: cleared when a mapping is added (PR #9).
- #4 Device-1 vs Device-10 mapping: fixed in code. Issue closed.
- Tests: TellStick simulator and broker tests run with ctest (PR #7).
- Setup script for build dependencies (PR #7).
- Sensor offline time is configurable (`sensor-offline-seconds`).

## Next
1. #5 Robust set: now one resend after 1 s. Find out if telldus-core repeats. If not, add more resends with a small random delay. Make the count a config value.
2. CI: build and run ctest on GitHub Actions for each PR.
3. Exit codes: use one exit code per failure type (MQTT, telldusd, config). Update `analyze-logs.sh`.

## Maybe later
- MQTT TLS.
- Free device and sensor lists on exit (low value: process exits).

## Dropped
- Message queue when MQTT is down: restart recovers, sensors send again.
- State file: Home Assistant keeps state from retained topics.
- Signal quality, metrics, dashboard: no clear need.

## Keep
- Fail fast: `exit(1)` on fatal errors. systemd/procd and the watchdog restart the process.
