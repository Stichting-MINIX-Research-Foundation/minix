# Importing newer NetBSD sources

This repository contains MINIX 3 code alongside sources periodically imported
from NetBSD. The project does not provide an automated tool for updating all
NetBSD components at once. The general approach below may be used to bring in
a more recent snapshot of NetBSD.

1. Add the official NetBSD repository as a remote and fetch its history:

    ```sh
    git remote add netbsd https://github.com/NetBSD/src.git
    git fetch netbsd
    ```

2. Choose the NetBSD branch or tag you want to integrate, for example
   `netbsd-10` or a specific release tag, and check it out:

    ```sh
    git checkout -b netbsd-update netbsd/netbsd-10    # example branch
    ```

3. Merge the selected NetBSD revision into the MINIX tree. Expect many
   conflicts that require manual resolution. Use `--allow-unrelated-histories`
   because the two projects started from different repositories.

    ```sh
    git merge --allow-unrelated-histories netbsd/netbsd-10
    ```

4. After resolving conflicts, rebuild the entire system to verify that the
   merge did not introduce build errors:

    ```sh
    ./build.sh -j$(nproc) tools build
    ```

   Consult `docs/UPDATING` for additional steps that may be required after
   bringing in new code.

Alternatively, set the `NETBSD_BRANCH` environment variable and run
`releasetools/sync_netbsd.sh` to automate these steps:

```sh
NETBSD_BRANCH=netbsd-10 releasetools/sync_netbsd.sh
```


Updating to a new NetBSD snapshot touches a large number of files and may
change interfaces used throughout MINIX. Take care to review each conflict
and test the result thoroughly before deploying.

## Files affected by NetBSD updates

Most directories in this tree originate from NetBSD and will change when
syncing to a new snapshot. The following areas are commonly affected:

- `sys/` – kernel sources and platform code
- `lib/`, `libexec/` – system libraries and helper daemons
- `bin/`, `sbin/`, `usr.bin/`, `usr.sbin/` – userland utilities
- `include/`, `crypto/`, `external/` – headers and third-party packages
- `tests/`, `tools/`, `distrib/` – test suite, build tools and release scripts

Only the `minix/` subtree and a few `minix` programs under `usr.*` are
maintained separately. Interface changes in any imported directory may require
corresponding updates throughout the MINIX-specific code.
