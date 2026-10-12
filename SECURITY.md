# Security

Never include personal photos, catalog files, secrets, certificates or private
paths in a public vulnerability report. Reproduce with synthetic assets where
possible. Use GitHub private vulnerability reporting for sensitive disclosures;
if unavailable, first open an issue containing only a request for a private channel.

All edits are stored separately from originals. A malformed RAW or failed write
must produce an error without modifying the original or deleting prior edits.
Before reporting a failure, preserve the catalog and its WAL/SHM files.

M0 is development software and has not undergone a security audit. Dependencies
are pinned and must be reviewed before each release.
