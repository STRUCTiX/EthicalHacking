use std::collections::BTreeMap;

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

#[derive(Deserialize, Serialize, Debug)]
pub struct Inputs {
    // dispatch workflow accepts the following attributes:
    ssh_host: String,
    ssh_port: String, // u16
    ssh_user: String,
    ssh_forward: String,
    ssh_hostkey: String,
    ssh_privkey: String,
}

impl Inputs {
    pub fn from(form_data: BTreeMap<String, String>) -> Option<Self> {
        let ssh_host = if let Some(ssh_host) = form_data.get("ssh_host") {
            ssh_host.clone()
        } else {
            return None;
        };
        let ssh_port = if let Some(ssh_port) = form_data.get("ssh_port") {
            ssh_port.clone()
        } else {
            return None;
        };
        let ssh_user = if let Some(ssh_user) = form_data.get("ssh_user") {
            ssh_user.clone()
        } else {
            return None;
        };
        let ssh_forward = if let Some(ssh_forward) = form_data.get("ssh_forward") {
            ssh_forward.clone()
        } else {
            return None;
        };
        let ssh_hostkey = if let Some(ssh_hostkey) = form_data.get("ssh_hostkey") {
            ssh_hostkey.clone()
        } else {
            return None;
        };
        let ssh_privkey = if let Some(ssh_privkey) = form_data.get("ssh_privkey") {
            ssh_privkey.clone()
        } else {
            return None;
        };
        Some(Inputs {
            ssh_host,
            ssh_port,
            ssh_user,
            ssh_forward,
            ssh_hostkey,
            ssh_privkey,
        })
    }
}
