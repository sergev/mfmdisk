/*
 * Decode Amiga floppy format.
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
#include <string.h>
#include "config.h"
#include "mfm.h"

/*
 * Первый аргумент содержит нечётные биты 32-битного слова,
 * второй - чётные биты. Возвращаем значение исходного слова.
 */
static unsigned long shuffle(int odd, int even)
{
    unsigned long word;
    int i;

    word = 0;
    for (i=0; i<16; ++i) {
        word <<= 2;
        word |= (even >> 15 & 1) | (odd >> 14 & 2);
        odd <<= 1;
        even <<= 1;
    }
    return word;
}

/*
 * Поиск идентификатора сектора на дискете формата Amiga.
 */
int mfm_scan_amiga(mfm_reader_t *reader, int *nbits_read)
{
    int bit, tag;
    unsigned long history;

    history = 0;
    if (nbits_read)
        *nbits_read = 0;
    for (;;) {
        bit = mfm_read_bit(reader);
        if (bit < 0) {
            if (mfm_verbose && nbits_read)
                fprintf(mfm_err, "Track %d/%d: final gap %d bits\n",
                    reader->track >> 1, reader->track & 1, *nbits_read);
            return -1;
        }
        history = history << 1 | bit;
        if (nbits_read)
            ++*nbits_read;

        if ((history & 0xffffffff) == 0xffffffff) {
            /* Все единицы - подсинхронизовываемся на полубит. */
            mfm_read_halfbit(reader);
            history = 0;
            continue;
        }

        /* Формат Amiga: ждем 00-a1-a1-fx. */
        if ((history & 0xfffffff0) == 0x00a1a1f0) {
            /* Нашли маркер, читаем и возвращаем его тег. */
            tag = history & 0xff;
            return tag;
        }
    }
}

/*
 * Определяем тип дискеты.
 * Возвращаем 0 для IBM PC или 1 для Amiga.
 */
int mfm_detect_amiga(FILE *fin)
{
    mfm_reader_t reader;
    unsigned long history;
    int bit;

    mfm_read_seek(&reader, fin, 0);
    history = 0x13713713;
    for (;;) {
        bit = mfm_read_bit(&reader);
        if (bit < 0)
            return -1;
        history = history << 1 | bit;
        history &= 0xffffffff;

        /* Все единицы - подсинхронизовываемся на полубит. */
        if (history == 0xffffffff) {
            mfm_read_halfbit(&reader);
            history = 0;
            continue;
        }

        /* Формат IBM PC: ждем 00-a1-a1-a1 или 00-c2-c2-c2. */
        if (history == 0x00a1a1a1 || history == 0x00c2c2c2) {
            return 0;
        }

        /* Формат Amiga: ждем 00-a1-a1-fx. */
        if ((history & ~0xf) == 0x00a1a1f0) {
            return 1;
        }
    }
}

/*
 * Чтение 32-битного слова с перестановкой битов.
 * Подсчитываем контрольную сумму.
 */
static unsigned long read_long(mfm_reader_t *reader, unsigned long *sum)
{
    int odd, even;

    odd = mfm_read_byte(reader) << 8;
    odd |= mfm_read_byte(reader);
    even = mfm_read_byte(reader) << 8;
    even |= mfm_read_byte(reader);
    *sum ^= odd ^ even;
    return shuffle(odd, even);
}

/*
 * Чтение 512-байтного блока с перестановкой битов.
 * Возвращаем контрольную сумму.
 */
static int read_data(mfm_reader_t *reader, unsigned char *data)
{
    unsigned long ldata;
    int odd [128], even [128], sum, i;

    /* Первая половина - нечётные биты. */
    for (i=0; i<SECTSZ/4; ++i) {
        odd[i] = mfm_read_byte(reader) << 8;
        odd[i] |= mfm_read_byte(reader);
    }
    /* Вторая половина - чётные биты. */
    for (i=0; i<SECTSZ/4; ++i) {
        even[i] = mfm_read_byte(reader) << 8;
        even[i] |= mfm_read_byte(reader);
    }
    /* Восстанавливаем данные. */
    sum = 0;
    for (i=0; i<SECTSZ/4; ++i) {
        ldata = shuffle(odd[i], even[i]);
        sum ^= odd[i] ^ even[i];
        *data++ = ldata >> 24;
        *data++ = ldata >> 16;
        *data++ = ldata >> 8;
        *data++ = ldata;
    }
    return sum;
}

/*
 * Чтение очередного сектора с дискеты формата Amiga.
 */
