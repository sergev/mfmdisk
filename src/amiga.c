/*
 * Copyright (GPL) 2008 Serge Vakulenko <serge@vak.ru>
 */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "mfm.h"

/*
 * Первый аргумент содержит нечётные биты 32-битного слова,
 * второй - чётные биты. Возвращаем значение исходного слова.
 */
static unsigned long shuffle (int odd, int even)
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
int mfm_scan_amiga (mfm_reader_t *reader)
{
	int bit, tag;
	unsigned long history;

	history = 0;
	for (;;) {
		bit = mfm_read_bit (reader);
		if (bit < 0)
			return -1;
		history = history << 1 | bit;

		if ((history & 0xffffffff) == 0xffffffff) {
			/* Все единицы - подсинхронизовываемся на полубит. */
			mfm_read_halfbit (reader);
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
int mfm_detect_amiga (FILE *fin)
{
	mfm_reader_t reader;
	unsigned long history;
	int bit;

	mfm_read_seek (&reader, fin, 0);
	history = 0x13713713;
	for (;;) {
		bit = mfm_read_bit (&reader);
		if (bit < 0)
			return -1;
		history = history << 1 | bit;
		history &= 0xffffffff;

		/* Все единицы - подсинхронизовываемся на полубит. */
		if (history == 0xffffffff) {
			mfm_read_halfbit (&reader);
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
static unsigned long read_long (mfm_reader_t *reader, unsigned long *sum)
{
	int odd, even;

	odd = mfm_read_byte (reader) << 8;
	odd |= mfm_read_byte (reader);
	even = mfm_read_byte (reader) << 8;
	even |= mfm_read_byte (reader);
	*sum ^= odd ^ even;
	return shuffle (odd, even);
}

/*
 * Чтение 512-байтного блока с перестановкой битов.
 * Возвращаем контрольную сумму.
 */
static int read_data (mfm_reader_t *reader, unsigned char *data)
{
	unsigned long ldata;
	int odd [128], even [128], sum, i;

	/* Первая половина - нечётные биты. */
	for (i=0; i<SECTSZ/4; ++i) {
		odd[i] = mfm_read_byte (reader) << 8;
		odd[i] |= mfm_read_byte (reader);
	}
	/* Вторая половина - чётные биты. */
	for (i=0; i<SECTSZ/4; ++i) {
		even[i] = mfm_read_byte (reader) << 8;
		even[i] |= mfm_read_byte (reader);
	}
	/* Восстанавливаем данные. */
	sum = 0;
	for (i=0; i<SECTSZ/4; ++i) {
		ldata = shuffle (odd[i], even[i]);
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
int mfm_read_sector_amiga (mfm_reader_t *reader, unsigned char *data)
{
	int tag, track, sector, odd, even;
	unsigned long label[4], header_sum, data_sum;
	unsigned long my_header_sum, my_data_sum;

	for (;;) {
		tag = mfm_scan_amiga (reader);
		if (tag < 0)
			return -1;
		odd = (tag << 8) | mfm_read_byte (reader);
		even = mfm_read_byte (reader) << 8;
		even |= mfm_read_byte (reader);
		tag = shuffle (odd, even) & 0xffffff;
		track = tag >> 16;
		sector = tag >> 8 & 0xff;
		my_header_sum = odd ^ even;

		/* Метка сектора. */
		label[0] = read_long (reader, &my_header_sum);
		label[1] = read_long (reader, &my_header_sum);
		label[2] = read_long (reader, &my_header_sum);
		label[3] = read_long (reader, &my_header_sum);

		header_sum = mfm_read_byte (reader) << 24;
		header_sum |= mfm_read_byte (reader) << 16;
		header_sum |= mfm_read_byte (reader) << 8;
		header_sum |= mfm_read_byte (reader);
		if (my_header_sum != header_sum) {
			fprintf (mfm_err, "track %d sector %d: header sum %08lx, expected %08lx\n",
				track, sector, my_header_sum, header_sum);
			return -1;
		}
		if (track != reader->track) {
			fprintf (mfm_err, "track %d, sector %d: incorrect track number, expected %d\n",
				track, sector, reader->track);
		}

		data_sum = mfm_read_byte (reader) << 24;
		data_sum |= mfm_read_byte (reader) << 16;
		data_sum |= mfm_read_byte (reader) << 8;
		data_sum |= mfm_read_byte (reader);

		my_data_sum = read_data (reader, data);
		if (my_data_sum != data_sum)
			fprintf (mfm_err, "track %d sector %d: data sum %08lx, expected %08lx\n",
				track, sector, my_data_sum, data_sum);
		return sector;
	}
}

/*
 * Читаем дискету Amiga из MFM-файла. Количество дорожек (до 160)
 * задаётся параметром ntracks.
 */
void mfm_read_amiga (mfm_disk_t *d, FILE *fin, int ntracks)
{
	int t, s;
	mfm_reader_t reader;
	unsigned char block [SECTSZ];
	int have_sector [MAXSECT];

	d->ntracks = ntracks;
	d->nsectors_per_track = 11;
	for (t=0; t<d->ntracks; ++t) {
		mfm_read_seek (&reader, fin, t);
		for (s=0; s<MAXSECT; ++s)
			have_sector [s] = 0;
		for (;;) {
			s = mfm_read_sector_amiga (&reader, block);
			if (s < 0)
				break;
			if (s >= d->nsectors_per_track) {
				fprintf (mfm_err, "track %d: too large sector number %d\n",
					t, s);
				continue;
			}
			/* Сектора могут следовать в произвольном порядке. */
			have_sector [s] = 1;
			memcpy (d->block[t][s], block, SECTSZ);
		}

		/* Проверим, что получили все сектора. */
		for (s=0; s<d->nsectors_per_track; ++s) {
			if (! have_sector [s])
				fprintf (mfm_err, "track %d: no sector %d\n",
					t, s);
		}
	}
}

/*
 * Исследуем и печатаем информацию о дискете Amiga из MFM-файла.
 * Количество дорожек (до 160) задаётся параметром ntracks.
 */
void mfm_analyze_amiga (FILE *fin, int ntracks)
{
	fprintf (mfm_err, "Format: Amiga\n");
	/* TODO */
}

/*
 * Записываем MFM-образ флоппи-диска в формате Amiga.
 */
void mfm_write_amiga (mfm_disk_t *d, FILE *fout)
{
	fprintf (mfm_err, "Writing Amiga floppies not implemented yet, sorry.\n");
	/* TODO */
}
