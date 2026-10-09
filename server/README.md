# scout-regiment server (Go)

Receives `POST /client-info` from the scout client, records every request
(form fields, zip MD5, client IP, timestamp) into a local SQLite database,
and stores the zip file in S3 unless its MD5 is blacklisted.

## Build

```bash
cd server
go build -o scoutreg-server .
```

No CGO is required (pure-Go SQLite driver).

## Run

```bash
./scoutreg-server -port 8080
```

| Flag        | Default            | Description                                  |
|-------------|--------------------|----------------------------------------------|
| `-port`     | `8080`             | HTTP port to listen on                       |
| `-db`       | `scoutreg.db`      | Path to the SQLite database file             |
| `-blacklist`| `md5_blacklist.txt`| Path to the MD5 blacklist file               |

The database and the blacklist file are resolved relative to the current
working directory.

### Configuration

S3 credentials are read from the `.env` file. The loader looks for `.env`
in the current directory first, then in the parent directory (repository
root), so both of these work:

```bash
cd server && ./scoutreg-server            # loads ../.env
cd <repo-root> && ./server/scoutreg-server # loads ./.env
```

Expected variables (see the repository-root `.env.example`):

```env
S3_ENDPOINT=http://localhost:9000
S3_REGION=garage
S3_BUCKET=scout-reg-bucket
S3_ACCESS_KEY=minioadmin
S3_SECRET_KEY=minioadmin
```

The server creates the bucket on startup if it does not exist yet.

### MD5 blacklist

`md5_blacklist.txt` holds one MD5 hash per line (`#` starts a comment).
When an uploaded zip matches a blacklist entry, the request is still
recorded in the database (`blacklisted = 1`, `s3_url` NULL) but the file
is **not** uploaded to S3.

## Database

All data lives in a single table:

```sql
CREATE TABLE reports (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    hostname      TEXT NOT NULL,
    username      TEXT NOT NULL,
    cpu_arch      TEXT NOT NULL,
    cpu_cores     TEXT NOT NULL,
    total_ram     TEXT NOT NULL,
    available_ram TEXT NOT NULL,
    client_ip     TEXT NOT NULL,
    file_name     TEXT,
    file_md5      TEXT NOT NULL,
    s3_url        TEXT,
    blacklisted   INTEGER NOT NULL DEFAULT 0,
    created_at    TEXT NOT NULL
);
```

### SQLite oneliner for a clean database

```bash
rm -f scoutreg.db && sqlite3 scoutreg.db "CREATE TABLE reports (id INTEGER PRIMARY KEY AUTOINCREMENT, hostname TEXT NOT NULL, username TEXT NOT NULL, cpu_arch TEXT NOT NULL, cpu_cores TEXT NOT NULL, total_ram TEXT NOT NULL, available_ram TEXT NOT NULL, client_ip TEXT NOT NULL, file_name TEXT, file_md5 TEXT NOT NULL, s3_url TEXT, blacklisted INTEGER NOT NULL DEFAULT 0, created_at TEXT NOT NULL);"
```

Useful queries:

```bash
# all reports
sqlite3 scoutreg.db "SELECT * FROM reports;"

# blacklisted uploads only
sqlite3 scoutreg.db "SELECT client_ip, file_md5, created_at FROM reports WHERE blacklisted=1;"
```

### aws-cli oneliner to clear bucket
```bash
set -a && source .env && set +a && AWS_ACCESS_KEY_ID="$S3_ACCESS_KEY" AWS_SECRET_ACCESS_KEY="$S3_SECRET_KEY" AWS_DEFAULT_REGION="$S3_REGION" aws s3 rm "s3://$S3_BUCKET" --recursive --endpoint-url "$S3_ENDPOINT"
```

## Testing with the client

From the repository root:

```bash
cd ./out/client
./scoutreg
```

The client POSTs to `http://127.0.0.1:8080/client-info` (see the XOR-encoded
`dest` file in that directory) with `test_payload.zip`, whose MD5
(`046567309ae14799927fd228bbda2972`) is present in the default blacklist, so
by default the request is recorded but not stored in S3. To exercise the S3
upload path, run the server with an empty blacklist file:

```bash
touch /tmp/empty.txt
./server/scoutreg-server -port 8080 -blacklist /tmp/empty.txt
```

Health check: `curl http://127.0.0.1:8080/healthz`
