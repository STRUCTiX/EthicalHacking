use std::collections::BTreeMap;

use anyhow::Context;
use reqwest::Client;
use serde_json::Value;
use tokio::fs;
use tracing::{info, warn};

use crate::reqauthclient;

pub async fn get_repo_workflows(
    client: &Client,
    owner: &str,
    repo: &str,
) -> anyhow::Result<BTreeMap<String, Value>> {
    Ok(client
        .get(format!(
            "https://api.github.com/repos/{owner}/{repo}/actions/runs"
        ))
        .send()
        .await?
        .json::<BTreeMap<String, Value>>()
        .await?)
}

pub async fn get_workflow_jobs(
    client: &Client,
    owner: &str,
    repo: &str,
    run_id: i64,
) -> anyhow::Result<BTreeMap<String, Value>> {
    Ok(client
        .get(format!(
            "https://api.github.com/repos/{owner}/{repo}/actions/runs/{run_id}/jobs"
        ))
        .send()
        .await?
        .json::<BTreeMap<String, Value>>()
        .await?)
}

pub async fn download_workflow_logs(
    client: &Client,
    owner: &str,
    repo: &str,
    run_id: &str,
) -> anyhow::Result<()> {
    let logs_zip = client
        .get(format!(
            "https://api.github.com/repos/{owner}/{repo}/actions/runs/{run_id}/logs"
        ))
        .send()
        .await?
        .bytes()
        .await?;

    if let Err(e) = fs::create_dir("./out").await {
        info!("{e}");
    }

    match fs::write(format!("./out/{owner}_{repo}_{run_id}.zip"), logs_zip).await {
        Ok(_) => info!("Log {owner}/{repo} run id: {run_id} downloaded."),
        Err(e) => warn!("Log download: {e}"),
    }
    Ok(())
}

/// This function requires a valid Github session cookie
pub async fn get_running_log(
    owner: &str,
    repo: &str,
    commit_sha: &str,
    job_id: i64,
    log_num: u32,
) -> anyhow::Result<String> {
    let client = reqauthclient!("session.json");

    let url = format!(
        "https://github.com/{owner}/{repo}/commit/{commit_sha}/checks/{job_id}/logs/{log_num}"
    );

    info!(url);

    let log = client.get(&url).send().await?.text().await?;

    Ok(log)
}

/// This function requires a valid Github session cookie
pub async fn get_ssh_login_info(
    owner: &str,
    repo: &str,
    commit_sha: &str,
    job_id: i64,
    log_num: u32,
) -> anyhow::Result<String> {
    let client = reqauthclient!("session.json");

    let url = format!(
        "https://github.com/{owner}/{repo}/commit/{commit_sha}/checks/{job_id}/logs/{log_num}"
    );

    info!(url);

    let log = client.get(&url).send().await?.text().await?;
    info!(log);
    let ssh_info = log.split_once('\n').context("Can't split log")?;
    let ssh_info = ssh_info.0.to_owned();
    let ssh_info = &ssh_info[42..];
    let ssh_info = ssh_info
        .strip_suffix(" sleep 1h")
        .context("Could not strip suffix")?
        .to_owned();
    info!("{ssh_info}");

    Ok(ssh_info)
}
