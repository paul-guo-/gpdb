# src/bin/pg_archivecleanup/nls.mk
CATALOG_NAME     = pg_archivecleanup
AVAIL_LANGUAGES  =de es fr ja ko pl ru sv tr vi
GETTEXT_FILES    = $(FRONTEND_COMMON_GETTEXT_FILES) pg_archivecleanup.c
GETTEXT_TRIGGERS = $(FRONTEND_COMMON_GETTEXT_TRIGGERS)
GETTEXT_FLAGS    = $(FRONTEND_COMMON_GETTEXT_FLAGS)
