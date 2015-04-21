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
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <limits.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <linux/prctl.h>
#include <pwd.h>
#ifdef ANDROID
#include <private/android_filesystem_config.h>
#endif
#include <sys/socket.h>
#include <linux/netlink.h>
#include "event.h"
#include "msg.h"
#include "log.h"

#include "diag_lsm.h"
#include "diagpkt.h"
#include "diagcmd.h"
#include "diag.h"
#include "cld-diag-parser.h"

#ifdef ANDROID
/* CAPs needed
 * CAP_NET_RAW   : Use RAW and packet socket
 * CAP_NET_ADMIN : NL broadcast receive
 */
const uint32_t capabilities = (1 << CAP_NET_RAW) | (1 << CAP_NET_ADMIN);

/* Groups needed
 * AID_INET      : Open INET socket
 * AID_NET_ADMIN : Handle NL socket
 * AID__DIAG : Access DIAG debugfs
 * AID_WIFI      : WIFI Operation
 */
#ifdef QCOM_BSP
const gid_t groups[] = {AID_INET, AID_NET_ADMIN, AID_QCOM_DIAG, AID_WIFI};
#else
const gid_t groups[] = {AID_INET, AID_NET_ADMIN, AID_DIAG, AID_WIFI};
#endif
#endif
const char options[] =
"Options:\n\
-f, --logfile=<Output log file> [Mandotory]\n\
-r, --reclimit=<Maximum number of records before the log rolls over> [Optional]\n\
-c, --console (prints the logs in the console)\n\
-s, --silent (No print will come when logging)\n\
-q, --qxdm  (prints the logs in the qxdm)\n\
-d, --debug  (more prints in logcat, check logcat -s ROME_DEBUG, example to use: -q -d or -c -d)\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
static int sock_fd = -1;
struct msghdr msg;

static FILE *fwlog_res;
static FILE *log_out;
const char *fwlog_res_file;
int32_t max_records;
int32_t record = 0;
const char *progname;
char dbglogoutfile[PATH_MAX];
int32_t optionflag = 0;
boolean isDriverLoaded = FALSE;
const char driverLoaded[] = "KNLREADY";
const char driverUnLoaded[] = "KNLCLOSE";

int32_t rec_limit = 100000000; /* Million records is a good default */

static void
usage(void)
{
    fprintf(stderr, "Usage:\n%s options\n", progname);
    fprintf(stderr, "%s\n", options);
    exit(-1);
}

static uint32_t get_le32(const uint8_t *pos)
{
    return pos[0] | (pos[1] << 8) | (pos[2] << 16) | (pos[3] << 24);
}

static size_t reorder(FILE *log_in, FILE *log_out)
{
    uint8_t buf[RECLEN];
    size_t res;
    uint32_t timestamp = 0, min_timestamp = -1;
    int32_t pos = 0, min_pos = 0;
    struct dbglog_slot *slot;
    uint32_t length = 0;

    pos = 0;
    while ((res = fread(buf, RECLEN, 1, log_in)) == 1) {
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((uint8_t *)&slot->timestamp);
        length = get_le32((uint8_t *)&slot->length);
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
        timestamp = get_le32((uint8_t *)&slot->timestamp);
        length = get_le32((uint8_t *)&slot->length);
        printf("Read record timestamp=%u length=%u\n",
               timestamp, length);
        if (fwrite(buf, RECLEN, res, log_out) != res)
               perror("fwrite");
    }

    fseek(log_in, 0, SEEK_SET);
    pos = min_pos;
    while (pos > 0 && (res = fread(buf, RECLEN, 1, log_out)) == 1) {
        slot = (struct dbglog_slot *)buf;
        timestamp = get_le32((uint8_t *)&slot->timestamp);
        length = get_le32((uint8_t *)&slot->length);
        pos--;
        printf("Read record timestamp=%u length=%u\n",
                timestamp, length);
        if (fwrite(buf, RECLEN, res, log_out) != res)
                perror("fwrite");
    }

    return 0;
}

#ifdef ANDROID
/*
 * Lower the privileges for security reason
 * the service will run only in system or diag mode
 *
 */
