use std::{env, io::Read, sync::Arc};

use futures::future::join_all;
use regex::Regex;
use tokio::fs;

#[tokio::main]
async fn main() {
    // Parse Github project name
    let project_name = env::args()
        .nth(1)
        .expect("First argument must be a project name.");

    let results = get_action_sites(&project_name, 1).await.unwrap();
    let urls = retrieve_run_urls(&project_name, results).await.unwrap();
    for (url, run) in &urls {
        println!("url: {url}, run: {run}");
    }
    let run_sites = get_run_sites(urls).await.unwrap();
    println!("run sites: {}", run_sites.len());
    let job_ids = retrieve_job_commit_ids(run_sites).await.unwrap();
    println!("job_ids: {}", job_ids.len());

    for (run, job, commit) in &job_ids {
        println!("Run: {run}, Job: {job}, Commit: {commit}");
    }

    let job_ids = vec![("s".into(), "a".into(), "asdf".into())];

    download_log(&project_name, job_ids).await.unwrap();
}

/// Get html from Github project. Each element in the vector is a pagination site.
async fn get_action_sites(
    project_name: &str,
    pagination_limit: u32,
) -> Result<Vec<String>, reqwest::Error> {
    let main_url = format!("https://github.com/{project_name}/actions?page=");

    // Create all site requests
    let mut site_futures = Vec::with_capacity(pagination_limit as usize);
    for i in 1..=pagination_limit {
        let resp = reqwest::get(format!("{main_url}{i}"));
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
    let mut futures = Vec::with_capacity(25);
    for (url, _) in action_urls {
        futures.push(reqwest::get(format!("https://github.com{url}")));
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

    let client = reqwest::Client::builder()
        .cookie_provider(Arc::clone(&cookie_store))
        .build()
        .unwrap();

    let mut files = Vec::with_capacity(25);
    for (run, job, commit) in run_job_commit {
        let log_url =
            format!("https://github.com/{project_name}/commit/{commit}/checks/{job}/logs");

        let text = client.get(log_url).send().await?.text().await?;
        files.push(fs::write(format!("./out/{run}_{job}_{commit}.txt"), text));
    }

    // Create directory
    if let Err(e) = fs::create_dir("./out").await {
        eprintln!("{e}");
    }

    // Write all files
    let results = join_all(files).await;

    // Print all file errors
    results.iter().for_each(|x| {
        if let Err(e) = x {
            eprintln!("{e}")
        }
    });

    Ok(())
}
