use std::env;

use futures::future::join_all;
use regex::Regex;

#[tokio::main]
async fn main() {
    // Parse Github project name
    let project_name = env::args()
        .nth(1)
        .expect("First argument must be a project name.");

    let results = get_action_sites(&project_name, 1).await.unwrap();
    let urls = retrieve_run_urls(&project_name, results).await.unwrap();
    for (url, run) in urls {
        println!("url: {url}, {run}");
    }
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
    regex.push_str(&project_name.replace("/", r"\/"));
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

async fn get_run_sites(action_urls: Vec<(String, String)>) -> Result<Vec<String>, reqwest::Error> {
    //let re = Regex::new(r#"\/actions\/runs\/(\d+)\/job\/(\d+)""#)?;

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
