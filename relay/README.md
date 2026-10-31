# Relay

The relay starts an HTTPS API server on the `API_Port` in `relay-config.json`
(8080 by default). The server currently has no API routes. It creates `key.pem`
and `cert.pem` in `RELAY_CONFIG_DIR`, or beside the executable if that variable
is unset.

The relay stores its API key in `api-key` in that same directory. It generates
and prints a key on first startup. Run `obs-relay --generate-api-key` with the
same `RELAY_CONFIG_DIR` to replace the key and print the new one; running relay
servers use the replacement for subsequent requests. Future API handlers can
call `authorize` to require the key in the `Authorization` header.

Build the Docker image from the repository root so the shared certificate code
is included:

```sh
docker build -f relay/Dockerfile -t obs-relay .
```
