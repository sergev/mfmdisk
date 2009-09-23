/*
 * Copyright (GPL) 2008 Serge Vakulenko <serge@vak.ru>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "mfm.h"

/*
 * Checksum lookup table.
 * CRC-CCITT = x^16 + x^12 + x^5 + 1
 */
static const unsigned short poly_tab [256] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
	0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
	0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
	0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
	0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
	0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
	0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
	0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
	0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
	0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
	0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
	0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
	0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
	0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
	0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
	0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
	0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
	0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
	0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
	0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
	0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
	0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
	0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/*
 * Calculate a new sum given the current sum and the new data.
 * Use 0xffff as the initial sum value.
 * Do not forget to invert the final checksum value.
 */
static unsigned short crc16_ccitt (unsigned short sum,
	unsigned const char *buf, unsigned int len)
{
	while (len--) {
		sum = (sum << 8) ^ poly_tab [*buf++ ^ (sum >> 8)];
	}
	return sum;
}

static unsigned short crc16_ccitt_byte (unsigned short sum, unsigned char byte)
{
	sum = (sum << 8) ^ poly_tab [byte ^ (sum >> 8)];
	return sum;
}

static int print_gap (unsigned long history, int printed)
{
	int byte;

	byte = (unsigned char) history;
	if (byte != (unsigned char) (history >> 8) ||
	    byte != (unsigned char) (history >> 16) ||
	    byte != (unsigned char) (history >> 24))
		return printed;
	if (printed == byte || byte == 0 ||
	    printed == (unsigned char) (history >> 1) ||
	    printed == (unsigned char) (history >> 2) ||
	    printed == (unsigned char) (history >> 3) ||
	    printed == (unsigned char) (history >> 4) ||
	    printed == (unsigned char) (history >> 5) ||
	    printed == (unsigned char) (history >> 6) ||
	    printed == (unsigned char) (history >> 7))
		return printed;
	fprintf (mfm_err, "Fill: %d%d%d%d%d%d%d%d\n",
		byte >> 7 & 1, byte >> 6 & 1, byte >> 5 & 1, byte >> 4 & 1,
		byte >> 3 & 1, byte >> 2 & 1, byte >> 1 & 1, byte & 1);
	return byte;
}

/*
 * Поиск идентификатора сектора на дискете формата IBM PC.
 */
int mfm_scan_ibmpc (mfm_reader_t *reader, int *nbits_read)
{
	int bit, tag, gap_printed = 0;
	unsigned long history;

	history = 0x13713713;
	if (nbits_read)
		*nbits_read = 0;
	for (;;) {
		bit = mfm_read_bit (reader);
		if (bit < 0) {
			if (mfm_verbose && nbits_read)
				fprintf (mfm_err, "Track %d/%d: final gap %d bits\n",
					reader->track >> 1, reader->track & 1,
					*nbits_read);
			return -1;
		}
		history = history << 1 | bit;
		history &= 0xffffffff;
		if (nbits_read)
			++*nbits_read;

		/* Все единицы - подсинхронизовываемся на полубит. */
		if (history == 0xffffffff) {
			mfm_read_halfbit (reader);
			history = 0;
			continue;
		}

		if (mfm_verbose > 1)
			gap_printed = print_gap (history, gap_printed);

		/* Формат IBM PC: ждем 00-a1-a1-a1 или 00-c2-c2-c2. */
		if (history == 0x00a1a1a1 || history == 0x00c2c2c2) {
			/* Нашли маркер, читаем и возвращаем его тег. */
			tag = mfm_read_byte (reader);
			return tag;
		}
	}
}

/*
 * Чтение очередного сектора с дискеты формата IBM PC.
 */
