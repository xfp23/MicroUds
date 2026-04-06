# MicroUDS

UDS (Unified Diagnostic Services) stack implementation.

## Features

- ISO 14229-1 compliant
- Minimal memory footprint
- Easy to port
- Modular architecture

## Directory Structure

```
MicroUDS/
├── inc/                  # Public headers
├── src/
│   ├── core/             # Core functionality
│   ├── service/          # UDS services (0x10, 0x11, 0x27, etc.)
│   ├── transport/isotp/  # ISO-TP layer
│   ├── session/          # Session management
│   ├── security/         # Security access
│   ├── did/              # Data Identifier
│   ├── routine/          # Routine control
│   ├── download/         # Download/Upload
│   ├── dtc/              # Diagnostic Trouble Codes
│   └── utils/            # Utilities
├── port/                 # Platform adaptation layer
├── example/              # Example code
└── doc/                  # Documentation
```

## Getting Started

1. Configure `MicroUds_conf.h`
2. Implement platform-specific functions in `port/`
3. Build and integrate into your project

## License

See LICENSE file for details.
