use std::collections::BTreeMap;

use reqwest::{header, Client};
use serde_json::Value;
use tokio::fs;
use tracing::{info, warn};

macro_rules! authclient {
    ($s:expr) => {{
        static RE: once_cell::sync::OnceCell<reqwest::Client> = once_cell::sync::OnceCell::new();
        RE.get_or_init(|| {
            // Add necessary Github API headers
            let mut headers = header::HeaderMap::new();
            headers.insert(
                "Accept",
                header::HeaderValue::from_static("application/vnd.github+json"),
            );
            headers.insert(
                "X-GitHub-Api-Version",
                header::HeaderValue::from_static("2022-11-28"),
            );
            let bearer = format!("Bearer {}", $s);
            let mut secret = header::HeaderValue::from_str(&bearer).unwrap();
            secret.set_sensitive(true);
            headers.insert("Authorization", secret);

            // Build client with a user_agent name and HeaderMap
            reqwest::Client::builder()
                .user_agent("actions_monitor")
                .default_headers(headers)
                .build()
                .unwrap()
        })
    }};
}

async fn get_repo_workflows(
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

async fn download_workflow_logs(
    client: &Client,
    owner: &str,
    repo: &str,
    run_id: &str,
) -> anyhow::Result<()> {
    let response_header = client
        .get(format!(
            "https://api.github.com/repos/{owner}/{repo}/actions/runs/{run_id}/logs"
        ))
        .send()
        .await?;
    if let Some(location) = response_header.headers().get("Location") {
        if let Ok(loc) = location.to_str() {
            // Get the actual logs
            let logs = client.get(loc).send().await?.text().await?;
            match fs::write(format!("./out/{owner}_{repo}_{run_id}.zip"), logs).await {
                Ok(_) => info!("Log {owner}/{repo} run id: {run_id} downloaded."),
                Err(e) => warn!("Log download: {e}"),
            }
        }
    }
    Ok(())
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();

    // Use the environment variables for configuration
    let token = match std::env::var("TOKEN") {
        Ok(token) => token,
        Err(e) => panic!("{e}"),
    };
    let owner = match std::env::var("OWNER") {
        Ok(owner) => owner,
        Err(e) => panic!("{e}"),
    };
    let repo = match std::env::var("REPO") {
        Ok(repo) => repo,
        Err(e) => panic!("{e}"),
    };

    let client = authclient!(token);
    get_repo_workflows(client, &owner, &repo).await.unwrap();
}
