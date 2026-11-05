# H4 test records

One file per run of `sh h4/test/record.sh` (add `--lathe` for the screen tests too): UTC date, commit (`+changes`
when the working copy had changes not yet committed), firmware and app versions, tools, whether LinuxCNC's `rs274`
was used, and every result line of the tests. The script also says what changed since the previous record.

These are results of tests on a PC (simulation). They do not say anything about a real H4: those tests are written
down separately, by hand, with the date and the firmware version.
