# Github Actions Events

## Setup
1. Compile the program with `cargo build -r`.
2. Export your Github `user_session` cookie with the [CookieEditor](https://addons.mozilla.org/en-US/firefox/addon/cookie-editor/) plugin as json.
3. Create a `github_session.json` file with the content of this cookie. An example is in `github_session_example.json`.
4. Execute the program with the `-c` flag to create a `session.json` file in the correct format (`./gha_events -c`).
5. Execute the program with a user and project name delimited with a `/`. Example: `./gha_events libssh2/libssh2`
6. The logs will be downloaded to the `out` directory.
