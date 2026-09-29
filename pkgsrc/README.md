# pkgsrc: building flappy packages

Templates live in `pkgsrc/templates/<name>/template` (POSIX sh fragments).

| package | what it tests |
|---|---|
| `flucid-hello` | minimal noarch package; used for upgrade tests |
| `flucid-greeter` | dependency on `flucid-hello>=1.0_1` |
| `libflucid-demo`, `flucid-demo` | compiled C, `shlib-provides` / `shlib-requires` |
| `flucid-motd` | config file preservation, `INSTALL` / `REMOVE` scripts |
| `ripgrep`, `jq` | real static binaries, downloaded and sha256-pinned |

## Build a signed repository

```
./configure && make
export PATH="$PWD/bin/flappy-create:$PWD/bin/flappy-rindex:$PWD/bin/flappy-query:$PATH"
export LD_LIBRARY_PATH="$PWD/lib"

REPO=$PWD/repo KEY=$PWD/repo-key.pem pkgsrc/build.sh            # everything
REPO=$PWD/repo KEY=$PWD/repo-key.pem pkgsrc/build.sh ripgrep    # one package
REPO=$PWD/repo KEY=$PWD/repo-key.pem pkgsrc/build.sh flucid-hello=1.1_1   # override version
```

`KEY` is generated on first use. **Keep it private and out of git.** Serve `repo/`
over HTTP (or point `repository=` at the directory).

## Trusting the repository key

`flappy update` shows the key fingerprint and asks for confirmation on first use;
`-y` deliberately does not answer that question. For non-interactive builds
(the ephemeral container), ship the trusted key with the image instead:

```
# once, on a scratch root:  yes | flappy update -r /tmp/r -C /tmp/r/etc/flappy.d
COPY <fingerprint>.plist /var/db/flappy/keys/
```

## Adding a package

Create `pkgsrc/templates/<name>/template` with `pkgname`, `version`, `revision`,
`arch`, `short_desc` and a `do_install()` that fills `$DESTDIR`. Optional:
`depends`, `conf_files`, `shlib_provides`, `shlib_requires`, `do_build()`, and
`distfile` + `checksum` (sha256, verified before use).
`INSTALL` / `REMOVE` scripts go in the root of `$DESTDIR`.

## End-to-end test

```
make e2e
```

Builds the repo, serves it over HTTP, and runs `flappy` in a scratch rootdir:
key trust, install, dependencies, shared libs, scripts, config files, update,
upgrade, remove, tampered packages, and error handling.
