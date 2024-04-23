mod dispatch;
use std::collections::BTreeMap;

use axum::{extract::Multipart, http::StatusCode, response::IntoResponse, routing::post, Router};

use tracing::info;

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();

    let router = Router::new().route("/libssh2/dispatch.php", post(dispatch_route));

    let listener = tokio::net::TcpListener::bind("0.0.0.0:3000").await.unwrap();
    axum::serve(listener, router).await.unwrap();
}

async fn dispatch_route(mut multipart: Multipart) -> impl IntoResponse {
    let mut form_data = BTreeMap::new();
    while let Some(field) = multipart.next_field().await.unwrap() {
        let name = field.name().unwrap().to_string();
        let data = field.bytes().await.unwrap().to_vec();
        form_data.insert(name, String::from_utf8(data).unwrap());
    }

    info!("{:?}", form_data);

    StatusCode::OK
}
