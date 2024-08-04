#[macro_export]
macro_rules! authclient {
    ($s:expr) => {{
        use reqwest::header;
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

#[macro_export]
macro_rules! reqauthclient {
    ($s:literal) => {{
        use std::sync::Arc;
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
