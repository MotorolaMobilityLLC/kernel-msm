/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */



#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <athdefs.h>
#include <a_types.h>
#include "dbglog_host.h"

static FILE *fwlog_in;
static FILE *fwlog_res;
static FILE *log_out;
const char *fwlog_res_file;
unsigned char buf[RECLEN];
int max_records;
int record;

static unsigned int get_le32(const unsigned char *pos)
{
    return pos[0] | (pos[1] << 8) | (pos[2] << 16) | (pos[3] << 24);
}

static size_t capture(FILE *out_log, FILE *in_log)
{
    size_t res;
    struct dbglog_slot *slot;
    unsigned int length = 0, timestamp = 0, dropped = 0;


    while ((res = fread(buf, RECLEN, 1, in_log)) == 1)  {
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((unsigned char *)&slot->timestamp);
        length = get_le32((unsigned char *)&slot->length);
        dropped = get_le32((unsigned char *)&slot->dropped);
        printf("Read record timestamp=%u length=%u no. fw dropped=%u\n",
               timestamp, length, dropped);
        fseek(out_log, record * RECLEN, SEEK_SET);
        if (fwrite(buf, RECLEN, res, out_log) != res)
                perror("fwrite");
        record++;
        if (record == max_records)
                record = 0;
    }

    return res;
}

static size_t reorder(FILE *log_in, FILE *log_out)
{
    unsigned char buf[RECLEN];
    size_t res;
    unsigned int timestamp, min_timestamp = -1;
    int pos = 0, min_pos = 0;
    struct dbglog_slot *slot;
    unsigned int length = 0;

    pos = 0;
    while ((res = fread(buf, RECLEN, 1, log_in)) == 1) {
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((unsigned char *)&slot->timestamp);
        if (timestamp < min_timestamp) {
                min_timestamp = timestamp;
                min_pos = pos;
        }
        pos++;
    }
    printf("First record at position %d\n", min_pos);

    fseek(log_in, min_pos * RECLEN, SEEK_SET);
    while ((res = fread(buf, RECLEN, 1, log_in)) == 1) {
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((unsigned char *)&slot->timestamp);
        length = get_le32((unsigned char *)&slot->length);
        printf("Read record timestamp=%u length=%u\n",
               timestamp, length);
        if (fwrite(buf, RECLEN, res, log_out) != res)
               perror("fwrite");
    }

    fseek(log_in, 0, SEEK_SET);
    pos = min_pos;
    while (pos > 0 && (res = fread(buf, RECLEN, 1, log_out)) == 1) {
        pos--;
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((unsigned char *)&slot->timestamp);
        length = get_le32((unsigned char *)&slot->length);
        printf("Read record timestamp=%u length=%u\n",
               timestamp, length);
        if (fwrite(buf, RECLEN, res, log_out) != res)
                perror("fwrite");
    }

    return 0;
}

static void cleanup(void) {
    fclose(fwlog_in);

    fwlog_res = fopen(fwlog_res_file, "w");

    if (fwlog_res == NULL) {
        perror("Failed to open reorder fwlog file");
        fclose(log_out);
        return;
    }

    reorder(log_out, fwlog_res);

    fclose(fwlog_res);
    fclose(log_out);
}

static void stop(int signum)
{
    printf("Recording stopped\n");
    cleanup();
    exit(0);
}

int main(int argc, char *argv[])
{
    int max_len;
    size_t res;

    if (argc != 4) {
        fprintf(stderr, "usage:\n"
            "%s <path to fwlog_block> <path to log file> \\\n"
            "    <max length>\n"
            "for example:\n"
            "cld-fwlog-record /sys/kernel/debug"
            "/cld/dbglog_block \\\n"
            "    /tmp/cld-fwlog 1000000\n",
            argv[0]);
        return -1;
    }

    max_len = atoi(argv[3]);
    if (max_len < RECLEN) {
        fprintf(stderr, "Too small maximum length (has to be >= %d)\n",
                RECLEN);
        return -1;
    }
    max_records = max_len / RECLEN;
    printf("Storing last %d records\n", max_records);

    fwlog_in = fopen(argv[1], "r");
    if (fwlog_in == NULL) {
        perror("Failed to open fwlog_block file");
        return -1;
    }

    log_out = fopen(argv[2], "w");
    if (log_out == NULL) {
        perror("Failed to create output file");
        fclose(fwlog_in);
        return -1;
    }

    fwlog_res_file = "./reorder";

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    res = capture(log_out, fwlog_in);

    printf("Incomplete read: %d bytes\n", (int) res);

    cleanup();

    return 0;
}
