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
#include <stdio.h>
#include "config.h"
#include "mfm.h"

int mfm_gap_byte = 0x4e;

/*
 * Чтение полубита.
 */
int mfm_read_halfbit(mfm_reader_t *reader)
{
    /* Дорожка закончилась. */
    if (reader->halfbit >= 102400)
        return -1;

    if ((reader->halfbit & 7) == 0)
        reader->byte = getc(reader->fd);

    ++reader->halfbit;
    reader->byte <<= 1;
    return reader->byte >> 8 & 1;
}

/*
 * Декодирование очередного бита.
 */
int mfm_read_bit(mfm_reader_t *reader)
{
    int a, b;

    a = mfm_read_halfbit(reader);
    b = mfm_read_halfbit(reader);
    if (a < 0 || b < 0)
        return -1;
    return b;
}

/*
 * Декодирование очередного байта.
 */
int mfm_read_byte(mfm_reader_t *reader)
{
    int byte, bit, i;

    byte = 0;
    for (i=0; i<8; ++i) {
        bit = mfm_read_bit(reader);
        if (bit < 0)
            return 0;
        byte <<= 1;
        byte |= bit;
    }
    return byte;
}

/*
 * Подготовка к чтению очередной дорожки.
 */
void mfm_read_seek(mfm_reader_t *reader, FILE *fin, int t)
{
    reader->fd = fin;
    reader->track = t;
    reader->halfbit = 0;
    fseek(reader->fd, t * 12800L, SEEK_SET);
}

/*
 * Подготовка к записи очередной дорожки.
 */
void mfm_write_reset(mfm_writer_t *writer, FILE *fout)
{
    writer->fd = fout;
    writer->halfbit = 0;
    writer->last = 0;
}

/*
 * Кодирование одного полубита.
 */
void mfm_write_halfbit(mfm_writer_t *writer, int val)
{
    /* Дорожка закончилась. */
    if (writer->halfbit >= 102400)
        return;

    val &= 1;
    writer->byte <<= 1;
    writer->byte |= val;
    writer->last = val;
    ++writer->halfbit;
    if ((writer->halfbit & 7) == 0)
        putc(writer->byte, writer->fd);
}

/*
 * Кодирование одного бита.
 */
void mfm_write_bit(mfm_writer_t *writer, int val)
{
    if (val & 1) {
        /* Кодируем единицу. */
        mfm_write_halfbit(writer, 0);
        mfm_write_halfbit(writer, 1);
    } else {
        /* Кодируем ноль. */
        mfm_write_halfbit(writer, ! writer->last);
        mfm_write_halfbit(writer, 0);
    }
}

/*
 * Кодирование очередного байта.
 */
void mfm_write_byte(mfm_writer_t *writer, int val)
{
    int i;

    for (i=0; i<8; ++i) {
        mfm_write_bit(writer, val >> 7);
        val <<= 1;
    }
}

/*
 * Кодирование массива байтов.
 */
void mfm_write(mfm_writer_t *writer, unsigned char *data, int nbytes)
{
    while (nbytes-- > 0)
        mfm_write_byte(writer, *data++);
}

/*
 * Заполнение зазора.
 */
void mfm_write_gap(mfm_writer_t *writer, int nbytes, int val)
{
    while (nbytes-- > 0)
        mfm_write_byte(writer, val);
}

/*
 * Заполнение зазора до конца дорожки.
 */
void mfm_fill_track(mfm_writer_t *writer, int val)
{
    while (writer->halfbit < 102400)
        mfm_write_byte(writer, val);
}

void mfm_dump(FILE *fin, int ntracks)
{
    mfm_reader_t reader;
    int t, i, a, b, last_b;

    for (t=0; t<ntracks; ++t) {
        mfm_read_seek(&reader, fin, t);
        a = b = last_b = 0;
        fprintf(mfm_err, "Track %d/%d:\n", t >> 1, t & 1);
        for (i=0;; ++i) {
            if (mfm_verbose)
                b = mfm_read_halfbit(&reader);
            else {
                last_b = b;
                a = mfm_read_halfbit(&reader);
                b = mfm_read_halfbit(&reader);
                if (! a && ! b && last_b)
                    a = 1;
            }
            if (b < 0)
                break;
            if (mfm_verbose || a != b)
                fprintf(mfm_err, "%d", b);
            else
                fprintf(mfm_err, b ? "#" : "_");
            if ((i & 63) == 63)
                fprintf(mfm_err, "\n");
        }
        fprintf(mfm_err, "\n");
    }
}
