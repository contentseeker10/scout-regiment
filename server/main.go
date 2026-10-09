package main

import (
	"bytes"
	"context"
	"crypto/md5"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"os"
	"strings"
	"time"

	"github.com/aws/aws-sdk-go-v2/aws"
	awsconfig "github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/credentials"
	"github.com/aws/aws-sdk-go-v2/service/s3"
	"github.com/aws/smithy-go"
	"github.com/joho/godotenv"
	_ "modernc.org/sqlite"
)

const createTableSQL = `
CREATE TABLE IF NOT EXISTS reports (
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
);`

type serverConfig struct {
	s3Endpoint  string
	s3Region    string
	s3Bucket    string
	s3AccessKey string
	s3SecretKey string
}

type report struct {
	hostname     string
	username     string
	cpuArch      string
	cpuCores     string
	totalRAM     string
	availableRAM string
	clientIP     string
	fileName     string
	fileMD5      string
	s3URL        *string
	blacklisted  bool
	createdAt    string
}

type handler struct {
	db        *sql.DB
	cfg       serverConfig
	s3        *s3.Client
	blacklist map[string]bool
}

func main() {
	port := flag.Int("port", 8080, "HTTP port to listen on (default 8080)")
	dbPath := flag.String("db", "scoutreg.db", "path to the SQLite database file")
	blacklistPath := flag.String("blacklist", "md5_blacklist.txt", "path to the file with blacklisted MD5 hashes, one per line")
	flag.Parse()

	// The .env with S3 credentials lives in the repository root; also allow
	// running the server directly from that root.
	if err := godotenv.Load(); err != nil {
		if err := godotenv.Load("../.env"); err != nil {
			log.Printf("warning: no .env found (%v); using process environment only", err)
		}
	}

	cfg := serverConfig{
		s3Endpoint:  strings.TrimRight(envOr("S3_ENDPOINT", "http://localhost:9000"), "/"),
		s3Region:    envOr("S3_REGION", "garage"),
		s3Bucket:    envOr("S3_BUCKET", "scout-reg-bucket"),
		s3AccessKey: os.Getenv("S3_ACCESS_KEY"),
		s3SecretKey: os.Getenv("S3_SECRET_KEY"),
	}

	db, err := sql.Open("sqlite", *dbPath)
	if err != nil {
		log.Fatalf("open database: %v", err)
	}
	defer db.Close()

	if _, err := db.Exec(createTableSQL); err != nil {
		log.Fatalf("create table: %v", err)
	}

	blacklist, err := loadBlacklist(*blacklistPath)
	if err != nil {
		log.Fatalf("load blacklist: %v", err)
	}
	log.Printf("loaded %d blacklisted md5 hashes from %s", len(blacklist), *blacklistPath)

	s3Client, err := newS3Client(cfg)
	if err != nil {
		log.Fatalf("s3 client: %v", err)
	}

	h := &handler{db: db, cfg: cfg, s3: s3Client, blacklist: blacklist}

	mux := http.NewServeMux()
	mux.HandleFunc("/client-info", h.handleClientInfo)
	mux.HandleFunc("/healthz", func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
	})

	addr := fmt.Sprintf(":%d", *port)
	log.Printf("scout-regiment server listening on %s (s3 bucket %q)", addr, cfg.s3Bucket)
	log.Fatal(http.ListenAndServe(addr, mux))
}

func (h *handler) handleClientInfo(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, map[string]string{"error": "only POST is allowed"})
		return
	}

	if err := r.ParseMultipartForm(32 << 20); err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "invalid multipart form: " + err.Error()})
		return
	}

	file, header, err := r.FormFile("file")
	if err != nil {
		writeJSON(w, http.StatusBadRequest, map[string]string{"error": "missing \"file\" part: " + err.Error()})
		return
	}
	defer file.Close()

	data, err := io.ReadAll(file)
	if err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "cannot read uploaded file: " + err.Error()})
		return
	}

	sum := md5.Sum(data)
	md5hex := hex.EncodeToString(sum[:])

	rep := report{
		hostname:     r.FormValue("hostname"),
		username:     r.FormValue("username"),
		cpuArch:      r.FormValue("cpu_arch"),
		cpuCores:     r.FormValue("cpu_cores"),
		totalRAM:     r.FormValue("total_ram"),
		availableRAM: r.FormValue("available_ram"),
		clientIP:     clientIP(r),
		fileName:     header.Filename,
		fileMD5:      md5hex,
		createdAt:    time.Now().UTC().Format(time.RFC3339),
	}

	if h.blacklist[md5hex] {
		rep.blacklisted = true
		if err := h.save(rep); err != nil {
			writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "db insert: " + err.Error()})
			return
		}
		log.Printf("REJECTED blacklisted upload from %s (md5 %s, file %q)", rep.clientIP, md5hex, rep.fileName)
		writeJSON(w, http.StatusOK, map[string]any{
			"status":     "blacklisted",
			"md5":        md5hex,
			"stored":     false,
			"blacklisted": true,
		})
		return
	}

	key := fmt.Sprintf("scoutreg/%d_%s.zip", time.Now().UnixNano(), md5hex)
	url, uploadErr := h.upload(r.Context(), key, data)
	if uploadErr != nil {
		log.Printf("s3 upload failed for %s: %v (recording row without s3_url)", key, uploadErr)
	} else {
		rep.s3URL = &url
	}

	if err := h.save(rep); err != nil {
		writeJSON(w, http.StatusInternalServerError, map[string]string{"error": "db insert: " + err.Error()})
		return
	}

	status := "stored"
	if uploadErr != nil {
		status = "recorded_without_s3"
	}

	log.Printf("accepted upload from %s (md5 %s, s3_url %v)", rep.clientIP, md5hex, urlOrNone(rep.s3URL))
	writeJSON(w, http.StatusOK, map[string]any{
		"status":     status,
		"md5":        md5hex,
		"stored":     uploadErr == nil,
		"blacklisted": false,
		"s3_url":     urlOrNone(rep.s3URL),
	})
}

