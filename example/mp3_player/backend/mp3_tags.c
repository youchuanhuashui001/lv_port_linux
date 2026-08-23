#include "mp3_tags.h"

#include <string.h>

#include "lvgl/lvgl.h"

/* ------------------------------------------------------------------ */
/* UTF-16 / Latin-1 -> UTF-8 conversion helpers                        */
/* ------------------------------------------------------------------ */

static size_t utf8_put(char *dst, size_t cap, size_t pos, uint32_t cp)
{
	if(cp < 0x80) {
		if(pos + 1 >= cap) return pos;
		dst[pos++] = (char)cp;
	}
	else if(cp < 0x800) {
		if(pos + 2 >= cap) return pos;
		dst[pos++] = (char)(0xC0 | (cp >> 6));
		dst[pos++] = (char)(0x80 | (cp & 0x3F));
	}
	else if(cp < 0x10000) {
		if(pos + 3 >= cap) return pos;
		dst[pos++] = (char)(0xE0 | (cp >> 12));
		dst[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		dst[pos++] = (char)(0x80 | (cp & 0x3F));
	}
	else {
		if(pos + 4 >= cap) return pos;
		dst[pos++] = (char)(0xF0 | (cp >> 18));
		dst[pos++] = (char)(0x80 | ((cp >> 12) & 0x3F));
		dst[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
		dst[pos++] = (char)(0x80 | (cp & 0x3F));
	}
	return pos;
}

static void text_store(char *dst, size_t cap, const char *src)
{
	size_t n = strlen(src);
	if(n >= cap) n = cap - 1;
	memcpy(dst, src, n);
	while(n > 0 && (dst[n] & 0xC0) == 0x80) n--; /* cut multibyte tail */
	dst[n] = '\0';
}

static size_t latin1_to_utf8(const uint8_t *src, size_t len, char *dst, size_t cap)
{
	size_t i, pos = 0;
	for(i = 0; i < len; i++) pos = utf8_put(dst, cap, pos, src[i]);
	return pos;
}

static size_t utf16_to_utf8(const uint8_t *src, size_t len, int big_endian,
                            char *dst, size_t cap)
{
	size_t i = 0, pos = 0;
	uint32_t hi = 0;

	if(len >= 2 && src[0] == 0xFF && src[1] == 0xFE) {        /* BOM LE */
		big_endian = 0;
		i = 2;
	}
	else if(len >= 2 && src[0] == 0xFE && src[1] == 0xFF) {   /* BOM BE */
		big_endian = 1;
		i = 2;
	}

	for(; i + 1 < len; ) {
		uint32_t cp = big_endian ?
		              ((uint32_t)src[i] << 8) | src[i + 1] :
		              ((uint32_t)src[i + 1] << 8) | src[i];
		i += 2;

		if(hi != 0) {
			if(cp >= 0xDC00 && cp <= 0xDFFF) {
				cp = 0x10000 + ((hi - 0xD800) << 10) + (cp - 0xDC00);
				pos = utf8_put(dst, cap, pos, cp);
			}
			hi = 0;
			continue;
		}
		if(cp >= 0xD800 && cp <= 0xDBFF) {
			hi = cp;
			continue;
		}
		pos = utf8_put(dst, cap, pos, cp);
	}
	return pos;
}

/* ------------------------------------------------------------------ */
/* ID3v2 parsing                                                       */
/* ------------------------------------------------------------------ */

#define TAG_BUF_SIZE 1024

static uint32_t syncsafe32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 21) | ((uint32_t)p[1] << 14) |
	       ((uint32_t)p[2] << 7) | p[3];
}

static uint32_t plain32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | p[3];
}

static bool frame_wanted(const uint8_t *id, int ver,
                         char *title, char *artist,
                         bool have_title, bool have_artist)
{
	if(ver == 2) {
		if(!have_title && title && memcmp(id, "TT2", 3) == 0) return true;
		if(!have_artist && artist && memcmp(id, "TP1", 3) == 0) return true;
	}
	else {
		if(!have_title && title && memcmp(id, "TIT2", 4) == 0) return true;
		if(!have_artist && artist && memcmp(id, "TPE1", 4) == 0) return true;
	}
	return false;
}

