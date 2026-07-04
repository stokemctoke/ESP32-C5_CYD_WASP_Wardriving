# Security Policy

## Supported Versions

Security fixes are applied to the latest code on the `master` branch.

| Version | Supported          |
| ------- | ------------------ |
| latest on `master` | :white_check_mark: |
| older tags / forks | :x:                |

## Reporting a Vulnerability

Please report security vulnerabilities via [GitHub Issues](https://github.com/stokemctoke/ESP32-C5_CYD_WASP_Wardriving/issues).

- Open a new issue and prefix the title with `[Security]`.
- Describe the issue, affected components, and steps to reproduce.
- Do not include live credentials, API tokens, or personal data in the report.

We will acknowledge receipt and work toward a fix on `master`. There is no bug-bounty program; coordinated disclosure is appreciated.

## Known Security Considerations

### Default pre-shared keys (PSKs)

W.A.S.P. ships with **default Wi-Fi passwords** intended for local development and first boot only:

| Setting | Default | Where |
| ------- | ------- | ----- |
| `apPsk` | `waspswarm` | Nest AP (`wasp.cfg`) |
| `nestPsk` | `waspswarm` | Worker sync target (`worker.cfg`) |

**Change these before deploying in the field.** Anyone who knows the defaults can join the Nest access point or impersonate it. Treat `waspswarm` as a placeholder, not a secret.

Recommended practice:

1. Set unique, strong values for `apPsk` (Nest) and `nestPsk` (Workers) in your SD-card config files.
2. Do not commit `wasp.cfg` or `worker.cfg` with real credentials (see `.gitignore`).
3. Rotate keys if a device or SD card may have been exposed.

### Other notes

- Home Wi-Fi credentials (`homeSsid` / `homePsk`) and upload API tokens are stored in plain text on the Nest SD card.
- The Nest HTTP API is intended for trusted local networks only; do not expose it to the public Internet without additional hardening.