int mfm_read_sector_amiga(mfm_reader_t *reader, unsigned char *data,
    int *sector_gap)
{
    int tag, track, sector, odd, even, gap;
    unsigned long label[4], header_sum, data_sum;
    unsigned long my_header_sum, my_data_sum;

    if (sector_gap)
        *sector_gap = 0;
    for (;;) {
        tag = mfm_scan_amiga(reader, &gap);
        if (sector_gap)
            *sector_gap += gap;
        if (tag < 0)
            return -1;
        odd = (tag << 8) | mfm_read_byte(reader);
        even = mfm_read_byte(reader) << 8;
        even |= mfm_read_byte(reader);
        tag = shuffle(odd, even) & 0xffffff;
        track = tag >> 16;
        sector = tag >> 8 & 0xff;
        my_header_sum = odd ^ even;

        /* Метка сектора. */
        label[0] = read_long(reader, &my_header_sum);
        label[1] = read_long(reader, &my_header_sum);
        label[2] = read_long(reader, &my_header_sum);
        label[3] = read_long(reader, &my_header_sum);
        if (mfm_verbose)
            fprintf(mfm_err, "Track %d, sector %d: label %08lx:%08lx:%08lx:%08lx\n",
                track, sector, label[0], label[1], label[2], label[3]);

        header_sum = mfm_read_byte(reader) << 24;
        header_sum |= mfm_read_byte(reader) << 16;
        header_sum |= mfm_read_byte(reader) << 8;
        header_sum |= mfm_read_byte(reader);
        if (my_header_sum != header_sum) {
            fprintf(mfm_err, "track %d sector %d: header sum %08lx, expected %08lx\n",
                track, sector, my_header_sum, header_sum);
            return -1;
        }
        if (track != reader->track) {
            fprintf(mfm_err, "track %d, sector %d: incorrect track number, expected %d\n",
                track, sector, reader->track);
        }

        data_sum = mfm_read_byte(reader) << 24;
        data_sum |= mfm_read_byte(reader) << 16;
        data_sum |= mfm_read_byte(reader) << 8;
        data_sum |= mfm_read_byte(reader);

        my_data_sum = read_data(reader, data);
        if (my_data_sum != data_sum)
            fprintf(mfm_err, "track %d sector %d: data sum %08lx, expected %08lx\n",
                track, sector, my_data_sum, data_sum);
        return sector;
    }
}

/*
 * Читаем дискету Amiga из MFM-файла. Количество дорожек (до 160)
 * задаётся параметром ntracks.
 */
void mfm_read_amiga(mfm_disk_t *d, FILE *fin, int ntracks)
{
    int t, s;
    mfm_reader_t reader;
    unsigned char block [SECTSZ];
    int have_sector [MAXSECT];

    d->ntracks = ntracks;
    d->nsectors_per_track = 11;
    for (t=0; t<d->ntracks; ++t) {
        mfm_read_seek(&reader, fin, t);
        for (s=0; s<MAXSECT; ++s)
            have_sector [s] = 0;
        for (;;) {
            s = mfm_read_sector_amiga(&reader, block, 0);
            if (s < 0)
                break;
            if (s >= d->nsectors_per_track) {
                fprintf(mfm_err, "track %d: too large sector number %d\n",
                    t, s);
                continue;
            }
            /* Сектора могут следовать в произвольном порядке. */
            have_sector [s] = 1;
            memcpy(d->block[t][s], block, SECTSZ);
        }

        /* Проверим, что получили все сектора. */
        for (s=0; s<d->nsectors_per_track; ++s) {
            if (! have_sector [s])
                fprintf(mfm_err, "track %d: no sector %d\n", t, s);
        }
    }
}

/*
 * Исследуем и печатаем информацию о дискете Amiga из MFM-файла.
 * Количество дорожек (до 160) задаётся параметром ntracks.
 */
void mfm_analyze_amiga(FILE *fin, int ntracks)
{
    int t, s, i, nsectors_per_track;
    mfm_reader_t reader;
    unsigned char block [SECTSZ];
    int have_sector [MAXSECT];
    int order_of_sectors [MAXSECT];
    int sector_gap [MAXSECT];

    fprintf(mfm_err, "Format: Amiga\n");
    for (t=0; t<ntracks; ++t) {
        fprintf(mfm_err, "\n");
        mfm_read_seek(&reader, fin, t);
        for (s=0; s<MAXSECT; ++s)
            have_sector [s] = 0;
        nsectors_per_track = 0;
        for (i=0; ; ++i) {
            s = mfm_read_sector_amiga(&reader, block, &sector_gap[i]);
            if (s < 0)
                break;
            if (s >= MAXSECT) {
                fprintf(mfm_err, "Too many sectors per track = %d, aborted.\n",
                    s+1);
                exit(-1);
            }
            if (s >= nsectors_per_track)
                nsectors_per_track = s + 1;

            /* Сектора могут следовать в произвольном порядке. */
            have_sector [s] = 1;
            order_of_sectors [i] = s;
        }
        fprintf(mfm_err, "Track %d/%d: %d sectors per track\n",
            t >> 1, t & 1, nsectors_per_track);
        if (nsectors_per_track < 1)
            continue;

        fprintf(mfm_err, "Order of sectors:");
        for (s=0; s<i; ++s) {
            fprintf(mfm_err, " %d", order_of_sectors[s] + 1);
        }
        fprintf(mfm_err, "\n");

        fprintf(mfm_err, "Sector gap:");
        for (s=0; s<i; ++s) {
            fprintf(mfm_err, " %d", sector_gap[s] - 5*8);
        }
        fprintf(mfm_err, " bits (std %d)\n", 0);

        /* Проверим, что получили все сектора. */
        for (s=0; s<nsectors_per_track; ++s) {
            if (! have_sector [s])
                fprintf(mfm_err, "No sector %d\n", s + 1);
        }
    }
}

/*
 * Записываем MFM-образ флоппи-диска в формате Amiga.
 */
void mfm_write_amiga(mfm_disk_t *d, FILE *fout)
{
    fprintf(mfm_err, "Writing Amiga floppies not implemented yet, sorry.\n");
    /* TODO */
}
