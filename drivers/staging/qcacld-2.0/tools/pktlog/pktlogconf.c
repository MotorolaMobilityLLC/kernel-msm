/*
 * Copyright (c) 2010,2013 The Linux Foundation. All rights reserved.
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdint.h>
#include "pktlog_ac_fmt.h"


int pktlog_enable(char *sysctl_name, unsigned filter)
{
    FILE *fp;

    fp = fopen(sysctl_name, "w");

    printf("Open status of the pktlog_enable:%p %s\n", fp, sysctl_name);
    if (fp != NULL) {
        fprintf(fp, "%i", filter);
        fclose(fp);
        return 0;
    }
    return -1;
}

int
pktlog_options(char *sysctl_options, unsigned long options)
{
    FILE *fp;

    fp = fopen(sysctl_options, "w");
    if (fp == NULL)
        return -1;

    fprintf(fp, "%lu", options);
    fclose(fp);
    return 0;
}

int pktlog_size(char *sysctl_size, char *sysctl_enable, int size)
{
    FILE *fp;

    /* Make sure logging is disabled before changing size */
    fp = fopen(sysctl_enable, "w");

    if (fp == NULL) {
        fprintf(stderr, "Open failed on enable sysctl\n");
        return -1;
    }
    fprintf(fp, "%i", 0);
    fclose(fp);

    fp = fopen(sysctl_size, "w");

    if (fp != NULL) {
        fprintf(fp, "%i", size);
        fclose(fp);
        return 0;
    }
    return -1;
}

void usage()
{
    fprintf(stderr,
            "Packet log configuration\n"
            "usage: pktlogconf [-a adapter] [-e[event-list]] [-d] [-s log-size] [-t -k -l]\n"
            "                  [-b -p -i] \n"
            "    -h    show this usage\n"
            "    -a    configures packet logging for specific 'adapter';\n"
            "          configures system-wide logging if this option is\n"
            "          not specified\n"
            "    -d    disable packet logging\n"
            "    -e    enable logging events listed in the 'event-list'\n"
            "          event-list is an optional comma separated list of one or more\n"
            "          of the following: rx tx rcf rcu ani  (eg., pktlogconf -erx,rcu,tx)\n"
            "    -s    change the size of log-buffer to \"log-size\" bytes\n"
            "    -t    enable logging of TCP headers\n"
            "    -k    enable triggered stop by a threshold number of TCP SACK packets\n"
            "    -l    change the number of packets to log after triggered stop\n"
            "    -b    enable triggered stop by a throuput threshold\n"
            "    -p    enable triggered stop by a PER threshold\n"
// not implemented
//            "    -y    enable triggered stop by a threshold number of Phyerrs\n"
            "    -i    change the time period of counting throughput/PER\n"
            );

    exit(-1);
}


