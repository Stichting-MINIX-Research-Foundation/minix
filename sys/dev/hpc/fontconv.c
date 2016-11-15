/*	$NetBSD: fontconv.c,v 1.3 2001/11/13 12:47:56 lukem Exp $	*/

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fontconv.c,v 1.3 2001/11/13 12:47:56 lukem Exp $");

#include <stdio.h>


int width, height, ascent;
char *fontname;
FILE *ifp;
FILE *ofp;


void fc_rcons(int ac, char* av[]);
void fc_rasops(int ac, char* av[]);


main(int ac, char* av[])
{
	ifp = stdin;
	ofp = stdout;
	width = 8;
	height = 8;
	ascent = 8;
	fontname = "vt220l";

	/*
	  fc_rcons(ac, av);
	*/
	fc_rasops(ac, av);
}


void
fc_rasops(int ac, char* av[])
{
	int code;
	int width_in_bytes;
	long code_min = 0x10000;
	long code_max = -1;

	width_in_bytes = (width + 7) / 8;

	code = 0;
	fprintf(ofp, "static u_char %s%dx%d_data[] = {\n",
	    fontname, width, height, code);
	while (1) {
		int n;
		int i, j, k;
		unsigned char buf[200];

		n = fread(buf, width_in_bytes, height, ifp);
		if (n != height) {
			if (!feof(ifp)) {
				perror("fread()");
				exit(1);
			} else {
				break;
			}
		}

		k = 0;
		fprintf(ofp, "    /* code %d */\n", code);
		for (i = 0; i < height; i++) {
			unsigned long d = 0, m;
			fprintf(ofp, "    ");
			for (j = 0; j < width_in_bytes; j++) {
				d |= (((unsigned long)buf[k]) << (24 - 8*j));
				fprintf(ofp, "0x%02x,", (buf[k] & 0xff));
				k++;
			}
			fprintf(ofp, " /* ");
			for (m = 0x80000000, j = 0; j < width; j++, m >>= 1) {
				printf((m & d) ? "##" : "..");
			}
			fprintf(ofp, " */\n");
		}
		fprintf(ofp, "\n");
		if (code < code_min) {
			code_min = code;
		}
		if (code_max < code) {
			code_max = code;
		}
		code++;
	}
	fprintf(ofp, "};\n");

	fprintf(ofp, "struct wsdisplay_font %s%dx%d = {\n",
	    fontname, width, height);
	fprintf(ofp, "    \"%s\",\t\t\t/* typeface name */\n", fontname);
	fprintf(ofp, "    0x%02x,\t\t\t/* firstchar */\n", code_min);
	fprintf(ofp, "    %d,\t\t\t/* numchars */\n", code_max - code_min + 1);
	fprintf(ofp, "    WSDISPLAY_FONTENC_ISO,\t/* encoding */\n");
	fprintf(ofp, "    %d,\t\t\t\t/* width */\n", width);
	fprintf(ofp, "    %d,\t\t\t\t/* height */\n", height);
	fprintf(ofp, "    %d,\t\t\t\t/* stride */\n", width_in_bytes);
	fprintf(ofp, "    WSDISPLAY_FONTENC_L2R,\t/* bit order */\n");
	fprintf(ofp, "    WSDISPLAY_FONTENC_L2R,\t/* byte order */\n");
	fprintf(ofp, "    %s%dx%d_data\t\t/* data */\n",
	    fontname, width, height);
	fprintf(ofp, "};\n");
}


void
fc_rcons(int ac, char* av[])
{
	int code;
	int width_in_bytes;
	long code_min = 0x10000;
	long code_max = -1;

	width_in_bytes = (width + 7) / 8;

	code = 0;
	while (1) {
		int n;
		int i, j, k;
		unsigned char buf[200];

		n = fread(buf, width_in_bytes, height, ifp);
		if (n != height) {
			if (!feof(ifp)) {
				perror("fread()");
				exit(1);
			} else {
				break;
			}
		}

		fprintf(ofp, "static u_int32_t %s%dx%d_%d_pix[] = {\n",
		    fontname, width, height, code);

		k = 0;
		for (i = 0; i < height; i++) {
			unsigned long d = 0, m;
			for (j = 0; j < width_in_bytes; j++) {
				d |= (((unsigned long)buf[k++]) << (24 - 8*j));
			}
			fprintf(ofp, "    0x%08x, /* ", d);
			for (m = 0x80000000, j = 0; j < width; j++, m >>= 1) {
				printf((m & d) ? "##" : "..");
			}
			fprintf(ofp, " */\n");
		}
		fprintf(ofp, "};\n");
		fprintf(ofp, "static struct raster %s%dx%d_%d = {",
		    fontname, width, height, code);
		fprintf(ofp, " %d, %d, 1, 1, %s%dx%d_%d_pix, 0 };\n",
		    width, height, fontname, width, height, code);
		if (code < code_min) {
			code_min = code;
		}
		if (code_max < code) {
			code_max = code;
		}
		code++;
	}

	fprintf(ofp, "struct raster_font %s%dx%d = {\n",
	    fontname, width, height);
	fprintf(ofp, "    %d, %d, %d, ", width, height, ascent);
	fprintf(ofp, "RASFONT_FIXEDWIDTH|RASFONT_NOVERTICALMOVEMENT,\n");
	fprintf(ofp, "    {\n");
	for (code = code_min; code <= code_max; code++) {
		fprintf(ofp, "        { &%s%dx%d_%d, ",
		    fontname, width, height, code);
		fprintf(ofp, "%d, %d, %d, %d },\n", 0, -ascent, width, 0);
	}
	fprintf(ofp, "    },\n");
	fprintf(ofp, "#ifdef COLORFONT_CACHE\n");
	fprintf(ofp, "    (struct raster_fontcache*) -1\n");
	fprintf(ofp, "#endif /*COLORFONT_CACHE*/\n");
	fprintf(ofp, "};\n");
}
