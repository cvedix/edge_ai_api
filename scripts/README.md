# Scripts Directory

Thư mục chứa các script cho development và utilities.

## Scripts Chính

### Development Setup

- **`setup.sh`** - Script chính cho development setup
  - Cài đặt dependencies
  - Fix symlinks (CVEDIX SDK, Cereal, cpp-base64, OpenCV)
  - Build project
  - Usage: `./scripts/setup.sh [options]`

- **`load_env.sh`** - Load environment variables và chạy server
  - Load từ `.env` file
  - Validate configuration
  - Run server
  - Usage: `./scripts/load_env.sh [--load-only]`

### Utilities

- **`utils.sh`** - Utility commands
  - `test` - Run unit tests
  - `generate-solution` - Generate solution template
  - `restore-solutions` - Restore default solutions
  - Usage: `./scripts/utils.sh <command>`

## Quick Start

### Development Setup

```bash
# Full setup
./scripts/setup.sh

# Skip dependencies
./scripts/setup.sh --skip-deps

# Only build
./scripts/setup.sh --build-only
```

### Run Server

```bash
# Load env and run
./scripts/load_env.sh

# Only load env (for current shell)
source ./scripts/load_env.sh --load-only
```

### Utilities

```bash
# Run tests
./scripts/utils.sh test

# Generate solution template
./scripts/utils.sh generate-solution
```

## Options

### setup.sh
- `--skip-deps` - Skip installing dependencies
- `--skip-symlinks` - Skip fixing symlinks
- `--skip-build` - Skip building
- `--build-only` - Only build, skip other steps

### load_env.sh
- `--load-only` - Only load env, don't run server
