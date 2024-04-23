use serde::{Deserialize, Serialize};

/// Currently unused
#[derive(Deserialize, Serialize, Debug)]
struct Dispatch {
    account: String,
    project: String,
    buildid: String,
    base: String,
    hash: String,
    repo: String,
    // dispatch workflow accepts the following attributes:
    ssh_host: String,
    ssh_port: String, // u16
    ssh_user: String,
    ssh_forward: String,
    ssh_hostkey: String,
    ssh_privkey: String,
}
