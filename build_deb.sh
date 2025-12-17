#!/bin/bash
# Wrapper script for packaging/scripts/build_deb.sh
# This allows running ./build_deb.sh from project root

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$SCRIPT_DIR/packaging/scripts/build_deb.sh" "$@"

