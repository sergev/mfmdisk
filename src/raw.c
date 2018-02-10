/*
 * Read/write a binary floppy image.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "config.h"
#include "mfm.h"

/*
 * Чтение образа дискеты из файла в традиционном бинарном виде.
 */
void mfm_read_raw(mfm_disk_t *d, FILE *fin, int nsectors_per_track)
{
    int t, s;
    struct stat st;

    if (fstat(fileno(fin), &st) < 0) {
        fprintf(mfm_err, "Cannot fstat() input file, aborted.\n");
        exit(-1);
    }
    d->ntracks = st.st_size / SECTSZ / nsectors_per_track;
    d->nsectors_per_track = nsectors_per_track;
    if (d->ntracks > MAXTRACK) {
        fprintf(mfm_err, "Too many tracks = %d, aborted.\n",
            d->ntracks);
        exit(-1);
    }
    fseek(fin, 0L, SEEK_SET);
    for (t=0; t<d->ntracks; ++t) {
        for (s=0; s<d->nsectors_per_track; ++s) {
            if (fread(d->block[t][s], SECTSZ, 1, fin) != 1) {
                fprintf(mfm_err, "Error reading input file, aborted.\n");
                exit(-1);
            }
        }
    }
}

/*
 * Запись образа дискеты в файл в традиционном бинарном виде.
 */
void mfm_write_raw(mfm_disk_t *d, FILE *fout)
{
    int t, s;

    for (t=0; t<d->ntracks; ++t) {
        for (s=0; s<d->nsectors_per_track; ++s) {
            fwrite(d->block[t][s], SECTSZ, 1, fout);
        }
    }
}