int mfm_read_sector_ibmpc (mfm_reader_t *reader, unsigned char *data,
	int *sector_gap, int *data_gap)
{
	int tag, cylinder, head, track, sector, size, gap, i;
	unsigned short header_sum, data_sum, my_header_sum, my_data_sum;

	if (sector_gap)
		*sector_gap = 0;
	for (;;) {
		tag = mfm_scan_ibmpc (reader, &gap);
		if (sector_gap)
			*sector_gap += gap;
		if (tag < 0)
			return -1;
		if (tag != 0xfe) {
			if (mfm_verbose) {
				fprintf (mfm_err, "Track %d/%d: tag %02X",
					reader->track >> 1, reader->track & 1,
					tag);
				if (sector_gap) {
					fprintf (mfm_err, ", gap %d bits",
						*sector_gap - 15*8);
					*sector_gap = 0;
				}
				fprintf (mfm_err, "\n");
			}
			continue;
		}
ident:		cylinder = mfm_read_byte (reader);
		head = mfm_read_byte (reader);
		sector = mfm_read_byte (reader);
		size = mfm_read_byte (reader);
		header_sum = mfm_read_byte (reader) << 8;
		header_sum |= mfm_read_byte (reader);

		my_header_sum = crc16_ccitt_byte (0xb230, cylinder);
		my_header_sum = crc16_ccitt_byte (my_header_sum, head);
		my_header_sum = crc16_ccitt_byte (my_header_sum, sector);
		my_header_sum = crc16_ccitt_byte (my_header_sum, size);
		if (my_header_sum != header_sum) {
			fprintf (mfm_err, "Track %d/%d: header sum %04x, expected %04x\n",
				reader->track >> 1, reader->track & 1,
				my_header_sum, header_sum);
			continue;
		}
		track = cylinder * 2 + head;
		if (track != reader->track) {
			fprintf (mfm_err, "Track %d/%d sector %d: incorrect c/h = %d/%d\n",
				reader->track >> 1, reader->track & 1,
				sector, cylinder, head);
		}
		if (size != 2) {
			fprintf (mfm_err, "Track %d/%d sector %d: incorrect block size = %d\n",
				reader->track >> 1, reader->track & 1,
				sector, size);
		}
		tag = mfm_scan_ibmpc (reader, data_gap);
		if (tag < 0)
			return -1;
		if (tag == 0xfe) {
			if (sector_gap)
				*sector_gap += *data_gap + 6*8;
			if (mfm_verbose)
				fprintf (mfm_err, "Track %d/%d sector %d: incorrect data tag %02X\n",
					reader->track >> 1, reader->track & 1,
					sector, tag);
			goto ident;
		}
		if (tag != 0xfb) {
			fprintf (mfm_err, "Track %d/%d sector %d: invalid tag %02X\n",
				reader->track >> 1, reader->track & 1,
				sector, tag);
		}
		for (i=0; i<SECTSZ; ++i)
			data[i] = mfm_read_byte (reader);
		data_sum = mfm_read_byte (reader) << 8;
		data_sum |= mfm_read_byte (reader);

		my_data_sum = crc16_ccitt_byte (0xcdb4, tag);
		my_data_sum = crc16_ccitt (my_data_sum, data, SECTSZ);
		if (my_data_sum != data_sum) {
			fprintf (mfm_err, "Track %d/%d sector %d: data sum %04x, expected %04x\n",
				reader->track >> 1, reader->track & 1,
				sector, my_data_sum, data_sum);
		}
		return sector - 1;
	}
}

/*
 * Читаем дискету IBM PC из MFM-файла. Количество дорожек (до 160)
 * задаётся параметром ntracks.
 */
