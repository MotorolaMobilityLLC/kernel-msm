#!/bin/bash
#
# Linux 2.6.32 and later Kernel module for VMware MVP Hypervisor Support
#
# Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as published by
# the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; see the file COPYING.  If not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# @brief Script providing the variant of the low memory killer implementation
#        to assist in mvpkm's export of the other_file calculation.

if [ -z "$1" ]
then
   echo "Usage: $0 <path to lowmemorykiller.c>"
   exit 1
fi

# We look at the relevant section of the lowmem_shrink function here. This
# pattern is sufficient to distinguish between the known variants without
# introducing too many false positives for new variants. I.e. we can spot the
# lines that matter for the other_file calculation. In some cases the
# lowmemorykiller uses only the other_file calculation instead of max(free,
# file) - in the cases we've seen this is OK with the balloon policy, since the
# free term isn't really significant when we get into low memory states anyway.

tmp_file="lmk_md5sum_$RANDOM"

cat $1 | tr -d '\ \t\n\r' > $tmp_file
sed -i -e 's/.*\(intother_file.*other_file<\).*/;\1/' \
       -e 's/[;][^;]*other_file[^;]*/#<#&#>#/g' \
       -e 's/#>#[^#]*//g' $tmp_file

MD5=`md5sum $tmp_file | cut -f1 -d\ `

rm $tmp_file

case $MD5 in
4af66fafb5e4cbd7b4092e29e071f152|\
a0f18472eb53e52b38d6f85d4ec66842|\
590b89af56f57146edffceba60845ad8|\
fddbb73a58e82ba1966fd862a561c2bd)
   #/*
   # * This is the same as the non-exported global_reclaimable_pages() when there
   # * is no swap.
   # */
   #other_file = global_page_state(NR_ACTIVE_FILE) +
   #   global_page_state(NR_INACTIVE_FILE);
   V=1
;;
943372c447dd868845d71781292eae17|\
14d0cc4189c1f4fd7818c3393cc8c311)
   # other_file = global_page_state(NR_FILE_PAGES);
   V=2
;;
59f3bb678a855acfea2365b7a904bc5b|\
c88eb5649a468e3f991972661ca7f906|\
b211b3542c24205b203cf6c103951733|\
4798a9bc2c04e225cbec2d2107e92f48|\
c1f73bb2771660d6e7e85f774d030a00|\
df96cbb1784869ac7d017dd343e4e8f2|\
6fdbe3eda927899d0ab567d5b70d4c35|\
07e8a520c0216b91222e4172a4e39213)
   # other_file = global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM);
   V=3
;;
ed03b69361c2881ed1a031c9b9a24d8a|\
8639aca416d3014d68548d6cb538405b)
   # other_file = global_page_state(NR_FREE_PAGES) + global_page_state(NR_FILE_PAGES);
   # (other_free not used, but max(other_free, other_file) = other_file in this
   # case.
   V=4
;;
00865dfbbc8d308d05ed017c9892b538|\
3e766830a787148a5540772630a5335f|\
85354de74c4a30ffe07d60b324be893b|\
97b9a5d6ac4bea28fea79f2723a78764)
   # other_file = global_page_state(NR_FILE_PAGES) - global_page_state(NR_SHMEM);
   # other_free and other_file are modified depending on zone index or/and
   # memory offlining and compared to "lowmem_minfree[i] - zone_adj".
   V=5
;;
*)
   V=0
;;
esac

echo "$MD5 $V"
