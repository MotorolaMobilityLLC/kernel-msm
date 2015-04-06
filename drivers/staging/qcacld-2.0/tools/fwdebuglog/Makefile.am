AM_CFLAGS = -Wall \
            $(DIAG_CFLAGS)

if DEBUG
AM_CFLAGS += -g
else
AM_CFLAGS += -O2
endif

AM_CFLAGS += -I $(top_srcdir)/../CORE/SERVICES/COMMON/

cld_fwlog_record_SOURCES = cld-fwlog-record.c

cnss_diag_SOURCES = cld-fwlog-netlink.c parser.c
cnss_diag_LDADD = $(DIAG_LIBS)

cld_fwlog_parser_SOURCES = cld-fwlog-parser.c

bin_PROGRAMS = cld_fwlog_record
bin_PROGRAMS += cnss_diag
bin_PROGRAMS += cld_fwlog_parser
