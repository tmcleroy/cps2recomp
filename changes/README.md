# Changes not released yet

Each pull request puts its changelog lines in a file of its own here, named after its branch
(`changes/street-fighter-alpha.md`), written as they are to read in `CHANGELOG.md`: one or more `- **...**` lines
in its style. Two pull requests open at once then never change the same lines, so neither conflicts with the other
in the changelog.

A release's pull request runs `tools/release/changelog.sh X.Y.Z`, which writes the version's section of
`CHANGELOG.md` from these files (the newest first) and removes them ([RELEASING.md](../docs/RELEASING.md), step 1).
