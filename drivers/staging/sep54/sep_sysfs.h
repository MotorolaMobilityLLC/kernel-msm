/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/

#ifndef _SEP_SYSFS_H_
#define _SEP_SYSFS_H_

int sep_setup_sysfs(struct kobject *sys_dev_kobj, struct sep_drvdata *drvdata);
void sep_free_sysfs(void);

void sysfs_update_drv_stats(unsigned int qid, unsigned int ioctl_cmd_type,
			    unsigned long long start_ns,
			    unsigned long long end_ns);

void sysfs_update_sep_stats(unsigned int qid, enum sep_sw_desc_type desc_type,
			    unsigned long long start_ns,
			    unsigned long long end_ns);

#ifdef SEP_HWK_UNIT_TEST
ssize_t sys_hwk_st_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf);
ssize_t sys_hwk_st_start(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count);
#endif


#endif /*_SEP_SYSFS_H_*/
