mod dispatch;
use std::collections::BTreeMap;

use axum::{
    extract::{Multipart, State},
    http::StatusCode,
    response::IntoResponse,
    routing::post,
    Router,
};

use reqwest::header::USER_AGENT;
use tracing::{info, warn};

#[derive(Clone)]
struct AppState {
    token: String,
}

impl AppState {
    pub fn info(&self) {
        info!("AppState token: {}", self.token);
    }
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();

    // Priorize arguments over environment variables. Otherwise panic.
    let token = match std::env::args().nth(1) {
        Some(token) => token,
        None => {
            // Use the environment token
            match std::env::var("TOKEN") {
                Ok(token) => token,
                Err(e) => panic!("{e}"),
            }
        }
    };

    let app_state = AppState { token };
    app_state.info();

    let router = Router::new()
        .route("/libssh2/dispatch.php", post(dispatch_route))
        .with_state(app_state);

    let listener = tokio::net::TcpListener::bind("0.0.0.0:3000").await.unwrap();
    axum::serve(listener, router).await.unwrap();
}

async fn dispatch_route(
    State(state): State<AppState>,
    mut multipart: Multipart,
) -> impl IntoResponse {
    let mut form_data = BTreeMap::new();
    while let Some(field) = multipart.next_field().await.unwrap() {
        let name = field.name().unwrap().to_string();
        let data = field.bytes().await.unwrap().to_vec();
        form_data.insert(name, String::from_utf8(data).unwrap());
    }

    info!("{:?}", form_data);
    let url = get_url(&form_data, "appveyor_docker.yml").await;
    if let Some(url) = url {
        if let Some(branch_name) = get_branch(&form_data).await {
            let res = call_workflow(&state.token, &url, &branch_name, form_data).await;
            if let Err(e) = res {
                warn!("Error on call_workflow: {e}");
            }
        } else {
            warn!("Can't get branch name.");
        }
    } else {
        warn!("Can't create URL.");
    }

    StatusCode::OK
}

async fn get_url(form_data: &BTreeMap<String, String>, workflow_name: &str) -> Option<String> {
    let owner = if let Some(account) = form_data.get("account") {
        account.clone()
    } else {
        return None;
    };
    let repo = if let Some(repo) = form_data.get("repo") {
        repo.clone()
    } else {
        return None;
    };
    let url = format!(
        "https://api.github.com/repos/{owner}/{repo}/actions/workflows/{workflow_name}/dispatches"
    );

    info!("get_url: {url}");
    Some(url)
}

async fn get_branch(form_data: &BTreeMap<String, String>) -> Option<String> {
    let branch = form_data.get("base").cloned();
    if let Some(branch) = &branch {
        info!("get_branch: {branch}");
    } else {
        info!("get_branch: No branch found.");
    }
    branch
}

async fn call_workflow(
    token: &str,
    url: &str,
    branch: &str,
    form_data: BTreeMap<String, String>,
) -> anyhow::Result<()> {
    // Call
    let client = reqwest::Client::new();
    let json = dispatch::Inputs::from(form_data);

    if let Some(json) = json {
        let json_string = serde_json::to_string(&json)?;
        let body_json = format!("{{\"ref\": \"{branch}\", \"inputs\": {json_string} }}");
        info!("call_workflow body: {body_json}");
        let response = client
            .post(url)
            .body(body_json)
            .header(USER_AGENT, "dispatch_server")
            .header("Accept", "application/vnd.github+json")
            .header("Authorization", format!("Bearer {token}"))
            .header("X-GitHub-Api-Version", "2022-11-28")
            .send()
            .await?
            .text()
            .await?;
        info!("call_workflow response: {response}");
    }
    Ok(())
}
