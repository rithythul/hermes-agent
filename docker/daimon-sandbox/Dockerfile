FROM python:3.12-slim AS base

# System dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl wget jq build-essential gcc g++ make \
    openssh-client ca-certificates gnupg \
    && rm -rf /var/lib/apt/lists/*

# Install uv
RUN curl -LsSf https://astral.sh/uv/install.sh | sh
ENV PATH="/root/.local/bin:$PATH"

# Install Node.js 20
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

# Install gh CLI
RUN curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
    | dd of=/usr/share/keyrings/githubcli-archive-keyring.gpg \
    && chmod go+r /usr/share/keyrings/githubcli-archive-keyring.gpg \
    && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
    | tee /etc/apt/sources.list.d/github-cli.list > /dev/null \
    && apt-get update && apt-get install -y gh \
    && rm -rf /var/lib/apt/lists/*

# Install gosu for privilege dropping in entrypoint
RUN apt-get update && apt-get install -y --no-install-recommends gosu \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user (no sudo access)
RUN useradd -m -u 1000 -s /bin/bash agent

# Create workspace root
RUN mkdir -p /workspaces && chown agent:agent /workspaces

# Create directory for hermes-agent clone (populated externally or at first boot)
RUN mkdir -p /opt/hermes-agent && chown agent:agent /opt/hermes-agent

# Create secrets directory (credential server reads from here)
RUN mkdir -p /run/secrets

# Git config for the agent user
RUN su agent -c 'git config --global user.name "daimon[bot]"' \
    && su agent -c 'git config --global user.email "daimon[bot]@nousresearch.com"' \
    && su agent -c 'git config --global credential.helper daimon' \
    && su agent -c 'git config --global init.defaultBranch main'

# Build credential server
COPY credential-server.c /tmp/credential-server.c
RUN gcc -O2 -Wall -o /usr/local/bin/credential-server /tmp/credential-server.c \
    && rm /tmp/credential-server.c

# Install credential helper client
COPY git-credential-daimon /usr/local/bin/git-credential-daimon
RUN chmod 755 /usr/local/bin/git-credential-daimon

# Default working directory
WORKDIR /workspaces

COPY entrypoint.sh /entrypoint.sh
RUN chmod 755 /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["sleep", "infinity"]
