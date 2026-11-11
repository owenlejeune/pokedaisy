/* GPL-3.0. Keep upstream pk_savevf.c unchanged; enforce Pocket's error policy
 * around its underlying file. Shared by GB/GBC/GBA, independent of companion.
 */
#include "host_io.h"
#include "../app/src/main/cpp/pk_savevf.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
struct checked_save {
  struct VFile d;
  struct VFile *inner;
  struct pocket_save_status *status;
};
static bool close_save(struct VFile *vf) {
  struct checked_save *s = (void *)vf;
  bool ok = s->inner->close(s->inner);
  s->status->failed |= !ok;
  s->status->closed = true;
  s->status->vf = NULL;
  s->status->mapped = NULL;
  s->status->mappedSize = 0;
  s->status->fd = -1;
  free(s);
  return ok;
}
static off_t seek_save(struct VFile *vf, off_t at, int whence) {
  struct checked_save *s = (void *)vf;
  off_t n = s->inner->seek(s->inner, at, whence);
  if (n < 0)
    s->status->failed = true;
  return n;
}
static ssize_t read_save(struct VFile *vf, void *out, size_t size) {
  struct checked_save *s = (void *)vf;
  if (s->status->failed)
    return -1;
  off_t pos = s->inner->seek(s->inner, 0, SEEK_CUR);
  ssize_t total = s->inner->size(s->inner);
  ssize_t n = s->inner->read(s->inner, out, size);
  if (pos < 0 || total < 0 || n < 0 ||
      (pos < total &&
       (size_t)n <
           ((size_t)(total - pos) < size ? (size_t)(total - pos) : size)))
    s->status->failed = true;
  return n;
}
static ssize_t line_save(struct VFile *vf, char *out, size_t size) {
  struct checked_save *s = (void *)vf;
  ssize_t n = s->inner->readline(s->inner, out, size);
  if (n < 0)
    s->status->failed = true;
  return n;
}
static ssize_t write_save(struct VFile *vf, const void *data, size_t size) {
  struct checked_save *s = (void *)vf;
  if (s->status->failed)
    return -1;
  ssize_t n = s->inner->write(s->inner, data, size);
  if (n < 0 || (size_t)n != size)
    s->status->failed = true;
  return n;
}
static void *map_save(struct VFile *vf, size_t size, int flags) {
  struct checked_save *s = (void *)vf;
  void *p = s->inner->map(s->inner, size, flags);
  if (p && (flags & MAP_WRITE)) {
    s->status->mapped = p;
    s->status->mappedSize = size;
  }
  return p;
}
static void unmap_save(struct VFile *vf, void *p, size_t size) {
  struct checked_save *s = (void *)vf;
  if (p && p == s->status->mapped) {
    s->status->mapped = NULL;
    s->status->mappedSize = 0;
  }
  s->inner->unmap(s->inner, p, size);
}
static void truncate_save(struct VFile *vf, size_t size) {
  struct checked_save *s = (void *)vf;
  if (s->status->failed)
    return;
  s->inner->truncate(s->inner, size);
  ssize_t actual = s->inner->size(s->inner);
  if (actual < 0 || (size_t)actual != size)
    s->status->failed = true;
}
static ssize_t size_save(struct VFile *vf) {
  struct checked_save *s = (void *)vf;
  ssize_t size = s->inner->size(s->inner);
  if (size < 0)
    s->status->failed = true;
  return size;
}
static bool sync_save(struct VFile *vf, void *p, size_t size) {
  struct checked_save *s = (void *)vf;
  if (s->status->failed)
    return false;
  bool ok = s->inner->sync(s->inner, p, size);
  if (!ok)
    s->status->failed = true;
  return ok;
}
struct VFile *pocket_save_wrap(struct VFile *inner,
                               struct pocket_save_status *status) {
  if (!status) {
    if (inner)
      inner->close(inner);
    return NULL;
  }
  memset(status, 0, sizeof *status);
  status->fd = -1;
  if (!inner) {
    status->failed = true;
    status->closed = true;
    return NULL;
  }
  struct checked_save *s = calloc(1, sizeof *s);
  if (!s) {
    inner->close(inner);
    status->closed = true;
    status->failed = true;
    return NULL;
  }
  s->inner = inner;
  s->status = status;
  s->d = (struct VFile){.close = close_save,
                        .seek = seek_save,
                        .read = read_save,
                        .readline = line_save,
                        .write = write_save,
                        .map = map_save,
                        .unmap = unmap_save,
                        .truncate = truncate_save,
                        .size = size_save,
                        .sync = sync_save};
  struct VFile *result = pk_save_vf(&s->d);
  if (result == &s->d) {
    close_save(result);
    status->failed = true;
    return NULL;
  }
  status->vf = result;
  return result;
}

struct VFile *pocket_save_open(const char *path, int flags,
                               struct pocket_save_status *status) {
  if (!path)
    return pocket_save_wrap(NULL, status);
  int fd = open(path, flags, 0666);
  if (fd < 0)
    return pocket_save_wrap(NULL, status);
  struct VFile *inner = VFileFromFD(fd);
  if (!inner)
    return pocket_save_wrap(NULL, status);
  struct VFile *result = pocket_save_wrap(inner, status);
  if (!result)
    return NULL;
  status->fd = fd;
  status->writable = (flags & O_ACCMODE) != O_RDONLY;
  return result;
}

