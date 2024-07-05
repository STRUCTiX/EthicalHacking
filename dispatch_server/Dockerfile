FROM lukemathwalker/cargo-chef:latest-rust-alpine AS chef
WORKDIR /src

FROM chef AS planner
COPY . .
# Create dependencies
RUN cargo chef prepare --recipe-path /src/recipe.json

FROM chef AS builder
COPY --from=planner /src/recipe.json /src/recipe.json
RUN apk add musl-dev openssl-dev openssl-libs-static
# Build dependencies
RUN cargo chef cook --release --recipe-path /src/recipe.json
# Build application
COPY . .
RUN cargo build --release --bin dispatch_server

# Use scratch image to reduce image size
FROM scratch
ARG version=0.1.0
ARG release=unreleased
LABEL name="dispatch_server" \
      maintainer="structix" \
      version=${version} \
      release=${release} \
      summary="A dispatch server connecting AppVeyor and Github Actions" \
      description="A dispatch server connecting AppVeyor and Github Actions"
EXPOSE 3000

COPY --from=builder /src/target/release/dispatch_server /
CMD ["./dispatch_server"]

