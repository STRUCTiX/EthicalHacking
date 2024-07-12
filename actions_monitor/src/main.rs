mod zip;

use std::{collections::BTreeMap, sync::Arc, time::Duration};

use anyhow::Context;
use reqwest::{header, Client};
use serde_json::Value;
use tokio::{fs, time::sleep};
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

macro_rules! reqauthclient {
    ($s:literal) => {{
        static RE: once_cell::sync::OnceCell<reqwest::Client> = once_cell::sync::OnceCell::new();
        RE.get_or_init(|| {
            // Build a client with the Github session
            let cookie_store = {
                let file = std::fs::File::open($s)
                    .map(std::io::BufReader::new)
                    .unwrap();

                // use re-exported version of `CookieStore` for crate compatibility
                reqwest_cookie_store::CookieStore::load_json(file).unwrap()
            };
            let cookie_store = reqwest_cookie_store::CookieStoreMutex::new(cookie_store);
            let cookie_store = Arc::new(cookie_store);

            // Create a reqwest client with the Github user session cookie
            let client = reqwest::Client::builder()
                .cookie_provider(Arc::clone(&cookie_store))
                .build()
                .unwrap();
            return client;
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

async fn get_workflow_jobs(
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
    loop {
        let repo_workflows = get_repo_workflows(client, &owner, &repo).await?;
        let runs = repo_workflows.get("workflow_runs").unwrap();

        if let Some(wfruns) = runs.as_array() {
            // Retrieve the run IDs
            let mut ids = wfruns
                .iter()
                .filter(|x| {
                    if let Some(status) = x.get("status") {
                        status != "completed"
                    } else {
                        false
                    }
                })
                .filter(|x| {
                    if let Some(name) = x.get("name") {
                        name == "AppVeyor Docker Bridge"
                    } else {
                        false
                    }
                })
                .filter_map(|x| {
                    if let Some(head_sha) = x.get("head_sha") {
                        if let Some(id) = x.get("id") {
                            if let Some(head) = head_sha.as_str() {
                                if let Some(id) = id.as_i64() {
                                    return Some((id, head.to_string()));
                                }
                            }
                        }
                    }
                    None
                })
                .collect::<Vec<(i64, String)>>();

            //if !ids.is_empty() {
            //    // Sort descending so the most recent run will be first
            //    ids.sort();
            //    ids.reverse();
            //    info!("Download workflow {}", &ids[0].to_string());

            //    download_workflow_logs(client, &owner, &repo, &ids[0].to_string())
            //        .await
            //        .unwrap();
            //    let filename = format!("./out/{owner}_{repo}_{}.zip", &ids[0]);
            //    zip::extract_zip(&filename, &format!("./out/{owner}_{repo}_{}", &ids[0]));
            //} else {
            //    info!("All workflows are completed");
            //}

            if !ids.is_empty() {
                ids.sort_by_key(|x| x.0);
                ids.reverse();
                let id = ids[0].0;
                let commit_sha = &ids[0].1;
                info!("Download log id: {id}, commit: {commit_sha}");

                // Retrieve job id from jobs endpoint
                let job_ids = get_workflow_jobs(client, &owner, &repo, id).await?;
                let job_ids = job_ids.get("jobs").unwrap();
                if let Some(job_ids) = job_ids.as_array() {
                    let job_ids = job_ids
                        .iter()
                        .filter_map(|x| x.get("id"))
                        .filter_map(|x| x.as_i64())
                        .collect::<Vec<i64>>();

                    // Download the current log with the cookie session
                    let log = get_running_log(&owner, &repo, commit_sha, job_ids[0], 2).await?;
                    info!("{log}");

                    // Extract private key
                    let filename = format!("{owner}_{repo}_{commit_sha}_{}", job_ids[0]);
                    store_private_key(&log, &filename).await?;
                    let ssh_command =
                        get_ssh_login_info(&owner, &repo, commit_sha, job_ids[0], 3).await?;
                    let full_command = format!("{ssh_command} -i ./out/{filename}");
                    info!("{full_command}");
                }
            } else {
                info!("No running job");
            }
        }
        sleep(Duration::from_secs(10)).await;
    }
}
