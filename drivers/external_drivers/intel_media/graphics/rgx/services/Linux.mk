########################################################################### ###
#@Title         Services tools and tests root make file
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# This make file defines convenient phony targets to allow Services scripts
# and developers to build all the targets belonging to Services in the DDK.
# When used they help prevent build regressions in tool and test modules not 
# in the main build by default.

modules := services_tools services_tests services_all


### ###########################################################################

services_tools_type := custom

.PHONY: services_tools

# rogueobjanal requires zlib.h found in the zlib1g-dev package

services_tools: rogueobjanal \
                hwperfbin2json \
                hwperfbin2jsont \
                pvrdebug \
                rgx_debug_info \
                pvrhwperf \
                tlioctl_cmd



### ###########################################################################

services_tests_type := custom

.PHONY: services_tests                              

# FYI Tests known to be built by default are:
#   rgx_blitsize_test
#   rgx_blit_test
#   rgx_compute_test
#   rgx_triangle_test
#   rgx_triangle_test_usc
#   rgx_twiddling_test
#   services_test   

# Transport Layer tests require PVR_TRANSPORTLAYER_TESTING to be defined in the 
# build shell when building the driver so that the extra test mechanism is 
# present. Currently this condition is not linked to DEBUG only builds so that
# the TL tests can be run in either DEBUG or RELEASE builds.

# Manual build test modules that need a certain custom build configuration to 
# build and execute.

services_tests_custom_build: devmem_pmmif_test


# Manual test modules that need a custom test environment or manual interaction
# during test execution.

services_tests_manual:  tlioctl_cmd


# Automated test modules that can be built and run in the standard 
# environment.

services_tests_auto:	tlintern_test \
						tlstream_test \
						tlclient_test \
						rgx_hwperf_test \
						services_apiperf_test \
						ri_test

# All 

SERVICES_TESTS_OTHER = rgx_blitsize_test \
                rgx_blit_test \
                rgx_compute_test \
                rgx_triangle_test \
                rgx_twiddling_test \
                services_test \
                \
                memory_test \
                sync_test \
                \
                axi_ace_coherent_test \
                breakpoint_test \
                rgx_ispscandir_test \
                rgx_smppos_test \
                rgx_tqplayer_test \
                services_mutex_test \
                tiling_test 
                
ifeq ($(PDUMP),1)
SERVICES_TESTS_OTHER += rgx2d_fbctest \
                rgx2d_fillrate \
                rgx2d_rtime \
                rgx2d_synctest \
                rgx2d_testsuite \
                rgx2d_tlatest \
                rgx2d_unittest 
endif

services_tests: services_tests_manual \
                services_tests_auto \
                $(SERVICES_TESTS_OTHER)
                

# Broken tests:
#               dev_mem_distr_test 
#               dev_mem_mgt_test 
#               cache_test 
#               ion_test 
#               rgx_wddmtq_test 
#               sparse_alloc_test 


### ###########################################################################

services_all_type := custom

.PHONY: services_all

services_all:	build \
				services_tools \
				services_tests



### ###########################################################################
