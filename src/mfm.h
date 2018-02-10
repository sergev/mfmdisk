/*
 * Read/write MFM floppy image.
 *
 * Copyright (C) 2008-2018 Serge Vakulenko
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. The name of the author may not be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define MAXTRACK        160
#define MAXSECT         11
#define SECTSZ          512

#define INDEX_GAP       42      /* before first sector */
#define DATA_GAP        22      /* between sector mark and data */
#define SECTOR_GAP_9    80      /* 720k, 9 sectors per track */
#define SECTOR_GAP_10   46      /* 800k, 10 sectors per track */

typedef struct {
    int ntracks;                /* 80 или 160 */
    int nsectors_per_track;     /* 9..11 */
    unsigned char block [MAXTRACK] [MAXSECT] [SECTSZ];
} mfm_disk_t;

typedef struct {
    FILE *fd;
    int track;                  /* 0..159 */
    int halfbit;                /* 0..102400 */
    int byte;
} mfm_reader_t;

typedef struct {
    FILE *fd;
    int last;
    int halfbit;                /* 0..102400 */
    int byte;
} mfm_writer_t;

FILE *mfm_err;
int mfm_verbose;
int mfm_gap_byte;
int mfm_index_gap;
int mfm_sector_gap;
int mfm_data_gap;

void mfm_read_seek(mfm_reader_t *reader, FILE *fin, int t);
int mfm_read_halfbit(mfm_reader_t *reader);
int mfm_read_bit(mfm_reader_t *reader);
int mfm_read_byte(mfm_reader_t *reader);
void mfm_dump(FILE *fin, int ntracks);

void mfm_write_reset(mfm_writer_t *writer, FILE *fout);
void mfm_write_halfbit(mfm_writer_t *writer, int val);
void mfm_write_bit(mfm_writer_t *writer, int val);
void mfm_write(mfm_writer_t *writer, unsigned char *data, int bytes);
void mfm_write_byte(mfm_writer_t *writer, int val);
void mfm_write_gap(mfm_writer_t *writer, int nbytes, int val);
void mfm_fill_track(mfm_writer_t *writer, int val);

void mfm_analyze_ibmpc(FILE *fin, int ntracks);
void mfm_read_ibmpc(mfm_disk_t *d, FILE *fin, int ntracks);
void mfm_write_ibmpc(mfm_disk_t *d, FILE *fout, int skip_index_mark);

int mfm_detect_amiga(FILE *fin);
void mfm_analyze_amiga(FILE *fin, int ntracks);
void mfm_read_amiga(mfm_disk_t *d, FILE *fin, int ntracks);
void mfm_write_amiga(mfm_disk_t *d, FILE *fout);

void mfm_read_raw(mfm_disk_t *d, FILE *fin, int nsectors_per_track);
void mfm_write_raw(mfm_disk_t *d, FILE *fout);
