FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        cmake \
        flex \
        make \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY . .

CMD ["sh", "scripts/docker-generate.sh"]
