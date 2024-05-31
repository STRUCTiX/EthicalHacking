mod actions;
mod json_format;

use futures::future::join_all;
use json_format::CookieEditor;
use regex::Regex;
use std::{env, sync::Arc};
use tokio::{
    fs::{self, File},
    io::AsyncWriteExt,
};

#[tokio::main]
async fn main() {
    // Parse Github project name
    let project_name = env::args()
        .nth(1)
        .expect("First argument must be a project name.");

    // Convert json
    if project_name == "-c" {
        let file = std::fs::File::open("github_session.json")
            .map(std::io::BufReader::new)
            .unwrap();

        let cookie = CookieEditor::from(file);
        println!("{}", cookie.convert());
        let mut file = File::create("session.json").await.unwrap();
        file.write_all(cookie.convert().as_bytes()).await.unwrap();
        return;
    }

    // Retrieve action sites and parse the action urls
    // CHANGE: amount sites to desired value
    let amount_sites = 1;
    let results = actions::Actions::new(&project_name, amount_sites)
        .await
        .unwrap();
    let urls = results.next_step(&project_name).await.unwrap();
    println!("urls: {}", urls.len().await);

    // Call actions and retrieve the run sites with run-id and job-id
    let run_sites = urls.next_step(&project_name).await.unwrap();
    println!("run sites: {}", run_sites.len().await);
    let job_ids = run_sites.next_step(&project_name).await.unwrap();
    //println!("job_ids: {}", job_ids.len().await);

    if let actions::Actions::RunJobCommit(job_ids) = &job_ids {
        for (run, job, commit) in job_ids {
            println!("Run: {run}, Job: {job}, Commit: {commit}");
        }
    }

    // Download the workflow logs
    if let Err(e) = job_ids.next_step(&project_name).await {
        eprintln!("{e}");
    }
}
