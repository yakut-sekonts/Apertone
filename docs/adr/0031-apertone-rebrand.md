# ADR 0031: Apertone product identity with Luma upgrade compatibility

## Decision

Version 1.1.0 changes the public product, application bundle, executable, icon,
release artifacts and GitHub repository name from Luma to Apertone.

`CFBundleIdentifier` remains `app.luma.desktop`, and the Sparkle EdDSA public key
remains unchanged. The desktop process also keeps the legacy Qt organization and
application names for storage after command-line parsing. This preserves the
existing `AppLocalDataLocation`, `QSettings`, catalog selection and update trust
chain for installed 1.0 users. Finder, menus, windows and `--version` expose only
the Apertone name.

Existing rendered DNG files whose maker/model fields contain Luma remain readable.
New exports identify Apertone. Internal serialized identifiers, cache suffixes,
model bundle filenames and in-flight import folder prefixes retain their stable
Luma values where changing them would invalidate user data or recovery state.

The Sparkle feed moves to the Apertone GitHub repository. GitHub's repository
rename redirect keeps the feed URL embedded in Luma 1.0 functional, allowing the
1.1.0 update to arrive through the existing bell.

## Consequences

- A user can install 1.1.0 over 1.0.0 without migrating or duplicating a catalog.
- The 1.0 public key can authenticate the 1.1.0 ZIP.
- Removing the legacy bundle identifier, storage names or DNG recognition requires
  a separate migration and cannot be folded into a visual rebrand.

