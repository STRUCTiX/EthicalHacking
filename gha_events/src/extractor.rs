use std::collections::HashMap;

mod ssh_keys;

pub trait Extractor {
    fn new() -> Self;
    fn parse(self, data: Vec<String>);
    fn extract(self) -> HashMap<String, String>;
}
