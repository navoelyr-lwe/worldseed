/* Worldseed Engine — evidence and package manifest tool.
 *
 * The release package must ship a manifest that names every artefact with its
 * size and SHA-256, so a download can be verified without trusting the transfer
 * (docs/12 "Paket manifesti, hash/checksum ve sürüm notu üretilir"; docs/18).
 *
 * Usage: ws_manifest --out <manifest.txt> --version <tag> <file> [<file> ...]
 *
 * Each existing file becomes a "<sha256>  <bytes>  <basename>" line; missing
 * files are reported on stderr and make the tool exit non-zero, so a package
 * that silently dropped an artefact fails the gate.
 */
#include "core/ws_hash.h"
#include "core/ws_result.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *basename_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *b = slash != NULL ? slash + 1 : path;
    if (back != NULL && back + 1 > b) {
        b = back + 1;
    }
    return b;
}

static int hash_file(const char *path, char *hex_out, unsigned long long *size_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return 0;
    }
    ws_sha256 ctx;
    ws_sha256_init(&ctx);
    unsigned char buf[65536];
    size_t n;
    unsigned long long total = 0u;
    while ((n = fread(buf, 1u, sizeof(buf), f)) > 0u) {
        ws_sha256_update(&ctx, buf, n);
        total += (unsigned long long)n;
    }
    int ok = !ferror(f);
    fclose(f);
    if (!ok) {
        return 0;
    }
    unsigned char digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_final(&ctx, digest);
    ws_sha256_hex(digest, hex_out);
    *size_out = total;
    return 1;
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *version = "0.0.0";
    const char **files = NULL;
    int file_count = 0;

    files = (const char **)calloc((size_t)argc, sizeof(char *));
    if (files == NULL) {
        fprintf(stderr, "manifest: out of memory\n");
        return 2;
    }
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc) {
            version = argv[++i];
        } else {
            files[file_count++] = argv[i];
        }
    }
    if (out_path == NULL || file_count == 0) {
        fprintf(stderr,
                "usage: %s --out <manifest.txt> [--version <tag>] <file> [<file> ...]\n",
                argv[0]);
        free(files);
        return 2;
    }

    FILE *out = fopen(out_path, "w");
    if (out == NULL) {
        fprintf(stderr, "manifest: cannot write %s\n", out_path);
        free(files);
        return 2;
    }
    fprintf(out, "# Worldseed Engine evidence manifest\n");
    fprintf(out, "version=%s\n", version);
    fprintf(out, "files=%d\n", file_count);

    int missing = 0;
    for (int i = 0; i < file_count; ++i) {
        char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
        unsigned long long size = 0u;
        if (!hash_file(files[i], hex, &size)) {
            fprintf(stderr, "manifest: missing or unreadable %s\n", files[i]);
            ++missing;
            continue;
        }
        fprintf(out, "%s  %llu  %s\n", hex, size, basename_of(files[i]));
        printf("manifest %s sha256=%s bytes=%llu\n", basename_of(files[i]), hex, size);
    }
    fclose(out);
    free(files);
    if (missing > 0) {
        fprintf(stderr, "manifest: %d file(s) missing\n", missing);
        return 1;
    }
    printf("WS_MANIFEST_OK files=%d path=%s\n", file_count, out_path);
    return 0;
}
