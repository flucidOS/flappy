#!/bin/sh
#
# build.sh - build flappy packages from pkgsrc/templates and (re)index a
# signed repository.
#
# Usage: pkgsrc/build.sh [pkg[=version_revision] ...]
#   no arguments builds every template.
#   pkg=1.1_1 overrides version/revision (handy for testing upgrades).
#
# Environment:
#   REPO   repository directory        (default: ./repo)
#   KEY    RSA private key for signing (default: ./repo-key.pem, generated
#          if missing)
#   BINDIR directory holding the flappy-* tools (default: use PATH)
#   CACHE  download cache               (default: ./.distfiles)
#
# Templates are POSIX sh fragments defining pkgname, version, revision, arch,
# short_desc and optionally depends, conf_files, shlib_provides,
# shlib_requires, distfile, checksum, do_build() and do_install().

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=${REPO:-$PWD/repo}
KEY=${KEY:-$PWD/repo-key.pem}
CACHE=${CACHE:-$PWD/.distfiles}
[ -n "${BINDIR:-}" ] && PATH="$BINDIR:$PATH"

mkdir -p "$REPO" "$CACHE"
REPO=$(cd "$REPO" && pwd)

if [ ! -f "$KEY" ]; then
	echo "==> generating signing key $KEY"
	openssl genrsa -out "$KEY" 4096 2>/dev/null
fi

if [ $# -eq 0 ]; then
	set -- $(ls "$HERE/templates")
fi

build_one() {
	spec=$1
	name=${spec%%=*}
	override=
	[ "$name" != "$spec" ] && override=${spec#*=}
	tdir=$HERE/templates/$name
	[ -f "$tdir/template" ] || { echo "no template for $name" >&2; return 1; }

	work=$(mktemp -d)
	trap 'rm -rf "$work"' EXIT
	(
		cd "$work"
		pkgname= version= revision=1 arch= short_desc= depends= conf_files=
		shlib_provides= shlib_requires= distfile= checksum= license= homepage=
		maintainer=
		. "$tdir/template"
		if [ -n "$override" ]; then
			version=${override%_*}
			revision=${override##*_}
		fi
		FILESDIR=$tdir
		DESTDIR=$work/destdir
		mkdir -p "$DESTDIR"

		DISTFILE=
		if [ -n "$distfile" ]; then
			DISTFILE=$CACHE/$(basename "$distfile")
			if [ ! -f "$DISTFILE" ]; then
				echo "==> fetching $distfile"
				curl -fsSL -o "$DISTFILE.part" "$distfile"
				mv "$DISTFILE.part" "$DISTFILE"
			fi
			got=$(sha256sum "$DISTFILE" | cut -d' ' -f1)
			if [ "$got" != "$checksum" ]; then
				echo "checksum mismatch for $name: expected $checksum got $got" >&2
				rm -f "$DISTFILE"
				exit 1
			fi
		fi

		type do_build >/dev/null 2>&1 && do_build
		do_install

		pkgver=${pkgname}-${version}_${revision}
		echo "==> packaging $pkgver ($arch)"
		set -- -q -A "$arch" -n "$pkgver" -s "$short_desc" \
			-m "$maintainer" -l "$license" -H "$homepage" -B "flappy-build"
		[ -n "$depends" ] && set -- "$@" -D "$depends"
		[ -n "$conf_files" ] && set -- "$@" -F "$conf_files"
		[ -n "$shlib_provides" ] && set -- "$@" --shlib-provides "$shlib_provides"
		[ -n "$shlib_requires" ] && set -- "$@" --shlib-requires "$shlib_requires"
		(cd "$REPO" && flappy-create "$@" "$DESTDIR")
		(cd "$REPO" && flappy-rindex -f -a "$REPO/$pkgver.$arch.flappy" >/dev/null)
	)
	rm -rf "$work"
	trap - EXIT
}

for p in "$@"; do build_one "$p"; done

echo "==> signing repository and packages in $REPO"
flappy-rindex --privkey "$KEY" --signedby "FlucidOS <flucidos@localhost>" -s "$REPO" >/dev/null
for f in "$REPO"/*.flappy; do
	[ -f "$f.sig2" ] && [ "$f.sig2" -nt "$f" ] && continue
	flappy-rindex --privkey "$KEY" -S "$f" >/dev/null
done
echo "==> repository ready: $REPO"
flappy-query -r /nonexistent --repository="$REPO" -L 2>/dev/null || true
