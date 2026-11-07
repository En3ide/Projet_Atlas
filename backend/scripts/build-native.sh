#!/usr/bin/env bash
set -euo pipefail

# On retrouve le backend même si le script est lancé depuis un autre dossier
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
backend_dir="$(cd -- "$script_dir/.." && pwd -P)"
compiler="${CC:-cc}"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "Le scanner ARP se compile uniquement pour Linux." >&2
    exit 1
fi

if ! command -v "$compiler" >/dev/null 2>&1; then
    echo "Compilateur introuvable : $compiler. Il faut un compilateur C pour cette étape." >&2
    exit 1
fi

# On génère le binaire local sans lui donner de permissions réseau
mkdir -p -- "$backend_dir/native/dist"
"$compiler" -std=gnu11 -O2 -Wall -Wextra -Werror \
    "$backend_dir/native/arp-discovery.c" \
    -o "$backend_dir/native/dist/atlas-arp"

echo "Scanner compilé : $backend_dir/native/dist/atlas-arp"
