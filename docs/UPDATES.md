# macOS updates

Luma uses pinned Sparkle 2.10.0 with an Ed25519 public key in Info.plist.
The bell presents scheduled checks without interrupting editing. Clicking Install
opens Sparkle's standard UI with release information, download and installation.
Checks run daily and can be disabled in the bell. Automatic background installation
and system profiling are disabled. Manual checks still work after opting out.

The native delegate postpones installation/relaunch until edits are successfully
saved and active loads, exports, Library writes and imports finish. A modal waiting
view blocks new editing and offers retry-save or cancel-import/export when needed.
Failed saves cannot be bypassed by a timer. The catalog and RAW files are outside
the application bundle; Sparkle replaces only the application.

## Release setup

The project's key was created under Keychain account `yakut-sekonts.Luma` using
Sparkle's generate_keys. The private key is not in the repository. Keep this key:
future archives must validate against the already installed public key.

Before tagging an updater-enabled release, add repository Actions secret
`SPARKLE_PRIVATE_KEY` at
[repository secrets](https://github.com/yakut-sekonts/Luma/settings/secrets/actions).
Its value is the private seed from the project's Sparkle Keychain item, not a
GitHub token. Transfer it yourself using Keychain Access or Sparkle's export tool;
never put it into an Issue, PR, tracked file or command argument.

Release candidate branches report whether the signing secret is configured.
When it is present, the candidate build signs and verifies the exact archive too;
missing secrets allow candidate QA but refuse tagged release builds before packaging.
The release workflow packages and verifies the app, signs the exact ZIP using
Sparkle's sign_update with the secret on stdin, and independently checks that
signature with CryptoKit against the embedded public key. It creates appcast.xml
and includes the feed in SHA256SUMS and the verified draft assets. Publishing
happens only after all assets match the manifest. Missing/wrong keys fail publication.
The app's HTTPS feed URL resolves to the latest release's appcast.xml asset.
No workstation GitHub credential is used for signing or release publication.

For a local verified release build, signing uses the Keychain without exporting:

```bash
.tools/venv/bin/python scripts/create-appcast.py \
  --signer build/_deps/sparkle-src/bin/sign_update
```

If using a local Sparkle source override, pass its actual sign_update path.
Generate the feed after `scripts/release-artifacts.sh` and standalone verification;
never modify the archive after signing. The feed contains the immutable tag ZIP URL,
version, length, minimum macOS version and its detached EdDSA signature.

## Adoption and limits

v0.3.0 predates this integration. It requires one manual installation of the first
updater-enabled release; already distributed binaries cannot gain this feature
retroactively. Until a signed new release is published, the new feed is unavailable
and a manual check reports a network/feed error in the bell, with retry available.
Development and CI acceptance flags disable real network update checks.

EdDSA archive signing does not provide Developer ID signing or notarization.
Those remain part of macOS distribution hardening. A release is not automatically
rolled back by Luma if its catalog migration is incompatible with old binaries;
keep catalog backups when testing development versions.

Implementation follows Sparkle's [programmatic setup](https://sparkle-project.org/documentation/programmatic-setup/)
and [gentle reminders](https://sparkle-project.org/documentation/gentle-reminders/).
