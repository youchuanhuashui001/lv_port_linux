#ifndef MP3_TAGS_H
#define MP3_TAGS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal ID3v2 text-frame reader for MP3 metadata.
 *
 * Extracts TIT2 (title) and TPE1 (artist) from the ID3v2.2/2.3/2.4
 * header at the start of `path` (native path, read through lv_fs so
 * the same code works on PC and on the target board).
 *
 * Text encodings 0 (Latin-1), 1 (UTF-16 with BOM), 2 (UTF-16BE) and
 * 3 (UTF-8) are converted to UTF-8.
 *
 * Either output pointer may be NULL. Buffers are always NUL
 * terminated. Returns 0 if a tag was parsed, -1 otherwise.
 */
int mp3_tags_read(const char *lv_fs_path,
                  char *title, size_t title_size,
                  char *artist, size_t artist_size);

/* File size in bytes via lv_fs; returns 0 when unknown. */
uint32_t mp3_tags_file_size(const char *lv_fs_path);

#ifdef __cplusplus
}
#endif

#endif /* MP3_TAGS_H */
