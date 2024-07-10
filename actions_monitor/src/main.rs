mod zip;

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

#[tokio::main]
async fn main() -> anyhow::Result<()> {
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

    info!("Token: {token}");
    info!("Owner: {owner}");
    info!("Repository: {repo}");

    let client = authclient!(token);
    let repo_workflows = get_repo_workflows(client, &owner, &repo).await?;
    let runs = repo_workflows.get("workflow_runs").unwrap();

    if let Some(wfruns) = runs.as_array() {
        // Retrieve the run IDs
        let mut ids = wfruns
            .iter()
            .filter(|x| {
                if let Some(name) = x.get("name") {
                    name == "AppVeyor Docker Bridge"
                } else {
                    false
                }
            })
            .filter_map(|x| x.get("id"))
            .filter_map(|x| x.as_i64())
            .collect::<Vec<i64>>();

        // Sort descending so the most recent run will be first
        ids.sort();
        ids.reverse();

        download_workflow_logs(client, &owner, &repo, &ids[0].to_string())
            .await
            .unwrap();
        let filename = format!("./out/{owner}_{repo}_{}.zip", &ids[0]);
        zip::extract_zip(&filename, &format!("./out/{owner}_{repo}_{}", &ids[0]));
    }

    Ok(())
}
