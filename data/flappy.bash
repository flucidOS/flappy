_flappy_parse_help() {
	local IFS line word

	$1 --help 2>&1 | while IFS=$'\n' read -r line; do
		[[ $line == *([ $'\t'])-* ]] || continue

		IFS=$' \t,='
		for word in $line; do
			[[ $word == -* ]] || continue
			printf -- '%s\n' $word
		done
	done | sort | uniq
}

_flappy_all_packages() {
	flappy-query -Rs "$1*" | sed 's/^... \([^ ]*\)-.* .*/\1/'
}

_flappy_installed_packages() {
	flappy-query -l | sed 's/^.. \([^ ]*\)-.* .*/\1/'
}

_flappy_all_reply() {
	COMPREPLY=( $( compgen -W '$(_flappy_all_packages "$1")' -- "$1") )
}

_flappy_installed_reply() {
	COMPREPLY=( $( compgen -W '$(_flappy_installed_packages)' -- "$1") )
}

_flappy_complete() {
	local cur prev words cword

	_init_completion || return

	if [[ "$cur" == -* ]]; then
		COMPREPLY=( $( compgen -W '$( _flappy_parse_help "$1" )' -- "$cur") )
		return
	fi

	local common='C|-config|r|-rootdir'
	local morecommon="$common|c|-cachedir"

	local modes='auto manual hold unhold'
	local props='architecture
		archive-compression-type
		automatic-install
		build-options
		conf_files
		conflicts
		filename-sha256
		filename-size
		homepage
		install-date
		install-msg
		install-script
		installed_size
		license
		maintainer
		metafile-sha256
		packaged-with
		pkgver
		preserve
		provides
		remove-msg
		remove-script
		replaces
		repository
		shlib-provides
		shlib-requires
		short_desc
		source-revisions
		state'

	case $1 in
		flappy-dgraph)
			if [[ $prev != -@(c|o|r) ]]; then
				_flappy_installed_reply $cur
				return
			fi
			;;
		flappy-install)
			if [[ $prev != -@($morecommon) ]]; then
				_flappy_all_reply $cur
				return
			fi
			;;
		flappy-pkgdb)
			if [[ $prev == -@(m|-mode) ]]; then
				COMPREPLY=( $( compgen -W "$modes" -- "$cur") )
				return
			fi
			if [[ $prev != -@($common) ]]; then
				_flappy_installed_reply $cur
				return
			fi
			;;
		flappy-query)
			if [[ $prev == -@(p|-property) ]]; then
				COMPREPLY=( $( compgen -W "$props" -- "$cur") )
				return
			fi
			if [[ $prev != -@($morecommon|o|-ownedby) ]]; then
				local w
				for w in "${words[@]}"; do
					if [[ "$w" == -@(R|-repository) ]]; then
						_flappy_all_reply $cur
						return
					fi
				done
				_flappy_installed_reply $cur
				return
			fi
			;;
		flappy-reconfigure)
			if [[ $prev != -@($common) ]]; then
				_flappy_installed_reply $cur
				return
			fi
			;;
		flappy-remove)
			if [[ $prev != -@($morecommon) ]]; then
				_flappy_installed_reply $cur
				return
			fi
			;;
	esac

	_filedir
}

complete -F _flappy_complete flappy-checkvers flappy-create flappy-dgraph flappy-install \
	flappy-pkgdb flappy-query flappy-reconfigure flappy-remove flappy-rindex

_flappy_cmd_complete() {
	local cur prev words cword

	_init_completion || return

	if [[ $cword -eq 1 ]]; then
		COMPREPLY=( $( compgen -W 'install remove update upgrade help version' -- "$cur") )
		return
	fi

	case ${words[1]} in
		install)
			[[ "$cur" == -* ]] && return
			_flappy_all_reply "$cur"
			;;
		remove)
			[[ "$cur" == -* ]] && return
			_flappy_installed_reply "$cur"
			;;
	esac
}

complete -F _flappy_cmd_complete flappy
