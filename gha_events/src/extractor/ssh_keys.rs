use std::collections::HashMap;

use super::Extractor;

pub struct SshKeys {
    private_key: String,
    public_key: String,
}

impl Extractor for SshKeys {
    fn new() -> Self {
        Self {
            private_key: String::new(),
            public_key: String::new(),
        }
    }

    fn parse(self, data: Vec<String>) {
        for d in data {
            todo!("Parse not implemented. Extract the ssh keys");
        }
    }

    fn extract(self) -> HashMap<String, String> {
        todo!("Extract private fields to HashMap tuples")
    }
}