int32_t
cnssdiagservice_cap_handle(void)
{
    int32_t i;
    int32_t err;

    struct __user_cap_header_struct cap_header_data;
    cap_user_header_t cap_header = &cap_header_data;
    struct __user_cap_data_struct cap_data_data;
    cap_user_data_t cap_data = &cap_data_data;

    cap_header->pid = 0;
    cap_header->version = _LINUX_CAPABILITY_VERSION;
    memset(cap_data, 0, sizeof(cap_data_data));

    if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) != 0)
    {
        printf("%d PR_SET_KEEPCAPS error:%s", __LINE__, strerror(errno));
        exit(1);
    }

    if (setgroups(sizeof(groups)/sizeof(groups[0]), groups) != 0)
    {
        printf("setgroups error %s", strerror(errno));
        return -1;
    }

    if (setgid(AID_SYSTEM))
    {
        printf("SET GID error %s", strerror(errno));
        return -1;
    }

    if (setuid(AID_SYSTEM))
    {
        printf("SET UID %s", strerror(errno));
        return -1;
    }

    /* Assign correct CAP */
    cap_data->effective = capabilities;
    cap_data->permitted = capabilities;
    /* Set the capabilities */
    if (capset(cap_header, cap_data) < 0)
    {
        printf("%d failed capset error:%s", __LINE__, strerror(errno));
        return -1;
    }
    return 0;
}
#endif

static void cleanup(void) {
    if (sock_fd)
        close(sock_fd);

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

static void stop(int32_t signum)
{

    if(optionflag & LOGFILE_FLAG){
        printf("Recording stopped\n");
        cleanup();
    }
    exit(0);
}


void process_cnss_log_file(uint8_t *dbgbuf)
{
    uint16_t length = 0;
    uint32_t dropped = 0;
    uint32_t timestamp = 0;
    uint32_t res =0;
    struct dbglog_slot *slot = (struct dbglog_slot *)dbgbuf;
    fseek(log_out, record * RECLEN, SEEK_SET);
    record++;
    timestamp = get_le32((uint8_t *)&slot->timestamp);
    length = get_le32((uint8_t *)&slot->length);
    dropped = get_le32((uint8_t *)&slot->dropped);
    if (!((optionflag & SILENT_FLAG) == SILENT_FLAG)) {
        /* don't like this have to fix it */
        printf("Read record %d timestamp=%u length=%u fw dropped=%u\n",
              record, timestamp, length, dropped);
    }
    if ((res = fwrite(dbgbuf, RECLEN, 1, log_out)) != 1){
        perror("fwrite");
        return;
    }
    fflush(log_out);
    if (record == max_records)
        record = 0;
}
/*
 * Process FW debug, FW event and FW log messages
 * Read the payload and process accordingly.
 *
 */
void process_cnss_diag_msg(tAniNlHdr *wnl)
{
    uint8_t *dbgbuf;
    uint8_t *eventbuf = (uint8_t *)NLMSG_DATA(wnl);
    uint16_t diag_type = 0;
    uint32_t event_id = 0;
    uint16_t length = 0;
    struct dbglog_slot *slot;
    uint32_t dropped = 0;

    dbgbuf = eventbuf;

    diag_type = *(uint16_t *)eventbuf;
    eventbuf += sizeof(uint16_t);

    length = *(uint16_t *)eventbuf;
    eventbuf += sizeof(uint16_t);

    if (wnl->nlh.nlmsg_type == WLAN_NL_MSG_CNSS_HOST_MSG
        && (wnl->wmsg.type == ANI_NL_MSG_LOG_HOST_MSG_TYPE)) {
          process_cnss_host_message(wnl, optionflag, log_out, &record, max_records);
    } else if (wnl->nlh.nlmsg_type == WLAN_NL_MSG_CNSS_HOST_EVENT_LOG
        && (wnl->wmsg.type == ANI_NL_MSG_LOG_HOST_EVENT_LOG_TYPE)) {
        process_cnss_host_diag_events_log((char *)((char *)&wnl->wmsg.length
        + sizeof(wnl->wmsg.length)), optionflag);
    } else {
        if (diag_type == DIAG_TYPE_FW_EVENT) {
            eventbuf += sizeof(uint32_t);
            event_id = *(uint32_t *)eventbuf;
            eventbuf += sizeof(uint32_t);
            if (optionflag & QXDM_FLAG) {
                if (length)
                    event_report_payload(event_id, length, eventbuf);
                else
                    event_report(event_id);
            }
        } else if (diag_type == DIAG_TYPE_FW_LOG) {
           /* Do nothing for now */
        } else if (diag_type == DIAG_TYPE_FW_DEBUG_MSG) {
            slot =(struct dbglog_slot *)dbgbuf;
            length = get_le32((uint8_t *)&slot->length);
            dropped = get_le32((uint8_t *)&slot->dropped);
            if (optionflag & LOGFILE_FLAG)
                process_cnss_log_file(dbgbuf);
            else if (optionflag & (CONSOLE_FLAG | QXDM_FLAG))
                dbglog_parse_debug_logs(&slot->payload[0], length, dropped);
        } else if (diag_type == DIAG_TYPE_FW_MSG) {
            uint32_t version = 0;
            slot = (struct dbglog_slot *)dbgbuf;
            length = get_32((uint8_t *)&slot->length);
            version = get_le32((uint8_t *)&slot->dropped);
            process_diagfw_msg(&slot->payload[0], length, optionflag, log_out,
                               &record, max_records, version, sock_fd);
        } else if (diag_type == DIAG_TYPE_HOST_MSG) {
            slot = (struct dbglog_slot *)dbgbuf;
            length = get_32((uint8_t *)&slot->length);
            process_diaghost_msg(slot->payload, length);
        } else {
           /* Do nothing for now */
        }
    }

}

/*
 * Open the socket and bind the socket with src
 * address. Return the socket fd if sucess.
 *
 */
static int32_t create_nl_socket()
{
    int32_t ret;
    int32_t sock_fd;

    sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if (sock_fd < 0) {
        fprintf(stderr, "Socket creation failed sock_fd 0x%x \n", sock_fd);
        return -1;
    }

    memset(&src_addr, 0, sizeof(src_addr));
    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_groups = 0x01;
    src_addr.nl_pid = getpid(); /* self pid */

    ret = bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));
    if (ret < 0)
    {
        close(sock_fd);
        return ret;
    }
    return sock_fd;
}

