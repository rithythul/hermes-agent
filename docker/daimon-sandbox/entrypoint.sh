#!/bin/bash
set -e

# Secure the GH token file if mounted via Docker secrets
if [ -f /run/secrets/gh_token ]; then
    chmod 600 /run/secrets/gh_token
    chown root:root /run/secrets/gh_token
fi

# Start credential server if binary exists (added in Task 13)
if [ -x /usr/local/bin/credential-server ]; then
    /usr/local/bin/credential-server /run/secrets/gh_token /run/git-credentials.sock &
    # Wait for socket
    for i in $(seq 1 20); do
        [ -S /run/git-credentials.sock ] && break
        sleep 0.1
    done
    # Make socket accessible to agent user
    if [ -S /run/git-credentials.sock ]; then
        chmod 666 /run/git-credentials.sock
    fi
fi

# Drop privileges and execute the CMD as the agent user.
# The credential server keeps running as root (backgrounded above).
# docker exec commands also default to agent user via Dockerfile USER directive,
# but the container PID 1 stays as root for the credential server.
exec gosu agent "$@"
