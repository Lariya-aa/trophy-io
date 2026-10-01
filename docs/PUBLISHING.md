# GitHub publishing

The repository is prepared for publication with an English homepage and Chinese/Japanese README links. Its application license is GPL-3.0-or-later; third-party font notices retain BSD-3-Clause. See [LICENSE](../LICENSE), [NOTICE](../NOTICE) and [THIRD_PARTY.md](../THIRD_PARTY.md).

## Repository contents

Publish this independent repository, including its source, build scripts, licenses and release source archive. Do not upload the surrounding PSV asset repository. Local build directories, bytecode, credentials, raw DAT files and trophy databases are excluded by `.gitignore`.

The existing `releases/v0.1.0/` files preserve the original build evidence. Its build log records a local temporary build directory; no account identifiers, credentials, raw trophy samples or databases are included. The source archive and VPK hashes are recorded in the accompanying metadata. Git commit author/committer metadata will also be part of the published history.

The public GitHub repository is [Lariya-aa/trophy-io](https://github.com/Lariya-aa/trophy-io). The associated account has the display name Lariya and login `Lariya-aa`; the connector and local GitHub CLI identities were checked before creating it on 2026-10-01. The local `origin` uses `https://github.com/Lariya-aa/trophy-io.git`. Publication includes `main` and the original `v0.1.0` tag; it does not establish hardware validation or a GitHub Release.

## Binary releases

For the first GitHub Release, describe v0.1.0 as an export-only, hardware-unverified build. Include the original VPK, checksum and corresponding source archive from `releases/v0.1.0/`, together with build evidence and license/notices. Provide LICENSE, NOTICE, THIRD_PARTY.md and third_party/PSPSDK-LICENSE alongside the binaries/source or through the accompanying release repository. Keep the dated modification notices with the source distribution.

For later builds, create a new version directory and matching source archive rather than replacing the original files. Preserve upstream authorship and applicable third-party notices. Provide the corresponding source and build instructions under the applicable GPL distribution terms; a link to unrelated or newer source is not a substitute for the source of a released binary.

## Verification

Before publishing, check the staged file list, local documentation links, VPK checksum and corresponding-source hashes. Distinguish host/package checks from device runtime validation in the release notes. Do not publish exported DAT files, account material or private device logs in issues or release assets.