func (h *handler) upload(ctx context.Context, key string, data []byte) (string, error) {
	_, err := h.s3.PutObject(ctx, &s3.PutObjectInput{
		Bucket:      aws.String(h.cfg.s3Bucket),
		Key:         aws.String(key),
		Body:        bytes.NewReader(data),
		ContentType: aws.String("application/zip"),
	})
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%s/%s/%s", h.cfg.s3Endpoint, h.cfg.s3Bucket, key), nil
}

func (h *handler) save(rep report) error {
	blacklisted := 0
	if rep.blacklisted {
		blacklisted = 1
	}
	_, err := h.db.Exec(
		`INSERT INTO reports
			(hostname, username, cpu_arch, cpu_cores, total_ram, available_ram, client_ip, file_name, file_md5, s3_url, blacklisted, created_at)
		 VALUES (?,?,?,?,?,?,?,?,?,?,?,?)`,
		rep.hostname, rep.username, rep.cpuArch, rep.cpuCores, rep.totalRAM, rep.availableRAM,
		rep.clientIP, rep.fileName, rep.fileMD5, rep.s3URL, blacklisted, rep.createdAt,
	)
	return err
}

func newS3Client(cfg serverConfig) (*s3.Client, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()

	awsCfg, err := awsconfig.LoadDefaultConfig(ctx,
		awsconfig.WithRegion(cfg.s3Region),
		awsconfig.WithCredentialsProvider(
			credentials.NewStaticCredentialsProvider(cfg.s3AccessKey, cfg.s3SecretKey, ""),
		),
		awsconfig.WithBaseEndpoint(cfg.s3Endpoint),
	)
	if err != nil {
		return nil, err
	}

	client := s3.NewFromConfig(awsCfg, func(o *s3.Options) {
		o.UsePathStyle = true
	})

	// Make sure the bucket exists; create it when missing.
	if _, err := client.HeadBucket(ctx, &s3.HeadBucketInput{Bucket: aws.String(cfg.s3Bucket)}); err != nil {
		_, createErr := client.CreateBucket(ctx, &s3.CreateBucketInput{Bucket: aws.String(cfg.s3Bucket)})
		if createErr != nil {
			var apiErr smithy.APIError
			if !errors.As(createErr, &apiErr) ||
				(apiErr.ErrorCode() != "BucketAlreadyOwnedByYou" && apiErr.ErrorCode() != "BucketAlreadyExists") {
				return nil, fmt.Errorf("ensure bucket %q: %w", cfg.s3Bucket, createErr)
			}
		}
		log.Printf("created s3 bucket %q", cfg.s3Bucket)
	}

	return client, nil
}

func loadBlacklist(path string) (map[string]bool, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	m := make(map[string]bool)
	for _, line := range strings.Split(string(data), "\n") {
		line = strings.ToLower(strings.TrimSpace(line))
		if line == "" || strings.HasPrefix(line, "#") {
			continue
		}
		m[line] = true
	}
	return m, nil
}

func clientIP(r *http.Request) string {
	if xff := r.Header.Get("X-Forwarded-For"); xff != "" {
		return strings.TrimSpace(strings.Split(xff, ",")[0])
	}
	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil {
		return r.RemoteAddr
	}
	return host
}

func writeJSON(w http.ResponseWriter, status int, body any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(body)
}

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func urlOrNone(p *string) any {
	if p == nil {
		return nil
	}
	return *p
}
