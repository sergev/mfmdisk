/*
 * Copyright (GPL) 2008 Serge Vakulenko <serge@vak.ru>
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "config.h"
#include "mfm.h"

/*
 * Чтение образа дискеты из файла в традиционном бинарном виде.
 */
void mfm_read_raw (mfm_disk_t *d, FILE *fin, int nsectors_per_track)
{
	int t, s;
	struct stat st;

	if (fstat (fileno (fin), &st) < 0) {
		fprintf (mfm_err, "Cannot fstat() input file, aborted.\n");
		exit (-1);
	}
	d->ntracks = st.st_size / SECTSZ / nsectors_per_track;
	d->nsectors_per_track = nsectors_per_track;
	if (d->ntracks > MAXTRACK) {
		fprintf (mfm_err, "Too many tracks = %d, aborted.\n",
			d->ntracks);
		exit (-1);
	}
	fseek (fin, 0L, SEEK_SET);
	for (t=0; t<d->ntracks; ++t) {
		for (s=0; s<d->nsectors_per_track; ++s) {
			if (fread (d->block[t][s], SECTSZ, 1, fin) != 1) {
				fprintf (mfm_err, "Error reading input file, aborted.\n");
				exit (-1);
			}
		}
	}
}

/*
 * Запись образа дискеты в файл в традиционном бинарном виде.
 */
void mfm_write_raw (mfm_disk_t *d, FILE *fout)
{
	int t, s;

	for (t=0; t<d->ntracks; ++t) {
		for (s=0; s<d->nsectors_per_track; ++s) {
			fwrite (d->block[t][s], SECTSZ, 1, fout);
		}
	}
}