int main(int argc, char *argv[])
{
    int c;
    int size = -1, tail_length = -1, sack_thr = -1;
    int thruput_thresh = -1, per_thresh = -1, phyerr_thresh = -1, trigger_interval = -1;
    unsigned long filter = 0, options = 0;
    char fstr[24];
    char ad_name[24];
    char sysctl_size[128];
    char sysctl_enable[128];
    char sysctl_options[128];
    char sysctl_sack_thr[128];
    char sysctl_tail_length[128];
    char sysctl_thruput_thresh[128];
    char sysctl_phyerr_thresh[128];
    char sysctl_per_thresh[128];
    char sysctl_trigger_interval[128];
    int opt_a=0, opt_d = 0, opt_e = 0, fflag=0;


    for (;;) {
        c = getopt(argc, argv, "s:e::a:d:tk:l:b:p:i:");

        if (c < 0)
            break;

        switch (c) {
            case 't':
                options |= ATH_PKTLOG_PROTO;
                break;
        case 'k': /* triggered stop after # of TCP SACK packets are seen */
            options |= ATH_PKTLOG_TRIGGER_SACK;
            sack_thr = atoi(optarg);
            break;
        case 'l': /* # of tail packets to log after triggered stop */
            tail_length = atoi(optarg);
            break;
        case 's':
            size = atoi(optarg);
            break;
        case 'e':
            if (opt_d) {
                usage();
                exit(-1);
            }
            opt_e = 1;
            if (optarg) {
                fflag = 1;
		snprintf(fstr, sizeof(fstr), "%s", optarg);
		fstr[sizeof(fstr) - 1] = '\0';
            }
            break;
        case 'a':
            opt_a = 1;
	    snprintf(ad_name, sizeof(ad_name), "%s", optarg);
            ad_name[sizeof(ad_name) - 1] = '\0';
            printf("Option a:%s\n", ad_name);
            break;
        case 'd':
	    snprintf(ad_name, sizeof(ad_name), "%s", optarg);
            printf("adname:%s\n", ad_name);
            if (opt_e) {
                usage();
                exit(-1);
            }
            opt_d = 1;
            break;
        case 'b': /* triggered stop after throughput drop below this threshold */
            options |= ATH_PKTLOG_TRIGGER_THRUPUT;
            thruput_thresh = atoi(optarg);
            break;
        case 'p': /* triggered stop after PER increase over this threshold */
            options |= ATH_PKTLOG_TRIGGER_PER;
            per_thresh = atoi(optarg);
            break;
        case 'y': /* triggered stop after # of phyerrs are seen */
            options |= ATH_PKTLOG_TRIGGER_PHYERR;
            phyerr_thresh = atoi(optarg);
            break;
        case 'i': /* time period of counting trigger statistics */
            trigger_interval = atoi(optarg);
            break;
        default:
            usage();
        }
    }

    /*
     * This protection is needed since system wide logging is not supported yet
     */
    if (opt_e) {
        if (!opt_a) {
            printf("Please enter the adapter\n");
            usage();
            exit(-1);
        }
    }

    if (opt_a) {
        snprintf(sysctl_enable, sizeof(sysctl_enable),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable", ad_name);
        snprintf(sysctl_size, sizeof(sysctl_size),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/size", ad_name);
        snprintf(sysctl_options, sizeof(sysctl_options),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/options", ad_name);
        snprintf(sysctl_sack_thr, sizeof(sysctl_sack_thr),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/sack_thr", ad_name);
        snprintf(sysctl_tail_length, sizeof(sysctl_tail_length),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/tail_length", ad_name);
        snprintf(sysctl_thruput_thresh, sizeof(sysctl_thruput_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/thruput_thresh", ad_name);
        snprintf(sysctl_per_thresh, sizeof(sysctl_per_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/per_thresh", ad_name);
        snprintf(sysctl_phyerr_thresh, sizeof(sysctl_phyerr_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/phyerr_thresh", ad_name);
        snprintf(sysctl_trigger_interval, sizeof(sysctl_trigger_interval),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/trigger_interval", ad_name);
    } else {
        snprintf(sysctl_enable, sizeof(sysctl_enable),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/enable");
        snprintf(sysctl_size, sizeof(sysctl_size),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/size");
        snprintf(sysctl_options, sizeof(sysctl_options),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/options");
        snprintf(sysctl_sack_thr, sizeof(sysctl_sack_thr),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/sack_thr");
        snprintf(sysctl_tail_length, sizeof(sysctl_tail_length),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/tail_length");
        snprintf(sysctl_thruput_thresh, sizeof(sysctl_thruput_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/thruput_thresh");
        snprintf(sysctl_per_thresh, sizeof(sysctl_per_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/per_thresh");
        snprintf(sysctl_phyerr_thresh, sizeof(sysctl_phyerr_thresh),
		 "/proc/sys/" PKTLOG_PROC_DIR "/" PKTLOG_PROC_SYSTEM "/phyerr_thresh");
        snprintf(sysctl_trigger_interval, sizeof(sysctl_trigger_interval),
		 "/proc/sys/" PKTLOG_PROC_DIR "/"
        PKTLOG_PROC_SYSTEM "/trigger_interval");
    }

    if (opt_d) {
        /*
         * Need to be removed
         * Must disbale the entire system
         * However doing the above does not work for individual adapter logging
         * Needs fix
         */
        snprintf(sysctl_enable, sizeof(sysctl_enable),
		 "/proc/sys/" PKTLOG_PROC_DIR "/%s/enable", ad_name);
        printf("sysctl_enable: %s\n", sysctl_options);
        pktlog_options(sysctl_options, 0);
        pktlog_enable(sysctl_enable, 0);
        printf("Called _pktlog_enable with parameter %d\n", (int) 0);
        return 0;
    }

    if (sack_thr > 0) {
        if (options & ATH_PKTLOG_PROTO) {
            if (pktlog_size(sysctl_sack_thr, sysctl_enable, sack_thr) != 0) {
                fprintf(stderr, "pktlogconf: log sack_thr setting failed\n");
                exit(-1);
            }
            if (pktlog_size(sysctl_tail_length, sysctl_enable, tail_length) != 0) {
                fprintf(stderr, "pktlogconf: log tail_length setting failed\n");
                exit(-1);
            }
        } else {
            usage();
            exit(-1);
        }
    }

    if (thruput_thresh > 0) {
        if (pktlog_size(sysctl_thruput_thresh, sysctl_enable, thruput_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log thruput_thresh setting failed\n");
            exit(-1);
        }
    }
    if (per_thresh > 0) {
        if (pktlog_size(sysctl_per_thresh, sysctl_enable, per_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log per_thresh setting failed\n");
            exit(-1);
        }
    }
    if (phyerr_thresh > 0) {
        if (pktlog_size(sysctl_phyerr_thresh, sysctl_enable, phyerr_thresh) != 0) {
            fprintf(stderr, "pktlogconf: log phyerr_thresh setting failed\n");
            exit(-1);
        }
    }
    if (trigger_interval > 0) {
        if (pktlog_size(sysctl_trigger_interval, sysctl_enable, trigger_interval) != 0) {
            fprintf(stderr, "pktlogconf: log trigger_interval setting failed\n");
            exit(-1);
        }
    }

    if (fflag) {
        if (strstr(fstr, "rx"))
            filter |= ATH_PKTLOG_RX;
        if (strstr(fstr, "tx"))
            filter |= ATH_PKTLOG_TX;
        if (strstr(fstr, "rcf"))
            filter |= ATH_PKTLOG_RCFIND;
        if (strstr(fstr, "rcu"))
            filter |= ATH_PKTLOG_RCUPDATE;
        if (strstr(fstr, "ani"))
            filter |= ATH_PKTLOG_ANI;
        if (strstr(fstr, "text"))
            filter |= ATH_PKTLOG_TEXT;

        printf("_pktlog_filter:%lu\n", filter);
        if (filter == 0)
            usage();
    } else {
        filter = ATH_PKTLOG_ANI | ATH_PKTLOG_RCUPDATE | ATH_PKTLOG_RCFIND |
            ATH_PKTLOG_RX | ATH_PKTLOG_TX | ATH_PKTLOG_TEXT;
        printf("_pktlog_filter:%lu\n", filter);
    }

    if (size >= 0)
        if (pktlog_size(sysctl_size, sysctl_enable, size) != 0) {
            fprintf(stderr, "pktlogconf: log size setting failed\n");
            exit(-1);
        }

    if (opt_e) {
        if (pktlog_enable(sysctl_enable, filter) != 0) {
            fprintf(stderr, "pktlogconf: log filter setting failed\n");
            exit(-1);
        }
        if (pktlog_options(sysctl_options, options) != 0) {
            fprintf(stderr, "pktlogconf: options setting failed\n");
            exit(-1);
        }
    }

    return 0;
}