void mfm_read_ibmpc (mfm_disk_t *d, FILE *fin, int ntracks)
{
	int t, s;
	mfm_reader_t reader;
	unsigned char block [SECTSZ];
	int have_sector [MAXSECT];

	d->ntracks = ntracks;
	d->nsectors_per_track = 10;
	for (t=0; t<d->ntracks; ++t) {
		mfm_read_seek (&reader, fin, t);
		for (s=0; s<MAXSECT; ++s)
			have_sector [s] = 0;
		for (;;) {
			s = mfm_read_sector_ibmpc (&reader, block, 0, 0);
			if (s < 0)
				break;
			if (s >= d->nsectors_per_track) {
				fprintf (mfm_err, "Track %d/%d: too large sector number %d\n",
					t >> 1, t & 1, s + 1);
				continue;
			}
			/* Сектора могут следовать в произвольном порядке. */
			have_sector [s] = 1;
			memcpy (d->block[t][s], block, SECTSZ);
		}
		/* Разпознаём количество секторов. */
		if (t == 0 && ! have_sector [9])
			d->nsectors_per_track = 9;

		/* Проверим, что получили все сектора. */
		for (s=0; s<d->nsectors_per_track; ++s)
			if (! have_sector [s])
				break;
		if (s < d->nsectors_per_track) {
			fprintf (mfm_err, "Track %d/%d: no sector",
				t >> 1, t & 1);
			for (; s<d->nsectors_per_track; ++s)
				if (! have_sector [s])
					fprintf (mfm_err, " %d", s);
			fprintf (mfm_err, "\n");
		}
	}
}

/*
 * Исследуем и печатаем информацию о дискете IBM PC из MFM-файла.
 * Количество дорожек (до 160) задаётся параметром ntracks.
 */
void mfm_analyze_ibmpc (FILE *fin, int ntracks)
{
	int t, s, i, nsectors_per_track;
	mfm_reader_t reader;
	unsigned char block [SECTSZ];
	int have_sector [MAXSECT];
	int order_of_sectors [MAXSECT];
	int sector_gap [MAXSECT];
	int data_gap [MAXSECT];

	fprintf (mfm_err, "Format: IBM PC\n");
	for (t=0; t<ntracks; ++t) {
		fprintf (mfm_err, "\n");
		mfm_read_seek (&reader, fin, t);
		for (s=0; s<MAXSECT; ++s)
			have_sector [s] = 0;
		nsectors_per_track = 0;
		for (i=0; ; ++i) {
			s = mfm_read_sector_ibmpc (&reader, block,
				&sector_gap[i], &data_gap[i]);
			if (s < 0)
				break;
			if (s >= MAXSECT) {
				fprintf (mfm_err, "Too many sectors per track = %d, aborted.\n",
					s+1);
				exit (-1);
			}
			if (s >= nsectors_per_track)
				nsectors_per_track = s + 1;

			/* Сектора могут следовать в произвольном порядке. */
			have_sector [s] = 1;
			order_of_sectors [i] = s;
		}
		fprintf (mfm_err, "Track %d/%d: %d sectors per track\n",
			t >> 1, t & 1, nsectors_per_track);
		if (nsectors_per_track < 1)
			continue;

		fprintf (mfm_err, "Order of sectors:");
		for (s=0; s<i; ++s) {
			fprintf (mfm_err, " %d", order_of_sectors[s] + 1);
		}
		fprintf (mfm_err, "\n");

		fprintf (mfm_err, "Sector gap:");
		for (s=0; s<i; ++s) {
			fprintf (mfm_err, " %d", sector_gap[s] - 15*8);
		}
		fprintf (mfm_err, " bits (std %d)\n",
			(nsectors_per_track == 10) ? 46*8 : 80*8);

		fprintf (mfm_err, "Data gap:");
		for (s=0; s<i; ++s) {
			fprintf (mfm_err, " %d", data_gap[s] - 15*8);
		}
		fprintf (mfm_err, " bits (std %d)\n", 22*8);

		/* Проверим, что получили все сектора. */
		for (s=0; s<nsectors_per_track; ++s) {
			if (! have_sector [s])
				fprintf (mfm_err, "No sector %d\n", s + 1);
		}
		if (! mfm_verbose)
			break;
	}
}

/*
 * Записываем идентификатор и его контрольную сумму.
 */
