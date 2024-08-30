# Ethical Hacking
This repository contains all code which was written for the Ethical Hacking Seminar.

- `actions_monitor`: is a program to monitor a libssh2 GitHub Actions project and connect to the AppVeyor Container via SSH.
- `appveyor_secrets_filter`: is a draft which exploits the secrets filter used in AppVeyor.
- `dispatch_apache`: The dockerfiles used to setup the original libssh2 dispatch script.
- `dispatch_curl`: A minimal curl request which manipulates the dispatch script GitHub API call.
- `dispatch_server`: A replication of the dispatch script for testing purposes.
- `fcnt`: A small program to count the amount of files in a directory used to count tree files from the `githubTreeFetch` program.
- `gha_events`: Abandoned version of the `actions_monitor` with a webscraping approach (which avoids the GitHub API).
- `githubBlobFetch`: A program to download BLOBs from a tree file.
- `githubRepoListFetch`: Fetches repository links and metadata from the GitHub API.
- `githubRepoListFilter`: Filters the repository list for interesting repositories and manipulates the output format.
- `githubTreeFetch`: Downloads the tree file of projects which are in a list downloaded by `githubRepoListFetch`.
