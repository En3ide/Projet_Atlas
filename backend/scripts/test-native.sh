#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
backend_dir="$(cd -- "$script_dir/.." && pwd -P)"
compiler="${CC:-cc}"
test_dir="$(mktemp -d)"
trap 'rm -rf -- "$test_dir"' EXIT

# On vérifie aussi les accès mémoire pendant les échanges simulés
"$compiler" -std=gnu11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$backend_dir/native/tests/arp-discovery.test.c" \
    -o "$test_dir/arp-discovery-tests"

if "$test_dir/arp-discovery-tests" 2>"$test_dir/errors.log"; then
    echo "Tests natifs terminés, aucun paquet envoyé sur le réseau."
else
    cat "$test_dir/errors.log" >&2
    exit 1
fi