static void write_ident (mfm_writer_t *writer, int t, int s)
{
	int sum;

	mfm_write_byte (writer, t >> 1);
	mfm_write_byte (writer, t & 1);
	mfm_write_byte (writer, s + 1);
	mfm_write_byte (writer, 2);

	sum = crc16_ccitt_byte (0xb230, t >> 1);
	sum = crc16_ccitt_byte (sum, t & 1);
	sum = crc16_ccitt_byte (sum, s + 1);
	sum = crc16_ccitt_byte (sum, 2);
	mfm_write_byte (writer, sum >> 8);
	mfm_write_byte (writer, sum);
}

/*
 * Записываем маркер индекса, с нарушением правил MFM.
 */
static void write_index_marker (mfm_writer_t *writer)
{
	int i;

	/* Двенадцать байтов нулей, для синхронизации. */
	for (i=0; i<12; ++i)
		mfm_write_byte (writer, 0);

	/* Три байта C2 с нарушением кодирования в шестом бите. */
	for (i=0; i<3; ++i) {
		mfm_write_bit (writer, 1);
		mfm_write_bit (writer, 1);
		mfm_write_bit (writer, 0);
		mfm_write_bit (writer, 0);
		mfm_write_bit (writer, 0);
		mfm_write_halfbit (writer, 0);
		mfm_write_halfbit (writer, 0);
		mfm_write_bit (writer, 1);
		mfm_write_bit (writer, 0);
	}
}

/*
 * Записываем маркер идентификатора, с нарушением правил MFM.
 */
static void write_marker (mfm_writer_t *writer)
{
	int i;

	/* Двенадцать байтов нулей, для синхронизации. */
	for (i=0; i<12; ++i)
		mfm_write_byte (writer, 0);

	/* Три байта A1 с нарушением кодирования в шестом бите. */
	for (i=0; i<3; ++i) {
		mfm_write_bit (writer, 1);
		mfm_write_bit (writer, 0);
		mfm_write_bit (writer, 1);
		mfm_write_bit (writer, 0);
		mfm_write_bit (writer, 0);
		mfm_write_halfbit (writer, 0);
		mfm_write_halfbit (writer, 0);
		mfm_write_bit (writer, 0);
		mfm_write_bit (writer, 1);
	}
}

/*
 * Записываем MFM-образ флоппи-диска в формате IBM PC.
 */
void mfm_write_ibmpc (mfm_disk_t *d, FILE *fout, int skip_index_mark)
{
	mfm_writer_t writer;
	int t, s, sum;

	if (mfm_verbose)
		fprintf (mfm_err, "Creating %d tracks, %d sectors per track\n",
			d->ntracks, d->nsectors_per_track);
	for (t=0; t<d->ntracks; ++t) {
		mfm_write_reset (&writer, fout);
		if (! skip_index_mark) {
			mfm_write_gap (&writer, 80, mfm_gap_byte);
			write_index_marker (&writer);
			mfm_write_byte (&writer, 0xfc);
		}
		mfm_write_gap (&writer, mfm_index_gap, mfm_gap_byte);
		for (s=0; s<d->nsectors_per_track; ++s) {
			if (s > 0)
				mfm_write_gap (&writer, mfm_sector_gap, mfm_gap_byte);
			write_marker (&writer);
			mfm_write_byte (&writer, 0xfe);
			write_ident (&writer, t, s);
			mfm_write_gap (&writer, mfm_data_gap, mfm_gap_byte);
			write_marker (&writer);
			mfm_write_byte (&writer, 0xfb);
			mfm_write (&writer, d->block[t][s], SECTSZ);

			sum = crc16_ccitt_byte (0xcdb4, 0xfb);
			sum = crc16_ccitt (sum, d->block[t][s], SECTSZ);
			mfm_write_byte (&writer, sum >> 8);
			mfm_write_byte (&writer, sum);
		}
		mfm_fill_track (&writer, mfm_gap_byte);
	}
}
