#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
backend_dir="$(cd -- "$script_dir/.." && pwd -P)"
install_dir="/usr/local/lib/atlas"
installed_binary="$install_dir/atlas-arp"
local_binary="$backend_dir/native/dist/atlas-arp"
target_group=""
skip_build=false
staged_binary=""

usage() {
    cat <<'HELP'
Usage : bash scripts/deploy-native.sh [--group GROUPE] [--skip-build]

Compile le scanner ARP, l'installe dans /usr/local/lib/atlas/atlas-arp
et lui accorde uniquement cap_net_raw=ep.

  --group GROUPE  Groupe autorisé à exécuter le scanner.
                  Par défaut : groupe principal de l'utilisateur qui lance le script.
  --skip-build    Installe un binaire déjà compilé dans native/dist/atlas-arp.
  --help         Affiche cette aide.

Lancer normalement ce script avec l'utilisateur de développement.
Les commandes d'installation utilisent sudo si nécessaire.
Pour un déploiement lancé directement par root, préciser --group.
HELP
}

fail() {
    echo "Erreur : $*" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --group)
            [[ $# -ge 2 && -n "$2" ]] || fail "Il manque le groupe après --group."
            target_group="$2"
            shift 2
            ;;
        --skip-build)
            skip_build=true
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "Option inconnue : $1. Utiliser --help pour voir les options."
            ;;
    esac
done

[[ "$(uname -s)" == "Linux" ]] || fail "Ce déploiement nécessite Linux."

for tool in install mktemp mv rm stat getent id setcap getcap; do
    command -v "$tool" >/dev/null 2>&1 || fail "Commande nécessaire introuvable : $tool."
done

# On garde sudo pour l'installation, le backend pourra tourner avec son utilisateur habituel
as_root() {
    if [[ "$EUID" -eq 0 ]]; then
        "$@"
    else
        sudo -- "$@"
    fi
}

if [[ "$EUID" -ne 0 ]]; then
    command -v sudo >/dev/null 2>&1 || fail "sudo est nécessaire pour installer le scanner."
fi

if [[ -z "$target_group" ]]; then
    if [[ "$EUID" -eq 0 && -z "${SUDO_USER:-}" ]]; then
        fail "Préciser --group avec le groupe de l'application pour un déploiement root."
    fi
    invoking_user="${SUDO_USER:-$(id -un)}"
    target_group="$(id -gn -- "$invoking_user")"
fi
getent group -- "$target_group" >/dev/null || fail "Groupe introuvable : $target_group."

if [[ "$skip_build" == false ]]; then
    bash "$script_dir/build-native.sh"
fi
[[ -f "$local_binary" && ! -L "$local_binary" && -x "$local_binary" ]] || \
    fail "Le binaire local est absent ou invalide. Lancer npm run build:native."

if [[ -L "$install_dir" || ( -e "$install_dir" && ! -d "$install_dir" ) ]]; then
    fail "$install_dir doit être un vrai dossier."
fi

# On prépare le nouveau fichier à côté de l'ancien pour pouvoir le remplacer d'un coup
cleanup() {
    if [[ -n "$staged_binary" ]]; then
        as_root rm -f -- "$staged_binary" || true
    fi
}
trap cleanup EXIT

as_root install -d -o root -g root -m 0755 -- "$install_dir"
staged_binary="$(as_root mktemp "$install_dir/.atlas-arp.XXXXXX")"
as_root install -o root -g "$target_group" -m 0750 -- "$local_binary" "$staged_binary"

# On vérifie les droits et la permission ARP avant de remplacer le scanner installé
[[ "$(as_root stat -c '%u:%a' -- "$staged_binary")" == "0:750" ]] || \
    fail "Le propriétaire ou les droits du binaire ne correspondent pas à l'installation attendue."
as_root setcap cap_net_raw=ep "$staged_binary"
capabilities="$(as_root getcap "$staged_binary")"
[[ "$capabilities" == "$staged_binary cap_net_raw=ep" ]] || \
    fail "La permission CAP_NET_RAW n'a pas pu être vérifiée. L'ancien scanner est conservé."

as_root mv -fT -- "$staged_binary" "$installed_binary"
staged_binary=""

echo "Scanner installé : $installed_binary"
echo "Propriétaire : root | Groupe : $target_group | Droits : 0750 | Permission : cap_net_raw=ep"
echo "Le compte qui lance Nest doit appartenir au groupe $target_group."
echo "Chemin à utiliser dans la configuration : ATLAS_ARP_BINARY=$installed_binary"
