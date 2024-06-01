use anyhow::bail;
use futures::future::join_all;
use regex::Regex;
use std::{collections::HashSet, sync::Arc};
use tokio::fs;

#[derive(PartialEq)]
pub enum Actions {
    ActionSites(Vec<String>),
    RunSites(Vec<(String, String)>),
    RunJobCommit(Vec<(String, String, String)>),
    Done,
}

impl Actions {
    pub async fn new(project_name: &str, pagination_limit: u32) -> Result<Self, reqwest::Error> {
        let actions = get_action_sites(project_name, pagination_limit).await;
        match actions {
            Ok(actions) => Ok(Actions::ActionSites(actions)),
            Err(e) => Err(e),
        }
    }

    pub async fn next_step(self, project_name: &str) -> anyhow::Result<Actions> {
        match self {
            Actions::ActionSites(actions_sites) => {
                let parsed = retrieve_run_urls(project_name, actions_sites).await;
                match parsed {
                    Ok(p) => Ok(Actions::RunSites(p)),
                    Err(e) => bail!(e),
                }
            }
            Actions::RunSites(action_urls) => {
                let response = get_run_sites(action_urls).await;
                match response {
                    Ok(run_sites) => {
                        let run_job_commit = retrieve_job_commit_ids(run_sites).await;
                        match run_job_commit {
                            Ok(run_job_commit) => Ok(Actions::RunJobCommit(run_job_commit)),
                            Err(e) => bail!(e),
                        }
                    }
                    Err(e) => bail!(e),
                }
            }
            Actions::RunJobCommit(run_job_commit) => {
                let download = download_log(project_name, run_job_commit).await;
                match download {
                    Ok(_) => Ok(Actions::Done),
                    Err(e) => bail!(e),
                }
            }
            Actions::Done => bail!("There's no next step."),
        }
    }

    pub async fn len(&self) -> usize {
        match self {
            Actions::ActionSites(v) => v.len(),
            Actions::RunSites(v) => v.len(),
            Actions::RunJobCommit(v) => v.len(),
            Actions::Done => 0,
        }
    }

    pub async fn get_runs_set(&self) -> anyhow::Result<HashSet<(String, String)>> {
        if let Actions::RunSites(sites) = &self {
            let set =
                HashSet::from_iter(sites.iter().map(|(url, runid)| (url.into(), runid.into())));
            Ok(set)
        } else {
            bail!("No actions in set")
        }
    }

    pub async fn from_runs_set<R>(runs: R) -> Self
    where
        R: IntoIterator<Item = (String, String)>,
    {
        Actions::RunSites(runs.into_iter().collect())
    }
}

/// Get html from Github project. Each element in the vector is a pagination site.
async fn get_action_sites(
    project_name: &str,
    pagination_limit: u32,
) -> Result<Vec<String>, reqwest::Error> {
    let main_url = format!("https://github.com/{project_name}/actions?page=");

    // Create a single reusable client
    let client = reqwest::Client::new();

    // Create all site requests
    let mut site_futures = Vec::with_capacity(pagination_limit as usize);
    for i in 1..=pagination_limit {
        let resp = client.get(format!("{main_url}{i}")).send();
        site_futures.push(resp);
    }

    // Fire all site requests and wait for the responses
    let site_responses = join_all(site_futures).await;

    // Retrieve the results as a vector of texts
    let mut results = Vec::with_capacity(pagination_limit as usize);
    for s in site_responses {
        results.push(s?.text().await?);
    }

    Ok(results)
}

/// Retrieve action run urls from html.
/// The tuple contains the full url and the extracted run id.
async fn retrieve_run_urls(
    project_name: &str,
    action_sites: Vec<String>,
) -> Result<Vec<(String, String)>, regex::Error> {
    // Generate regex string
    let mut regex = String::with_capacity(40);
    regex.push_str(r#"a href=\"(\/"#);
    regex.push_str(&project_name.replace('/', r"\/"));
    regex.push_str(r#"\/actions\/runs\/(\d+))\""#);
    let re = Regex::new(&regex)?;

    let mut urls: Vec<(String, String)> = Vec::with_capacity(25);
    for s in action_sites {
        //let captures = re.captures_iter(&s);
        for (_, [url, run_id]) in re.captures_iter(&s).map(|c| c.extract()) {
            urls.push((url.into(), run_id.into()));
        }
    }
    Ok(urls)
}

/// Get the run site for each action
async fn get_run_sites(action_urls: Vec<(String, String)>) -> Result<Vec<String>, reqwest::Error> {
    let client = reqwest::Client::new();

    let mut futures = Vec::with_capacity(25);
    for (url, _) in action_urls {
        futures.push(client.get(format!("https://github.com{url}")).send());
    }

    let responses = join_all(futures).await;

    let mut results = Vec::with_capacity(25);
    for s in responses {
        results.push(s?.text().await?);
    }

    Ok(results)
}

/// Retrieves the run-id, job-id and commit hash from the run sites
async fn retrieve_job_commit_ids(
    run_sites: Vec<String>,
) -> Result<Vec<(String, String, String)>, regex::Error> {
    // Get the job id

    let re = Regex::new(r#"\/actions\/runs\/(?<runid>\d+)\/job\/(?<jobid>\d+)""#)?;
    let re_commit = Regex::new(r#"\/commit\/(?<commit>[a-f0-9]+)"#)?;

    let mut job_ids: Vec<(String, String, String)> = Vec::with_capacity(25);
    for r in run_sites {
        if let Some(capgroups) = re.captures(&r) {
            let run_id = &capgroups["runid"];
            let job_id = &capgroups["jobid"];

            // Only check the commit capture if the first was successful
            if let Some(capcommit) = re_commit.captures(&r) {
                let commit = &capcommit["commit"];

                job_ids.push((run_id.into(), job_id.into(), commit.into()));
            } else {
                // These runs were triggered by a pull request from a foreign repository.
                // Therefore, there is no commit id
                eprintln!("{run_id}, {job_id}, Commit not found.");
            }
        } else {
            eprintln!("Run-ID and Job-ID not found")
        }
    }

    Ok(job_ids)
}

async fn download_log(
    project_name: &str,
    run_job_commit: Vec<(String, String, String)>,
) -> Result<(), reqwest::Error> {
    // Build a client with the Github session
    let cookie_store = {
        let file = std::fs::File::open("session.json")
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

    // Download files as authenticated user
    let mut files = Vec::with_capacity(25);
    for (run, job, commit) in run_job_commit {
        let log_url =
            format!("https://github.com/{project_name}/commit/{commit}/checks/{job}/logs");

        let text = client.get(log_url).send().await?.text().await?;
        files.push(fs::write(format!("./out/{run}_{job}_{commit}.txt"), text));
    }

    // Create directory
    if let Err(e) = fs::create_dir("./out").await {
        eprintln!("Creating directory: {e}");
    }

    // Write all files
    let results = join_all(files).await;

    // Print all file errors
    results.iter().for_each(|x| {
        if let Err(e) = x {
            eprintln!("Creating file: {e}")
        }
    });

    Ok(())
}
