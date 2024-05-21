use std::env;

use futures::{future::join_all, FutureExt};
use reqwest::{Error, Response};

#[tokio::main]
async fn main() {
    // Parse Github project name
    let project_name = env::args()
        .nth(1)
        .expect("First argument must be a project name.");
}

async fn get_action_links(project_name: &str, pagination_limit: u32) -> Result<Vec<String>, Error> {
    let main_url = format!("https://github.com/{project_name}/actions?page=");

    let mut site_futures = Vec::with_capacity(pagination_limit as usize);
    for i in 1..pagination_limit {
        let resp = reqwest::get(format!("{main_url}{i}"));
        site_futures.push(resp);
    }

    let site_responses = join_all(site_futures).await;

    let mut results = Vec::with_capacity(pagination_limit as usize);
    for s in site_responses {
        results.push(s?.text().await?);
    }

    Ok(results)
}
