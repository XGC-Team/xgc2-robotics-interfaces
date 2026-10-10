#!/usr/bin/env bash
# Build one architecture-independent wire deb locally, in a fresh output path.
# No Host/plugin build, network, package installation, or release dispatch.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)"
if [[ $# != 2 || "$1" != --output || -z "$2" ]]; then
  echo 'usage: .xgc2/scripts/build_deb.sh --output DIR' >&2
  exit 2
fi
output="$2"
package=libxgc2-robotics-interfaces-dev
version="$(awk '$1 == "focal:" { print $2; exit }' "$root/.xgc2/product.yml")"
base="$(awk '/^version:/ { print $2; exit }' "$root/.xgc2/product.yml")"
[[ -n "$base" && "$version" == "$base~focal" ]] || { echo 'wire product version/apt_versions.focal is missing or inconsistent' >&2; exit 2; }
dpkg --validate-version "$version"
mkdir -p -- "$output"
output="$(cd "$output" && pwd -P)"
deb="$output/${package}_${version}_all.deb"
[[ ! -e "$deb" && ! -L "$deb" ]] || { echo "refusing existing artifact: $deb" >&2; exit 2; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
cmake -S "$root" -B "$work/build" -DCMAKE_INSTALL_PREFIX=/usr
DESTDIR="$work/package" cmake --install "$work/build"
mkdir -p "$work/package/DEBIAN" "$work/package/usr/share/doc/$package"
cp "$root/LICENSE" "$work/package/usr/share/doc/$package/copyright"
cat > "$work/package/DEBIAN/control" <<CONTROL
Package: $package
Version: $version
Section: libdevel
Priority: optional
Architecture: all
Maintainer: XGC Team <867768510@qq.com>
Description: XGC2 robotics interfaces and simulation control contracts
 Fourteen unchanged C payload layouts, usable from C11 and C++14.
 Exports the C11/C++14 Interfaces component without runtime dependencies.
 Installs simulation-v1 contracts; no runtime executable or ROS adapter.
CONTROL
# Build exactly one all-architecture artifact. A release orchestrator may later
# publish these verified bytes; this local builder does not claim publication.
epoch="${SOURCE_DATE_EPOCH:-$(git -C "$root" log -1 --format=%ct)}"
[[ "$epoch" =~ ^[0-9]+$ ]] || { echo 'invalid SOURCE_DATE_EPOCH' >&2; exit 2; }
export SOURCE_DATE_EPOCH="$epoch"
find "$work/package" -exec touch -h -d "@$epoch" {} +
dpkg-deb --root-owner-group --build "$work/package" "$work/interfaces.deb"
# Refuse a racing existing artifact instead of silently overwriting it.
cp --no-clobber "$work/interfaces.deb" "$deb"
cmp -s "$work/interfaces.deb" "$deb" || { echo "artifact appeared during build: $deb" >&2; exit 2; }
dpkg-deb --info "$deb"
printf '%s\n' "$deb"
