/* $NetBSD: overlay.c,v 1.1 2020/06/26 03:23:04 thorpej Exp $ */

/*-
 * Copyright (c) 2020 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared McNeill <jmcneill@invisible.ca> and Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "efiboot.h"
#include "overlay.h"

#include <sys/queue.h>

int	dtoverlay_enabled = 1;

struct dtoverlay_entry {
	TAILQ_ENTRY(dtoverlay_entry) entries;
	char overlay_path[];
};
TAILQ_HEAD(, dtoverlay_entry) dtoverlay_entries =
    TAILQ_HEAD_INITIALIZER(dtoverlay_entries);

#define	DTOVERLAY_ENTRY_SIZE(p)	(sizeof(struct dtoverlay_entry) + strlen(p) + 1)

void
dtoverlay_foreach(void (*fn)(const char *))
{
	struct dtoverlay_entry *d;

	TAILQ_FOREACH(d, &dtoverlay_entries, entries) {
		fn(d->overlay_path);
	}
}

void
dtoverlay_add(const char *overlay_path)
{
	struct dtoverlay_entry *d;

	/* Trim leading whitespace */
	while (*overlay_path == ' ' || *overlay_path == '\t')
		++overlay_path;

	/* Duplicate check */
	TAILQ_FOREACH(d, &dtoverlay_entries, entries) {
		if (strcmp(d->overlay_path, overlay_path) == 0)
			return;
	}

	/* Add to list of overlays. */
	d = alloc(DTOVERLAY_ENTRY_SIZE(overlay_path));
	strcpy(d->overlay_path, overlay_path);
	TAILQ_INSERT_TAIL(&dtoverlay_entries, d, entries);
}

#if 0
void
dtoverlay_remove(const char *overlay_path)
{
	struct dtoverlay_entry *d;

	/* Trim leading whitespace */
	while (*overlay_path == ' ' || *overlay_path == '\t')
		++overlay_path;

	TAILQ_FOREACH(d, &dtoverlay_entries, entries) {
		if (strcmp(d->overlay_path, overlay_path) == 0) {
			TAILQ_REMOVE(&dtoverlay_entries, d, entries);
			dealloc(d, DTOVERLAY_ENTRY_SIZE(d->overlay_path));
			return;
		}
	}
}
#endif

void
dtoverlay_remove_all(void)
{
	struct dtoverlay_entry *d;

	while ((d = TAILQ_FIRST(&dtoverlay_entries)) != NULL) {
		TAILQ_REMOVE(&dtoverlay_entries, d, entries);
		dealloc(d, DTOVERLAY_ENTRY_SIZE(d->overlay_path));
	}
}

void
dtoverlay_enable(int onoff)
{
	dtoverlay_enabled = onoff;
}
