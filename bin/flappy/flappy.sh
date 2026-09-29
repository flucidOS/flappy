#!/bin/sh
#
# flappy - simple front-end for the FLAPPY package manager.
#
#   flappy install <pkg...>   sync repositories, then install packages
#   flappy remove <pkg...>    remove packages
#   flappy update             sync repository indexes
#   flappy upgrade            sync repository indexes, then upgrade all packages
#
# Extra options are passed through to the underlying tool, e.g.
#   flappy install -y curl
#   flappy remove -R curl
#   flappy upgrade -n

PROG=${0##*/}

usage() {
	cat <<USAGE
Usage: $PROG <command> [options] [package...]

Commands:
  install <pkg...>   Install one or more packages
  remove  <pkg...>   Remove one or more packages
  update             Refresh the repository indexes
  upgrade            Upgrade all installed packages

Options (passed through to flappy-install / flappy-remove):
  -y, --yes          Assume yes to all questions
  -n, --dry-run      Show what would be done (install/upgrade)
  -h, --help         Show this help
  -V, --version      Show version

See flappy-install(1) and flappy-remove(1) for all options.
USAGE
}

die() {
	echo "$PROG: $*" >&2
	exit 1
}

# True if the argument list contains at least one package name, i.e. a
# non-option argument that is not the value of an option taking one.
# $1 is a space-separated list of options that take a value.
has_pkg() {
	valopts=" $1 "
	shift
	skip=0
	for a in "$@"; do
		if [ $skip -eq 1 ]; then
			skip=0
			continue
		fi
		case "$a" in
		--*=*) ;;
		-*)
			case "$valopts" in
			*" $a "*) skip=1 ;;
			esac
			;;
		*) return 0 ;;
		esac
	done
	return 1
}

[ $# -ge 1 ] || { usage >&2; exit 1; }

cmd=$1
shift

case "$cmd" in
install)
	has_pkg "-C --config -c --cachedir -R --repository -r --rootdir" "$@" ||
		die "install: no package specified"
	exec flappy-install -S "$@"
	;;
remove)
	has_pkg "-C --config -c --cachedir -r --rootdir" "$@" ||
		die "remove: no package specified"
	exec flappy-remove "$@"
	;;
update)
	exec flappy-install -S "$@"
	;;
upgrade)
	flappy-install -Su "$@"
	rc=$?
	# flappy-install exits with EBUSY (16) when the 'flappy' package itself
	# must be updated before anything else. Do that first, then retry.
	if [ $rc -eq 16 ]; then
		flappy-install -u "$@" flappy || exit $?
		flappy-install -Su "$@"
		rc=$?
	fi
	exit $rc
	;;
-h | --help | help)
	usage
	;;
-V | --version | version)
	exec flappy-install -V
	;;
*)
	echo "$PROG: unknown command '$cmd'" >&2
	usage >&2
	exit 1
	;;
esac
