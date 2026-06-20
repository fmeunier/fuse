---
name: update-changelog
description: Drafts a new top-of-file Fuse release entry in ChangeLog using unrecorded commits and the project’s existing grouped format. Drafts first, then waits for confirmation before editing ChangeLog.
---

# Update ChangeLog

This is a project-specific skill for preparing a Fuse release ChangeLog entry.

Use this skill when the user wants to update `ChangeLog` for a release.

## Rules

- Follow the existing style in `ChangeLog` exactly.
- Preserve all existing entries unchanged.
- Draft first.
- Do not edit `ChangeLog` until the user confirms.
- If the release version is not yet known, ask the user for it before drafting the new entry.
- Use the most recent unrecorded commit date for the new heading.
- Use Philip Kendall’s maintainer identity in the heading:
  `YYYY-MM-DD  Philip Kendall  <philip-fuse@shadowmagic.org.uk>`

## Workflow

Update the ChangeLog file in the current directory `ChangeLog` with any commits that are not yet recorded.

1. Use `git log ChangeLog` to find the most recent modification git hash.
2. Run `git log --format="%H %ai %s" <last modification git hash>..HEAD` to find unrecorded commits.
3. Run `git log <last modification git hash>..HEAD` to inspect commit bodies when needed.
4. Filter out noise commits: merges, "revert accidental commit", "add ignore", "update for version",
   "first updates for", "bring up to date", "tidy up commit text", and similar meta-commits that don't
   describe user-visible changes.
5. For each remaining commit, determine its category according to the principles below.
6. Add a new date entry at the top of the `ChangeLog` after the first line of the file, using the most recent
   unrecorded commit's date, with Philip Kendall's name/email as maintainer:
   `YYYY-MM-DD  Philip Kendall  <philip-fuse@shadowmagic.org.uk>`
7. Under that date, add a `* Fuse X.Y.Z released.` entry.
8. Under that date, add entries grouped by category, 8-space indented, matching the existing format.
9. Show the proposed new entry to the user and ask for confirmation.
10. Only after confirmation, write the updated `ChangeLog`.

## ChangeLog grouping structure

### Top-level structure per release

Each release entry has:
1. Date line (`YYYY-MM-DD`)
2. Author line (`Philip Kendall` with email)
3. Release announcement (`* Fuse X.Y.Z released.`)
4. Categorized bullet groups (see below)
5. Catch-all footer (`* Various other minor bugfixes.` or similar)

### Category groups

The ChangeLog uses a fixed set of category headings, each as a bullet followed by a colon. Sub-items are indented
bullets under the heading:

- `* New features:` User-facing additions: new hardware emulation, new file formats, new UI backends, new capabilities.
- `* Emulation core improvements:` Z80 accuracy, timing fixes, peripheral emulation, disk/tape handling, sound accuracy.
- `* Machine specific improvements:` Fixes scoped to a particular machine type (`+3`, Timex, Pentagon, etc.).
- `* UI improvements:` GTK/Win32/SDL/Xlib UI changes, dialogs, menus, display rendering, redraw fixes.
- `* Debugger improvements:` Breakpoints, disassembly, debugger variables, memory browser.
- `* Profiler improvements:` Code profiler fixes.
- `* Scaler improvements:` Graphics scaler additions/fixes.
- `* RZX improvements:` Recording/playback features.
- `* Deprecated features removed:` Removed UI backends, removed syntax, dropped dependencies.
- `* Miscellaneous improvements:` Build system, compiler warnings, docs, platform portability, memory leaks, anything that doesn't fit above.
- `* Testing improvements:` Unit tests, CI workflows, test coverage.
- `* Bugfixes:` / `* Minor bugfixes:` Small standalone fixes used in older releases.

## Key patterns

1. Not all categories appear in every release — only categories that have relevant changes are included.
2. Each bullet is a single sentence describing the change, often with:
   - the what
   - the impact
   - credits in parentheses at the end: `(Author Name)` or `(thanks, Contributor) (Author Name)`
   - bug references: `(fixes bug #NNN)`
3. Credits are consistent — the primary author of the commit is in the final parentheses. `thanks, X` acknowledges
   upstream or community input.
4. Bullet hierarchy is two levels deep — category heading to specific changes. No deeper nesting.
5. Most releases end with `* Various other minor bugfixes.` as a catch-all for small fixes not worth itemizing.
6. Order is roughly consistent: New features, Emulation core, Machine specific, UI, Debugger, optional categories,
   Deprecated, Miscellaneous, Testing, footer.
7. Indentation: 8 spaces for the category heading line, 10 spaces for sub-bullets, using spaces and not tabs.
8. Older releases use a simpler flat bullet list; follow the current grouped format used at the top of the file.

## When generating entries

- Classify each commit into one of the standard categories.
- Write a single-sentence bullet per change.
- Include impact or context where available, including bug numbers and affected games.
- Include credits in parentheses.
- Use the standard category ordering.
- Skip categories with no entries.
- Add `Various other minor bugfixes` as a footer if there are unclassified small fixes.

## Drafting instructions

When presenting the draft:

- show only the proposed new ChangeLog entry
- do not edit the file yet
- ask the user to confirm or request changes

When the user confirms:

- insert the new entry at the top of `ChangeLog` after the first line
- do not alter older entries