int mp3_tags_read(const char *path, char *title, size_t title_size,
                  char *artist, size_t artist_size)
{
	lv_fs_file_t f;
	uint8_t hdr[10];
	uint8_t buf[TAG_BUF_SIZE];
	uint32_t tag_size, consumed;
	int ver;
	bool have_title = false, have_artist = false;

	if(title && title_size) title[0] = '\0';
	if(artist && artist_size) artist[0] = '\0';

	if(lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return -1;
	if(lv_fs_read(&f, hdr, sizeof(hdr), NULL) != LV_FS_RES_OK ||
	   memcmp(hdr, "ID3", 3) != 0 || hdr[3] < 2 || hdr[3] > 4) {
		lv_fs_close(&f);
		return -1;
	}

	ver = hdr[3];
	tag_size = syncsafe32(hdr + 6);   /* excludes the 10-byte header */

	const uint8_t id_len = (ver == 2) ? 3 : 4;
	const uint32_t hdr_len = (ver == 2) ? 6 : 10;

	for(consumed = 0; (!have_title || !have_artist) &&
	    consumed + hdr_len <= tag_size; ) {

		if(lv_fs_read(&f, buf, hdr_len, NULL) != LV_FS_RES_OK) break;

		if(buf[0] == 0x00) break;   /* padding reached */

		uint32_t frame_size =
		    (ver == 4) ? syncsafe32(buf + id_len) :
		    (ver == 3) ? plain32(buf + id_len) :
		    ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];

		consumed += hdr_len;

		if(frame_size == 0 || consumed + frame_size > tag_size) break;

		if(frame_wanted(buf, ver, title, artist, have_title, have_artist)) {
			uint8_t id[4];
			bool is_title;

			memcpy(id, buf, 4);

			/* Read the wanted frame's payload (encoding byte included). */
			if(frame_size > sizeof(buf)) break;
			if(lv_fs_read(&f, buf, frame_size, NULL) != LV_FS_RES_OK) break;
			consumed += frame_size;

			if(frame_size < 2) continue;    /* need enc byte + >=1 char */

			int enc = buf[0];
			uint32_t tlen = frame_size - 1;

			/* strip trailing NULs some writers append */
			while(tlen > 0 && buf[tlen] == 0) tlen--;
			if(tlen == 0) continue;

			char tmp[TAG_BUF_SIZE];
			size_t n = 0;
			switch(enc) {
				case 0:
					n = latin1_to_utf8(buf + 1, tlen, tmp, sizeof(tmp));
					break;
				case 3:
					n = tlen < sizeof(tmp) ? tlen : sizeof(tmp) - 1;
					memcpy(tmp, buf + 1, n);
					break;
				case 1:
				case 2:
					n = utf16_to_utf8(buf + 1, tlen, enc == 2, tmp, sizeof(tmp));
					break;
				default:
					continue;
			}
			tmp[n] = '\0';

			is_title = (ver == 2) ? memcmp(id, "TT2", 3) == 0
			           : memcmp(id, "TIT2", 4) == 0;
			if(is_title) {
				text_store(title, title_size, tmp);
				have_title = true;
			}
			else {
				text_store(artist, artist_size, tmp);
				have_artist = true;
			}
		}
		else {
			if(lv_fs_seek(&f, frame_size, LV_FS_SEEK_CUR) != LV_FS_RES_OK) break;
			consumed += frame_size;
		}
	}

	lv_fs_close(&f);
	return have_title || have_artist ? 0 : -1;
}

uint32_t mp3_tags_file_size(const char *path)
{
	lv_fs_file_t f;
	uint32_t size = 0;

	if(lv_fs_open(&f, path, LV_FS_MODE_RD) != LV_FS_RES_OK) return 0;
	if(lv_fs_seek(&f, 0, LV_FS_SEEK_END) == LV_FS_RES_OK)
		lv_fs_tell(&f, &size);
	lv_fs_close(&f);
	return size;
}