bool pocket_save_flush(struct pocket_save_status *status) {
  if (!status)
    return false;
  if (status->closed || !status->vf)
    return status->closed && !status->failed;
  if (status->failed)
    return false;
  bool ok = true;
  if (status->mapped && status->mappedSize > 0) {
    if (msync(status->mapped, status->mappedSize, MS_SYNC) != 0)
      ok = false;
    if (ok && status->fd >= 0 && status->writable) {
      /* Emit a VFS write event (IN_MODIFY) without changing bytes or fd offset. */
      struct stat st;
      if (fstat(status->fd, &st) == 0 && st.st_size > 0) {
        unsigned char first = *(const unsigned char *)status->mapped;
        if (pwrite(status->fd, &first, 1, 0) != 1)
          ok = false;
      }
    }
  }
  if (!status->vf->sync(status->vf, NULL, 0))
    ok = false;
  if (status->fd >= 0 && fsync(status->fd) != 0)
    ok = false;
  if (!ok)
    status->failed = true;
  return ok;
}

static unsigned char *read_all(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  unsigned char *data = NULL;
  long n = fseek(f, 0, SEEK_END) ? -1 : ftell(f);
  if (n >= 0 && n <= 64 * 1024 * 1024 && !fseek(f, 0, SEEK_SET) &&
      (data = malloc(n ? (size_t)n : 1)) && fread(data, 1, (size_t)n, f) != (size_t)n) {
    free(data);
    data = NULL;
  }
  fclose(f);
  *size = data ? (size_t)n : 0;
  return data;
}
/* Same save data: every save chip's size is a multiple of 512, so anything past
 * the last whole 512 bytes is mGBA's RTC footer, which moves with the clock. */
static bool same_save(const unsigned char *a, size_t an, const unsigned char *b,
                      size_t bn) {
  size_t n = an - an % 512;
  if (!n || n != bn - bn % 512)
    return an == bn && !memcmp(a, b, an);
  return !memcmp(a, b, n);
}
static int newest_first(const void *a, const void *b) {
  return -strcmp(*(char *const *)a, *(char *const *)b);
}
const char *pocket_backup_dir;
bool pocket_save_backup(const char *savePath, time_t now) {
  size_t size;
  unsigned char *save = read_all(savePath, &size);
  if (!save)
    return errno == ENOENT;
  if (!size) {
    free(save);
    return true;
  }
  const char *slash = strrchr(savePath, '/');
  const char *file = slash ? slash + 1 : savePath;
  const char *dot = strrchr(file, '.');
  char dir[1100], stem[256], ext[32] = "sav";
  if (pocket_backup_dir)
    snprintf(dir, sizeof dir, "%s", pocket_backup_dir);
  else
    snprintf(dir, sizeof dir, "%.*s%s", slash ? (int)(slash - savePath + 1) : 0,
             savePath, POCKET_BACKUP_DIR);
  snprintf(stem, sizeof stem, "%.*s.backup-", dot && dot != file ? (int)(dot - file) : (int)strlen(file), file);
  if (dot && dot != file)
    snprintf(ext, sizeof ext, "%s", dot + 1);
  mkdir(dir, 0755);
  char *names[256];
  int count = 0;
  DIR *d = opendir(dir);
  for (struct dirent *e; d && (e = readdir(d)) && count < 256;)
    if (!strncmp(e->d_name, stem, strlen(stem)) && !strstr(e->d_name, ".tmp"))
      names[count++] = strdup(e->d_name);
  if (d)
    closedir(d);
  qsort(names, count, sizeof *names, newest_first);
  bool ok = true, write = true;
  char path[1400], tmp[1500];
  if (count) {
    snprintf(path, sizeof path, "%s/%s", dir, names[0]);
    size_t lastSize;
    unsigned char *last = read_all(path, &lastSize);
    write = !last || !same_save(last, lastSize, save, size);
    free(last);
  }
  if (write) {
    struct tm t;
    char stamp[32];
    localtime_r(&now, &t);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &t);
    snprintf(path, sizeof path, "%s/%s%s.%s", dir, stem, stamp, ext);
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *out = fopen(tmp, "wb");
    ok = out && fwrite(save, 1, size, out) == size;
    if (out && fclose(out))
      ok = false;
    ok = ok && rename(tmp, path) == 0;
    if (!ok)
      unlink(tmp);
  }
  /* The new copy is the newest; drop what's past the newest KEEP. */
  for (int i = write && ok ? POCKET_BACKUP_KEEP - 1 : POCKET_BACKUP_KEEP; i < count; i++) {
    if (i >= 0) {
      snprintf(path, sizeof path, "%s/%s", dir, names[i]);
      unlink(path);
    }
  }
  for (int i = 0; i < count; i++)
    free(names[i]);
  free(save);
  return ok;
}
