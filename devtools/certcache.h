// certcache.h: persistence of rebuilt tables for certsolve
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ---- persistence of rebuilt tables (default on): a table is saved under the cache directory, named by
// a fingerprint of the certificate data it was rebuilt from, and read back on later runs
inline std::string gCache;
static uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ULL) {
  const uint8_t *b = (const uint8_t *)p;
  for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 1099511628211ULL;
  return h;
}
static std::string cachePath(const std::string &kind, uint64_t fp) {
  if (gCache.empty()) return "";
  char b[64];
  snprintf(b, sizeof b, "-%016llx.tab", (unsigned long long)fp);
  std::string k = kind;
  for (auto &c : k) if (c == '/' || c == '.') c = '_';
  return gCache + "/" + k + b;
}
// blocks: (pointer, bytes) in order
static bool readCache(const std::string &path, const std::vector<std::pair<void *, size_t>> &blocks) {
  if (path.empty()) return false;
  FILE *f = fopen(path.c_str(), "rb");
  if (!f) return false;
  char magic[8];
  bool ok = fread(magic, 1, 8, f) == 8 && !memcmp(magic, "CSTAB01\n", 8);
  for (auto &[p, n] : blocks) ok = ok && fread(p, 1, n, f) == n;
  ok = ok && fgetc(f) == EOF;
  fclose(f);
  return ok;
}
static void writeCache(const std::string &path, const std::vector<std::pair<const void *, size_t>> &blocks) {
  if (path.empty()) return;
  if (system(("mkdir -p '" + gCache + "'").c_str())) return;
  const std::string tmp = path + ".tmp";
  FILE *f = fopen(tmp.c_str(), "wb");
  if (!f) return;
  bool ok = fwrite("CSTAB01\n", 1, 8, f) == 8;
  for (auto &[p, n] : blocks) ok = ok && fwrite(p, 1, n, f) == n;
  ok = fclose(f) == 0 && ok;
  if (ok) rename(tmp.c_str(), path.c_str());
  else remove(tmp.c_str());
}

