# Release Checklist

> **Upstream historical document:** this is JA2 Stracciatella's original release
> checklist and mentions upstream milestones/platform packages. It is not evidence
> that Escape from Arulco publishes or tests those targets. The OS//0 `v0.1.0`
> tag records one **FRAGILE-TESTED EARLY ALPHA Windows x64 playtest**.
> Unreleased source changes require a fresh complete test, build, package inspection
> and startup check; this historical list does not supply that validation.

## Escape from Arulco publication gate

The release packager intentionally refuses a dirty tree or a `HEAD` that is not tagged
with the version from `escape-from-arulco-version`. Before running it:

- [ ] Finish the intended source and documentation changes; choose a new version only
      when a new publication is actually authorized.
- [ ] Configure a separate `WITH_UNITTESTS=ON` build and run the complete registered
      suite. Record the count/output from that exact run rather than copying an older
      number into living documentation.
- [ ] Build the Windows game and launcher from the same clean commit.
- [ ] Tag that exact commit with `v<escape-from-arulco-version>`.
- [ ] Run `.ci/package-windows-playtest.ps1`, inspect the generated runtime/DLL manifest
      and verify `BUILD_INFO.txt` names the tagged commit.
- [ ] Start the extracted archive on a clean test path and complete the Golden Path in
      `PLAYTESTING.md`; keep broader manual scenarios marked PASS, FAIL or NOT REACHED.

`BUILD_AND_START_LOCAL.cmd` is deliberately not a publication gate: it configures
`WITH_UNITTESTS=OFF`, incrementally builds `ja2` and starts the local executable.

## Pre-release development

- [ ] Fix or postpone issues in the [milestone](https://github.com/ja2-stracciatella/ja2-stracciatella/milestone/6)
- [ ] Triage open pull requests and recent bugs
- [ ] Finalize choice of version number
  - [ ] Decide if this version should be 1.0 #443
- [ ] Add new contributors to contributors.txt
- [ ] Update changes.md (run docs/draft-changelog.sh to get started)
- [ ] Update man page if necessary

## Release candidate

- [ ] Update Version number to `-rc` and tag
- [ ] Mark the tag as a Github prerelease
- [ ] Runtime tests of prerelease packages
  - [ ] Linux
    - [ ] Ubuntu 20.04 x64
  - [ ] Windows
  - [ ] OS X

Runtime tests:

- IMP creation
- AIM and extending contracts
- Taking Drassen airport or more
- Buying from Bobby Ray's
- Visual inspection of laptop screens

If you want to build the packages manually:

    $ make clean
    $ make build-releases # to build all releases
    $ make build-win-release-on-linux # crosscompile Windows release on Linux
    $ make build-release-on-mac # build the mac bundle
    $ make build-debian-package # build the .DEB package


## Release

- [ ] Attach built versions to GitHub release
  - [ ] Linux, Windows
  - [ ] OS X
- [ ] Document new features on the website
- [ ] Make GitHub prerelease the release, update texts
- [ ] Announce
  - [ ] Write main announcement
  - [ ] Website download page (update frontmatter variables as needed)
  - [ ] Website news
  - [ ] Bear's pit
  - [ ] Moddb


## Post-release

- [ ] Set version to +1 and update README.md link
- [ ] Close this bug and GitHub milestone
- [ ] Create new milestone if none exists yet