static uint32_t initialize(int32_t sock_fd)
{
    char *mesg = "Hello";

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */

    if (nlh) {
        free(nlh);
        nlh = NULL;
    }
    nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_MSG_SIZE));
    if (nlh == NULL) {
        fprintf(stderr, "Cannot allocate memory \n");
        close(sock_fd);
        return -1;
    }
    memset(nlh, 0, NLMSG_SPACE(MAX_MSG_SIZE));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_MSG_SIZE);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_type = WLAN_NL_MSG_CNSS_DIAG;
    nlh->nlmsg_flags = NLM_F_REQUEST;

    memcpy(NLMSG_DATA(nlh), mesg, strlen(mesg));

    iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    sendmsg(sock_fd, &msg, 0);
    return 0;
}

int32_t main(int32_t argc, char *argv[])
{
    uint32_t res =0;
    uint8_t *eventbuf = NULL;
    uint8_t *dbgbuf = NULL;
    int32_t c;
    struct dbglog_slot *slot;

    progname = argv[0];
    uint16_t diag_type = 0;
    int32_t option_index = 0;
    static struct option long_options[] = {
        {"logfile", 1, NULL, 'f'},
        {"reclimit", 1, NULL, 'r'},
        {"console", 0, NULL, 'c'},
        {"qxdm", 0, NULL, 'q'},
        {"silent", 0, NULL, 's'},
        {"debug", 0, NULL, 'd'},
        { 0, 0, 0, 0}
    };

    while (1) {
        c = getopt_long (argc, argv, "f:scqdr:", long_options, &option_index);
        if (c == -1) break;

        switch (c) {
            case 'f':
                memset(dbglogoutfile, 0, PATH_MAX);
                memcpy(dbglogoutfile, optarg, strlen(optarg));
                optionflag |= LOGFILE_FLAG;
            break;

            case 'c':
                optionflag |= CONSOLE_FLAG;
            break;

            case 'q':
                optionflag |= QXDM_FLAG;
            break;

            case 'r':
                rec_limit = strtoul(optarg, NULL, 0);
            break;

            case 's':
                optionflag |= SILENT_FLAG;
            break;

            case 'd':
                optionflag |= DEBUG_FLAG;
            break;
            default:
                usage();
        }
    }

    if (!(optionflag & (LOGFILE_FLAG | CONSOLE_FLAG | QXDM_FLAG | SILENT_FLAG
           | DEBUG_FLAG))) {
        usage();
        return -1;
    }

    if (optionflag & QXDM_FLAG) {
        /* Intialize the fd required for diag APIs */
        if (TRUE != Diag_LSM_Init(NULL))
        {
             perror("Failed on Diag_LSM_Init\n");
             return -1;
        }
         /* Register CALLABACK for QXDM input data */
        DIAGPKT_DISPATCH_TABLE_REGISTER(DIAG_SUBSYS_WLAN, cnss_wlan_tbl);
#ifdef ANDROID
        if(cnssdiagservice_cap_handle()) {
            printf("Cap bouncing failed EXIT!!!");
            exit(1);
        }
#endif
    }

    sock_fd = create_nl_socket();
    if (sock_fd < 0) {
        fprintf(stderr, "Socket creation failed sock_fd 0x%x \n", sock_fd);
        return -1;
    }

    initialize(sock_fd);
    cnssdiag_register_kernel_logging(sock_fd, nlh);

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    if (optionflag & LOGFILE_FLAG) {

        if (rec_limit < RECLEN) {
            fprintf(stderr, "Too small maximum length (has to be >= %d)\n",
                    RECLEN);
            close(sock_fd);
            free(nlh);
            return -1;
        }
        max_records = rec_limit / RECLEN;
        printf("Storing last %d records\n", max_records);

        log_out = fopen(dbglogoutfile, "w");
        if (log_out == NULL) {
            perror("Failed to create output file");
            close(sock_fd);
            free(nlh);
            return -1;
        }

        fwlog_res_file = "./reorder";
    }


    parser_init();

    while ((res = recvmsg(sock_fd, &msg, 0)) > 0)  {
       if ((isDriverLoaded == FALSE) &&
            (res == SIZEOF_NL_MSG_LOAD)) {
           eventbuf = (uint8_t *)NLMSG_DATA(nlh);
           if (0 == strncmp(driverLoaded, (const char *)eventbuf,
                            strlen(driverLoaded))) {
               isDriverLoaded = TRUE;
               close(sock_fd);
               /* Wait for driver to Load */
               sleep(10);
               sock_fd = create_nl_socket();
               if (sock_fd < 0) {
                   printf("create nl sock failed ret %d \n", sock_fd);
                   return -1;
              }
              initialize(sock_fd);
              diag_initialize(isDriverLoaded, sock_fd, optionflag);
              cnssdiag_register_kernel_logging(sock_fd, nlh);
           }
       } else if ((isDriverLoaded == TRUE) &&
                   (res == SIZEOF_NL_MSG_UNLOAD)) {
           eventbuf = (uint8_t *)NLMSG_DATA(nlh);
           if (0 == strncmp(driverUnLoaded, (const char *)eventbuf,
                            strlen(driverUnLoaded))) {
               isDriverLoaded = FALSE;
               diag_initialize(isDriverLoaded, sock_fd, optionflag);
           }
       } else if (((res >= sizeof(struct dbglog_slot)) &&
                 (res != SIZEOF_NL_MSG_LOAD) &&
                 (res != SIZEOF_NL_MSG_UNLOAD)) ||
                 (nlh->nlmsg_type == WLAN_NL_MSG_CNSS_HOST_EVENT_LOG)) {
           isDriverLoaded = TRUE;
           process_cnss_diag_msg((tAniNlHdr *)nlh);
           memset(nlh,0,NLMSG_SPACE(MAX_MSG_SIZE));
       } else {
           /* Ignore other messages that might be broadcast */
           continue;
       }
    }
    /* Release the handle to Diag*/
    Diag_LSM_DeInit();
    if (optionflag & LOGFILE_FLAG)
        cleanup();
    close(sock_fd);
    free(nlh);
    return 0;
}
