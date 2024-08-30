use anyhow::anyhow;
use anyhow::Context;
use tokio::fs;
use tracing::{info, warn};

pub async fn store_private_key(log_msg: &str, keyname: &str) -> anyhow::Result<()> {
    let lines = log_msg.split('\n');

    for l in lines.into_iter() {
        if l.len() < 42 {
            continue;
        }
        let rm_prefix = &l[42..];
        if !rm_prefix.starts_with("-----") {
            continue;
        }
        let split_once = rm_prefix.split_once('\'');
        if let Some(sp) = split_once {
            let privkey = sp.0.replace(',', "\n");
            let privkey = privkey + "\n";
            println!("{privkey}");
            match fs::write(format!("./out/{keyname}"), privkey).await {
                Ok(_) => info!("Private key {keyname} extracted."),
                Err(e) => warn!("Private key: {e}"),
            }
            break;
        }
    }

    Ok(())
}

pub async fn extract_host_ip(log_msg: &str) -> anyhow::Result<String> {
    let lines = log_msg.split('\n');

    for l in lines.into_iter() {
        if l.len() < 42 {
            continue;
        }
        let rm_prefix = &l[29..];
        if !rm_prefix.starts_with("Host ") {
            continue;
        }
        let ip = rm_prefix.split(' ').nth(1).context("Can't parse ssh IP")?;
        return Ok(ip.to_string());
    }

    Err(anyhow!("Can't parse ssh IP"))
}
