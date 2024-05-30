use chrono::{DateTime, Utc};
use std::{
    io::BufRead,
    time::{Duration, UNIX_EPOCH},
};

use serde::{Deserialize, Serialize};

#[allow(non_snake_case)]
#[derive(Serialize, Deserialize)]
pub struct CookieEditor {
    name: String,
    value: String,
    domain: String,
    hostOnly: bool,
    path: String,
    secure: bool,
    httpOnly: bool,
    sameSite: String,
    session: bool,
    firstPartyDomain: String,
    partitionKey: Option<String>,
    expirationDate: u64,
    storeId: Option<u64>,
}

impl CookieEditor {
    pub fn convert(&self) -> String {
        let d = UNIX_EPOCH + Duration::from_secs(self.expirationDate);
        let datetime = DateTime::<Utc>::from(d);
        let timestamp_str = datetime.format("%Y-%m-%dT%H:%M:%SZ").to_string();
        format!("{{ \"raw_cookie\": \"{}={}\", \"path\":[\"/\", true], \"domain\":{{\"Suffix\": \"{}\"}}, \"expires\": {{\"AtUtc\": \"{}\"}} }}", self.name, self.value, self.domain, timestamp_str)
    }

    pub fn from<R: BufRead>(reader: R) -> Self {
        serde_json::from_reader(reader).unwrap()
    }
}
