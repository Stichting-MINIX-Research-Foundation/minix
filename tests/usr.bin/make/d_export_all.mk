# $NetBSD: d_export_all.mk,v 1.1 2012/03/17 16:33:14 jruoho Exp $

UT_OK=good
UT_F=fine

.export

.include "d_export.mk"

UT_TEST=export-all
UT_ALL=even this gets exported
