/*
 * Copyright (GPL) 2008 Serge Vakulenko <serge@vak.ru>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "config.h"
#include "mfm.h"

enum {
	ACTION_INFO,
	ACTION_EXTRACT,
	ACTION_CREATE,
	ACTION_DUMP,
};

mfm_disk_t disk;

void usage ()
{
	printf ("mfmdisk utility, version %s\n", PACKAGE_VERSION);
	printf ("\n");

	printf ("Usage:\n");
	printf ("    mfmdisk [-i] input.mfm\n");
	printf ("    mfmdisk -x input.mfm output.img\n");
	printf ("    mfmdisk -c output.mfm [input.img]\n");
	printf ("\n");

	printf ("Options:\n");
	printf ("    -i, --info         show information about MFM file\n");
	printf ("    -x, --extract      extract data from MFM file\n");
	printf ("    -c, --create       create MFM file\n");
	printf ("    -d, --dump         dump raw bit contents of MFM file\n");
	printf ("    -v, --verbose      verbose mode\n");
	printf ("    -a, --amiga        use Amiga format (default IBM PC)\n");
	printf ("    -b, --bk           use BK-0010 format\n");
	printf ("    -s N, --sectors-per-track=N\n");
	printf ("                       use N sectors per track\n");
	exit (-1);
}

/*
 * Открытие бинарного файла на чтение.
 * Если имя "-", то используем стандартный ввод.
 */
FILE *open_input (char *filename)
{
	FILE *fin;

	if (strcmp (filename, "-") == 0)
		return stdin;

	fin = fopen (filename, "rb");
	if (! fin) {
		perror (filename);
		exit (-1);
	}
	return fin;
}

/*
 * Открытие бинарного файла на запись.
 * Если имя "-", то используем стандартный вывод,
 * а диагностику переключаем на стандартный вывод ошибок.
 */
FILE *open_output (char *filename)
{
	FILE *fout;

	if (strcmp (filename, "-") == 0) {
		mfm_err = stderr;
		return stdout;
	}
	fout = fopen (filename, "wb");
	if (! fout) {
		perror (filename);
		exit (-1);
	}
	return fout;
}

int main (int argc, char **argv)
{
	static struct option longopts[] = {
		/* option	     has arg	integer code */
		{ "help",		0, 0,	'h'		},
		{ "version",		0, 0,	'V'		},
		{ "info",		0, 0,	'i'		},
		{ "extract",		0, 0,	'x'		},
		{ "create",		0, 0,	'c'		},
		{ "dump",		0, 0,	'd'		},
		{ "amiga",		0, 0,	'a'		},
		{ "bk",			0, 0,	'b'		},
		{ "sectors-per-track",	0, 0,	's'		},
		{ 0,			0, 0,	0		},
	};
	int c;
	FILE *fin, *fout;
	int action = ACTION_INFO;
	int amiga = 0;
	int bk = 0;
	int nsectors_per_track = 9;

	mfm_err = stdout;
	for (;;) {
		c = getopt_long (argc, argv, "hVixcdvabs:", longopts, 0);
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage ();
			return 0;
		case 'V':
			printf ("Version: %s\n", PACKAGE_VERSION);
			return 0;
		case 'i':
			action = ACTION_INFO;
			break;
		case 'x':
			action = ACTION_EXTRACT;
			break;
		case 'c':
			action = ACTION_CREATE;
			break;
		case 'd':
			action = ACTION_DUMP;
			break;
		case 'v':
			++mfm_verbose;
			break;
		case 'a':
			amiga = 1;
			bk = 0;
			nsectors_per_track = 11;
			break;
		case 'b':
			bk = 1;
			amiga = 0;
			nsectors_per_track = 10;
			break;
		case 's':
			nsectors_per_track = strtol (optarg, 0, 0);
			break;
		}
	}

	switch (action) {
	default:
	case ACTION_INFO:
		/* Выдача информации о файле MFM. */
		if (optind != argc-1)
			usage ();
		fin = open_input (argv[optind]);

		if (mfm_detect_amiga (fin))
			mfm_analyze_amiga (fin, MAXTRACK);
		else
			mfm_analyze_ibmpc (fin, MAXTRACK);
		break;

	case ACTION_DUMP:
		/* Выдача битового содержимого файла MFM. */
		if (optind != argc-1)
			usage ();
		fin = open_input (argv[optind]);
		mfm_dump (fin, MAXTRACK);
		break;

	case ACTION_EXTRACT:
		/* Извлечение данных из файла MFM. */
		if (optind != argc-2)
			usage ();
		fin = open_input (argv[optind]);
		fout = open_output (argv[optind+1]);

		if (amiga || mfm_detect_amiga (fin))
			mfm_read_amiga (&disk, fin, MAXTRACK);
		else
			mfm_read_ibmpc (&disk, fin, MAXTRACK);

		mfm_write_raw (&disk, fout);
		break;

	case ACTION_CREATE:
		/* Создание файла MFM. */
		if (optind < argc-2 || optind > argc-1)
			usage ();
		fout = open_output (argv[optind]);
		fin = open_input (argv[optind+1]);

		mfm_read_raw (&disk, fin, nsectors_per_track);

		if (! mfm_index_gap)
			mfm_index_gap = INDEX_GAP;
		if (! mfm_data_gap)
			mfm_data_gap = DATA_GAP;
		if (! mfm_sector_gap) {
			mfm_sector_gap = (nsectors_per_track == 10) ?
				SECTOR_GAP_10 : SECTOR_GAP_9;
		}

		if (amiga)
			mfm_write_amiga (&disk, fout);
		else
			mfm_write_ibmpc (&disk, fout, bk);
		break;
	}
	return 0;
}
