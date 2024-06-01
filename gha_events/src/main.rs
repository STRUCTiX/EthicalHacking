mod actions;
mod json_format;

use json_format::CookieEditor;
use std::{env, time::Duration};
use tokio::{fs::File, io::AsyncWriteExt, time::interval};

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

    let download_once = false;
    if download_once {
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

        if let actions::Actions::RunJobCommit(job_ids) = &job_ids {
            for (run, job, commit) in job_ids {
                println!("Run: {run}, Job: {job}, Commit: {commit}");
            }
        }

        // Download the workflow logs
        if let Err(e) = job_ids.next_step(&project_name).await {
            eprintln!("{e}");
        }
    } else {
        // Start monitoring

        // Download the run ids for the first time
        let reference_ids = actions::Actions::new(&project_name, 1)
            .await
            .expect("Could not download reference ids");

        // parse the run_ids
        let mut reference_ids = reference_ids
            .next_step(&project_name)
            .await
            .unwrap()
            .get_runs_set()
            .await
            .unwrap();

        let mut interval = interval(Duration::from_secs(20));
        loop {
            interval.tick().await;
            println!("Tick");

            let current_ids = actions::Actions::new(&project_name, 1).await;
            let current_ids = match current_ids {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("{e}");
                    continue;
                }
            };

            let run_ids = current_ids.next_step(&project_name).await;
            let run_ids = match run_ids {
                Ok(r) => r,
                Err(e) => {
                    eprintln!("{e}");
                    continue;
                }
            };
            let runs_set = run_ids.get_runs_set().await;
            let runs_set = match runs_set {
                Ok(r) => r,
                Err(e) => {
                    eprintln!("{e}");
                    continue;
                }
            };

            // Check if there's a difference between reference set and runs set
            let difference = runs_set
                .difference(&reference_ids)
                .map(|f| f.to_owned())
                .collect::<Vec<(String, String)>>();

            if !difference.is_empty() {
                // We've found a new entry
                let action = actions::Actions::from_runs_set(difference).await;

                // Download stuff
                let run_job_commit = action.next_step(&project_name).await;
                let run_job_commit = match run_job_commit {
                    Ok(r) => r,
                    Err(e) => {
                        eprintln!("{e}");

                        // Just try again.
                        continue;
                    }
                };
                let done = run_job_commit.next_step(&project_name).await;
                if let Ok(d) = done {
                    if d == actions::Actions::Done {
                        // replace the reference_ids with the runs_set
                        reference_ids = runs_set;
                        println!("Download finished.");
                    }
                }
            } else {
                println!("No new job found");
            }
        }
    }
}
