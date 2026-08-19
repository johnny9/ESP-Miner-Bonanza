# ESP-Miner story map

Use this map to identify the feature area before reading the complete
specification index. Add a feature link when a durable contract exists.

| Outcome | Feature areas | Typical evidence |
|---|---|---|
| Keep features portable and independently owned | [feature ownership and dependency direction](architecture/feature-ownership/SPEC.md); [ASIC driver contract](asic/driver-contract/SPEC.md); [mining work pipeline](mining/work-pipeline/SPEC.md) | dependency checks, host contracts, fake adapters, integration/HIL checkpoints |
| Operate a compatible miner safely | [thermal, fan, and overheat control](device-safety/thermal-fan-overheat/SPEC.md); [on-device display lifecycle](device-ui/display-lifecycle/SPEC.md); [factory self-test](manufacturing/factory-self-test/SPEC.md) | component tests, device measurements, recovery checks |
| Connect and mine reliably | [Stratum client lifecycle and pool failover](mining/stratum-client-lifecycle/SPEC.md); [Stratum session and message safety](mining/stratum-session-safety/SPEC.md); [mining work pipeline](mining/work-pipeline/SPEC.md); [ASIC driver contract](asic/driver-contract/SPEC.md); [Bitmain backend](asic/bitmain-work-pipeline/SPEC.md); [hostname, mDNS, and Origin safety](networking/hostname-mdns-origin/SPEC.md) | component tests, protocol cases, ASIC vectors, failover/soak, runtime logs |
| Configure and observe through AxeOS | [settings and persistence](configuration/settings-persistence/SPEC.md); [dashboard statistics](axeos/dashboard-statistics/SPEC.md); [scoreboard](observability/share-scoreboard/SPEC.md); [swarm compatibility](axeos/swarm-compatibility/SPEC.md) | API tests, Angular tests, schema generation |
| Update and recover a device | factory images, ESP/AxeOS OTA, rollback, flashing guidance | artifact checks, negative paths, authorized device verification |
| Build and qualify releases | firmware build, frontend bundle, unit tests, configuration validation | CI-equivalent builds and tests |
