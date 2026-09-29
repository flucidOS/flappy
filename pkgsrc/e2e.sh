#!/bin/sh
#
# e2e.sh - end-to-end test of the `flappy` command against a freshly built,
# signed repository served over HTTP. Installs into a scratch rootdir; the
# host is never touched.
#
# Usage: pkgsrc/e2e.sh        (after ./configure && make)

set -u
TOP=$(cd "$(dirname "$0")/.." && pwd)
W=$(mktemp -d)
PASS=0
FAIL=0
SRV=

for d in "$TOP"/bin/flappy "$TOP"/bin/flappy-*; do
	[ -d "$d" ] && PATH="$d:$PATH"
done
export PATH
export LD_LIBRARY_PATH="$TOP/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export FLAPPY_SYSLOG=false

cleanup() {
	[ -n "$SRV" ] && kill "$SRV" 2>/dev/null
	rm -rf "$W"
}
trap cleanup EXIT INT TERM

ok()   { PASS=$((PASS+1)); echo "  ok   $1"; }
bad()  { FAIL=$((FAIL+1)); echo "  FAIL $1"; }
check() { # check <desc> <cmd...>
	d=$1; shift
	if "$@" >"$W/out" 2>&1; then ok "$d"; else bad "$d"; sed 's/^/       /' "$W/out" | tail -5; fi
}
check_fail() { # command must fail
	d=$1; shift
	if "$@" >"$W/out" 2>&1; then bad "$d (expected failure)"; sed 's/^/       /' "$W/out" | tail -5; else ok "$d"; fi
}
contains() { grep -q "$2" "$1"; }

echo "== build packages"
export BINDIR=
REPO=$W/repo KEY=$W/key.pem CACHE=${CACHE:-$TOP/.distfiles} \
	sh "$TOP/pkgsrc/build.sh" >"$W/build.log" 2>&1 || { cat "$W/build.log"; exit 1; }
tail -3 "$W/build.log"

echo "== serve repo over http"
PORT=$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])')
(cd "$W/repo" && exec python3 -m http.server "$PORT" --bind 127.0.0.1 >/dev/null 2>&1) &
SRV=$!
sleep 1

newroot() { # newroot <name>
	R=$W/$1
	mkdir -p "$R/etc/flappy.d" "$R/var/db/flappy"
	echo "repository=http://127.0.0.1:$PORT" > "$R/etc/flappy.d/10-test.conf"
	C="-C $R/etc/flappy.d"
}
F() { flappy "$@" $C; }

echo "== trust: unknown key is refused without confirmation"
newroot untrusted
check_fail "update refuses untrusted key on closed stdin" sh -c "flappy update -r $R $C </dev/null"
check_fail "nothing was installed" sh -c "flappy install -r $R $C flucid-hello </dev/null && test -e $R/usr/bin/flucid-hello"

echo "== install / dependencies"
newroot main; R=$W/main
check "update: user trusts the repo key at the prompt" sh -c "yes | flappy update -r $R $C"
check "key stored in the rootdir" sh -c "ls $R/var/db/flappy/keys/*.plist"
check "install flucid-hello" F install -r "$R" -y flucid-hello
check "flucid-hello runs" sh -c "$R/usr/bin/flucid-hello | grep -q '1.0_1'"
check "install flucid-greeter" F install -r "$R" -y flucid-greeter
check "greeter runs" sh -c "$R/usr/bin/flucid-greeter | grep -q greeter"
check_fail "remove of a required package is refused" F remove -r "$R" -y flucid-hello
check "flucid-hello still installed" test -x "$R/usr/bin/flucid-hello"
check "remove greeter" F remove -r "$R" -y flucid-greeter
check "greeter files gone" sh -c "! test -e $R/usr/bin/flucid-greeter"
check "now remove flucid-hello" F remove -r "$R" -y flucid-hello

echo "== dependency pulled in automatically"
check "install greeter on empty system" F install -r "$R" -y flucid-greeter
check "hello came along" test -x "$R/usr/bin/flucid-hello"

echo "== shared library dependency"
check "install flucid-demo" F install -r "$R" -y flucid-demo
check "libflucid-demo came along" test -e "$R/usr/lib/libflucid-demo.so.1"
check "flucid-demo runs" sh -c "LD_LIBRARY_PATH=$R/usr/lib $R/usr/bin/flucid-demo | grep -q 'hello from libflucid-demo'"
check_fail "removing the library is refused" F remove -r "$R" -y libflucid-demo

echo "== real software (static binaries)"
check "install ripgrep jq" F install -r "$R" -y ripgrep jq
check "rg --version" sh -c "$R/usr/bin/rg --version | grep -q 14.1.1"
check "jq parses json" sh -c "echo '{\"a\":42}' | $R/usr/bin/jq .a | grep -q 42"
check "flappy-query lists them" sh -c "flappy-query -r $R -l | grep -q ripgrep-14.1.1_1"

echo "== install/remove scripts and config files"
check "install flucid-motd" F install -r "$R" -y flucid-motd
check "INSTALL script ran" sh -c "grep -q flucid-motd $R/var/lib/flucid-motd/last-install"
echo 'MESSAGE="customised"' > "$R/etc/flucid-motd.conf"

echo "== update / upgrade"
sleep 2	# HTTP Last-Modified has 1s resolution; make sure the new index is newer
REPO=$W/repo KEY=$W/key.pem CACHE=${CACHE:-$TOP/.distfiles} \
	sh "$TOP/pkgsrc/build.sh" flucid-hello=1.1_1 flucid-motd=1.1_1 >/dev/null 2>&1
check "update sees new index" F update -r "$R" -y
check "dry-run upgrade lists updates" sh -c "flappy upgrade -r $R $C -n -y | grep -q flucid-hello"
check "upgrade all" F upgrade -r "$R" -y
check "hello upgraded to 1.1_1" sh -c "$R/usr/bin/flucid-hello | grep -q '1.1_1'"
check "motd upgraded" sh -c "flappy-query -r $R -l | grep -q flucid-motd-1.1_1"
check "locally edited config file preserved" sh -c "grep -q customised $R/etc/flucid-motd.conf"
check "INSTALL script saw the update" sh -c "grep -q 'update=yes' $R/var/lib/flucid-motd/last-install"
check "second upgrade is a no-op" F upgrade -r "$R" -y

echo "== remove runs REMOVE script"
check "remove motd" F remove -r "$R" -y flucid-motd
check "REMOVE script cleaned up" sh -c "! test -e $R/var/lib/flucid-motd"

echo "== integrity"
sleep 2
REPO=$W/repo KEY=$W/key.pem CACHE=${CACHE:-$TOP/.distfiles} \
	sh "$TOP/pkgsrc/build.sh" flucid-hello=1.2_1 >/dev/null 2>&1
printf 'x' >> "$W/repo/flucid-hello-1.2_1.noarch.flappy"
check "update with tampered package still fetches index" F update -r "$R" -y
check_fail "tampered package is rejected" F upgrade -r "$R" -y
check "rejected by signature/checksum verification" sh -c "flappy upgrade -r $R $C -y 2>&1 | grep -qE 'signature is not valid|checksum does not match'"
check "hello stays on 1.1_1" sh -c "$R/usr/bin/flucid-hello | grep -q '1.1_1'"

echo "== error handling"
check_fail "install without package name" F install -r "$R" -y
check_fail "remove without package name" F remove -r "$R" -y
check_fail "unknown package" F install -r "$R" -y no-such-package
check_fail "unknown command" flappy frobnicate

echo
echo "passed: $PASS  failed: $FAIL"
[ "$FAIL" -eq 0 ]
