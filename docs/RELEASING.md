# Releases

`vcpkg.json` holds the version. Update its entry in `CHANGELOG.md` with player-facing changes.

A push to `master` builds the DLL and runs Linux and Windows tests. Download Main, source, symbols and API packages from that run's **Tessera** artifact. Development filenames include the commit ID.

To publish a version, wait for the commit's checks to pass, then tag that commit:

```sh
git tag -a v0.32.6 -m "Tessera 0.32.6"
git push origin v0.32.6
```

The tag must match `vcpkg.json`. The workflow tests it again and publishes the packages after both jobs pass. Tags such as `v0.33.0-rc.1` produce GitHub prereleases; `v0.33.0` produces a regular release.

Use a patch version for fixes and a minor version for feature releases. Use an RC when a finished set of changes needs wider testing. Publish 1.0 when the core experience is stable and ready for regular use.

Card faces, thumbnails and expansion illustrations stay in the Nexus Card Art download. Main includes card backs and the remaining runtime assets. The packaging checks enforce this split. Local Nexus packaging is available through `python3 tools/package.py` when the artwork is installed in the checkout.

Each release includes `SHA256SUMS.txt` and `build-info.json`, which records the source commit, tested DLL hash and Windows probe results.
