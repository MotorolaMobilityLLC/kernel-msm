/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9897_I2CVERSION 34

typedef enum nxpTfa1BfEnumList {
    TFA1_BF_VDDS  = 0x0000,    /*!< Power-on-reset flag                                */
    TFA1_BF_PLLS  = 0x0010,    /*!< PLL lock                                           */
    TFA1_BF_OTDS  = 0x0020,    /*!< Over Temperature Protection alarm                  */
    TFA1_BF_OVDS  = 0x0030,    /*!< Over Voltage Protection alarm                      */
    TFA1_BF_UVDS  = 0x0040,    /*!< Under Voltage Protection alarm                     */
    TFA1_BF_OCDS  = 0x0050,    /*!< Over Current Protection alarm                      */
    TFA1_BF_CLKS  = 0x0060,    /*!< Clocks stable flag                                 */
    TFA1_BF_CLIPS = 0x0070,    /*!< Amplifier clipping                                 */
    TFA1_BF_MTPB  = 0x0080,    /*!< MTP busy                                           */
    TFA1_BF_NOCLK = 0x0090,    /*!< Flag lost clock from clock generation unit         */
    TFA1_BF_SPKS  = 0x00a0,    /*!< Speaker error flag                                 */
    TFA1_BF_ACS   = 0x00b0,    /*!< Cold Start flag                                    */
    TFA1_BF_SWS   = 0x00c0,    /*!< Flag Engage                                        */
    TFA1_BF_WDS   = 0x00d0,    /*!< Flag watchdog reset                                */
    TFA1_BF_AMPS  = 0x00e0,    /*!< Amplifier is enabled by manager                    */
    TFA1_BF_AREFS = 0x00f0,    /*!< References are enabled by manager                  */
    TFA1_BF_BATS  = 0x0109,    /*!< Battery voltage readout; 0 .. 5.5 [V]              */
    TFA1_BF_TEMPS = 0x0208,    /*!< Temperature readout from the temperature sensor    */
    TFA1_BF_REV   = 0x030b,    /*!< Device type number is B97                          */
    TFA1_BF_RCV   = 0x0420,    /*!< Enable Receiver Mode                               */
    TFA1_BF_CHS12 = 0x0431,    /*!< Channel Selection TDM input for Coolflux           */
    TFA1_BF_INPLVL= 0x0450,    /*!< Input level selection control                      */
    TFA1_BF_CHSA  = 0x0461,    /*!< Input selection for amplifier                      */
    TFA1_BF_I2SDOE= 0x04b0,    /*!< Enable data output                                 */
    TFA1_BF_AUDFS = 0x04c3,    /*!< Audio sample rate setting                          */
    TFA1_BF_BSSCR = 0x0501,    /*!< Protection Attack Time                             */
    TFA1_BF_BSST  = 0x0523,    /*!< ProtectionThreshold                                */
    TFA1_BF_BSSRL = 0x0561,    /*!< Protection Maximum Reduction                       */
    TFA1_BF_BSSRR = 0x0582,    /*!< Battery Protection Release Time                    */
    TFA1_BF_BSSHY = 0x05b1,    /*!< Battery Protection Hysteresis                      */
    TFA1_BF_BSSR  = 0x05e0,    /*!< battery voltage for I2C read out only              */
    TFA1_BF_BSSBY = 0x05f0,    /*!< bypass clipper battery protection                  */
    TFA1_BF_DPSA  = 0x0600,    /*!< Enable dynamic powerstage activation               */
    TFA1_BF_CFSM  = 0x0650,    /*!< Soft mute in CoolFlux                              */
    TFA1_BF_BSSS  = 0x0670,    /*!< BatSenseSteepness                                  */
    TFA1_BF_VOL   = 0x0687,    /*!< volume control (in CoolFlux)                       */
    TFA1_BF_DCVO  = 0x0702,    /*!< Boost Voltage                                      */
    TFA1_BF_DCMCC = 0x0733,    /*!< Max boost coil current - step of 175 mA            */
    TFA1_BF_DCIE  = 0x07a0,    /*!< Adaptive boost mode                                */
    TFA1_BF_DCSR  = 0x07b0,    /*!< Soft RampUp/Down mode for DCDC controller          */
    TFA1_BF_DCPAVG= 0x07c0,    /*!< ctrl_peak2avg for analog part of DCDC              */
    TFA1_BF_TROS  = 0x0800,    /*!< Select external temperature also the ext_temp will be put on the temp read out  */
    TFA1_BF_EXTTS = 0x0818,    /*!< external temperature setting to be given by host   */
    TFA1_BF_PWDN  = 0x0900,    /*!< Device Mode                                        */
    TFA1_BF_I2CR  = 0x0910,    /*!< I2C Reset                                          */
    TFA1_BF_CFE   = 0x0920,    /*!< Enable CoolFlux                                    */
    TFA1_BF_AMPE  = 0x0930,    /*!< Enable Amplifier                                   */
    TFA1_BF_DCA   = 0x0940,    /*!< EnableBoost                                        */
    TFA1_BF_SBSL  = 0x0950,    /*!< Coolflux configured                                */
    TFA1_BF_AMPC  = 0x0960,    /*!< Selection on how Amplifier is enabled              */
    TFA1_BF_DCDIS = 0x0970,    /*!< DCDC not connected                                 */
    TFA1_BF_PSDR  = 0x0980,    /*!< IDDQ test amplifier                                */
    TFA1_BF_DCCV  = 0x0991,    /*!< Coil Value                                         */
    TFA1_BF_CCFD  = 0x09b0,    /*!< Selection CoolFlux Clock                           */
    TFA1_BF_INTPAD= 0x09c1,    /*!< INT pad configuration control                      */
    TFA1_BF_IPLL  = 0x09e0,    /*!< PLL input reference clock selection                */
    TFA1_BF_MTPK  = 0x0b07,    /*!< 5Ah, 90d To access KEY1_Protected registers (Default for engineering) */
    TFA1_BF_CVFDLY= 0x0c25,    /*!< Fractional delay adjustment between current and voltage sense */
    TFA1_BF_TDMPRF= 0x1011,    /*!< TDM_usecase                                        */
    TFA1_BF_TDMEN = 0x1030,    /*!< TDM interface control                              */
    TFA1_BF_TDMCKINV= 0x1040,    /*!< TDM clock inversion                                */
    TFA1_BF_TDMFSLN= 0x1053,    /*!< TDM FS length                                      */
    TFA1_BF_TDMFSPOL= 0x1090,    /*!< TDM FS polarity                                    */
    TFA1_BF_TDMSAMSZ= 0x10a4,    /*!< TDM Sample Size for all tdm sinks/sources          */
    TFA1_BF_TDMSLOTS= 0x1103,    /*!< Number of slots                                    */
    TFA1_BF_TDMSLLN= 0x1144,    /*!< Slot length                                        */
    TFA1_BF_TDMBRMG= 0x1194,    /*!< Bits remaining                                     */
    TFA1_BF_TDMDDEL= 0x11e0,    /*!< Data delay                                         */
    TFA1_BF_TDMDADJ= 0x11f0,    /*!< Data adjustment                                    */
    TFA1_BF_TDMTXFRM= 0x1201,    /*!< TXDATA format                                      */
    TFA1_BF_TDMUUS0= 0x1221,    /*!< TXDATA format unused slot sd0                      */
    TFA1_BF_TDMUUS1= 0x1241,    /*!< TXDATA format unused slot sd1                      */
    TFA1_BF_TDMSI0EN= 0x1270,    /*!< TDM sink0 enable                                   */
    TFA1_BF_TDMSI1EN= 0x1280,    /*!< TDM sink1 enable                                   */
    TFA1_BF_TDMSI2EN= 0x1290,    /*!< TDM sink2 enable                                   */
    TFA1_BF_TDMSO0EN= 0x12a0,    /*!< TDM source0 enable                                 */
    TFA1_BF_TDMSO1EN= 0x12b0,    /*!< TDM source1 enable                                 */
    TFA1_BF_TDMSO2EN= 0x12c0,    /*!< TDM source2 enable                                 */
    TFA1_BF_TDMSI0IO= 0x12d0,    /*!< tdm_sink0_io                                       */
    TFA1_BF_TDMSI1IO= 0x12e0,    /*!< tdm_sink1_io                                       */
    TFA1_BF_TDMSI2IO= 0x12f0,    /*!< tdm_sink2_io                                       */
    TFA1_BF_TDMSO0IO= 0x1300,    /*!< tdm_source0_io                                     */
    TFA1_BF_TDMSO1IO= 0x1310,    /*!< tdm_source1_io                                     */
    TFA1_BF_TDMSO2IO= 0x1320,    /*!< tdm_source2_io                                     */
    TFA1_BF_TDMSI0SL= 0x1333,    /*!< sink0_slot [GAIN IN]                               */
    TFA1_BF_TDMSI1SL= 0x1373,    /*!< sink1_slot [CH1 IN]                                */
    TFA1_BF_TDMSI2SL= 0x13b3,    /*!< sink2_slot [CH2 IN]                                */
    TFA1_BF_TDMSO0SL= 0x1403,    /*!< source0_slot [GAIN OUT]                            */
    TFA1_BF_TDMSO1SL= 0x1443,    /*!< source1_slot [Voltage Sense]                       */
    TFA1_BF_TDMSO2SL= 0x1483,    /*!< source2_slot [Current Sense]                       */
    TFA1_BF_NBCK  = 0x14c3,    /*!< NBCK                                               */
    TFA1_BF_INTOVDDS= 0x2000,    /*!< flag_por_int_out                                   */
    TFA1_BF_INTOPLLS= 0x2010,    /*!< flag_pll_lock_int_out                              */
    TFA1_BF_INTOOTDS= 0x2020,    /*!< flag_otpok_int_out                                 */
    TFA1_BF_INTOOVDS= 0x2030,    /*!< flag_ovpok_int_out                                 */
    TFA1_BF_INTOUVDS= 0x2040,    /*!< flag_uvpok_int_out                                 */
    TFA1_BF_INTOOCDS= 0x2050,    /*!< flag_ocp_alarm_int_out                             */
    TFA1_BF_INTOCLKS= 0x2060,    /*!< flag_clocks_stable_int_out                         */
    TFA1_BF_INTOCLIPS= 0x2070,    /*!< flag_clip_int_out                                  */
    TFA1_BF_INTOMTPB= 0x2080,    /*!< mtp_busy_int_out                                   */
    TFA1_BF_INTONOCLK= 0x2090,    /*!< flag_lost_clk_int_out                              */
    TFA1_BF_INTOSPKS= 0x20a0,    /*!< flag_cf_speakererror_int_out                       */
    TFA1_BF_INTOACS= 0x20b0,    /*!< flag_cold_started_int_out                          */
    TFA1_BF_INTOSWS= 0x20c0,    /*!< flag_engage_int_out                                */
    TFA1_BF_INTOWDS= 0x20d0,    /*!< flag_watchdog_reset_int_out                        */
    TFA1_BF_INTOAMPS= 0x20e0,    /*!< flag_enbl_amp_int_out                              */
    TFA1_BF_INTOAREFS= 0x20f0,    /*!< flag_enbl_ref_int_out                              */
    TFA1_BF_INTOACK= 0x2201,    /*!< Interrupt status register output - Corresponding flag */
    TFA1_BF_INTIVDDS= 0x2300,    /*!< flag_por_int_in                                    */
    TFA1_BF_INTIPLLS= 0x2310,    /*!< flag_pll_lock_int_in                               */
    TFA1_BF_INTIOTDS= 0x2320,    /*!< flag_otpok_int_in                                  */
    TFA1_BF_INTIOVDS= 0x2330,    /*!< flag_ovpok_int_in                                  */
    TFA1_BF_INTIUVDS= 0x2340,    /*!< flag_uvpok_int_in                                  */
    TFA1_BF_INTIOCDS= 0x2350,    /*!< flag_ocp_alarm_int_in                              */
    TFA1_BF_INTICLKS= 0x2360,    /*!< flag_clocks_stable_int_in                          */
    TFA1_BF_INTICLIPS= 0x2370,    /*!< flag_clip_int_in                                   */
    TFA1_BF_INTIMTPB= 0x2380,    /*!< mtp_busy_int_in                                    */
    TFA1_BF_INTINOCLK= 0x2390,    /*!< flag_lost_clk_int_in                               */
    TFA1_BF_INTISPKS= 0x23a0,    /*!< flag_cf_speakererror_int_in                        */
    TFA1_BF_INTIACS= 0x23b0,    /*!< flag_cold_started_int_in                           */
    TFA1_BF_INTISWS= 0x23c0,    /*!< flag_engage_int_in                                 */
    TFA1_BF_INTIWDS= 0x23d0,    /*!< flag_watchdog_reset_int_in                         */
    TFA1_BF_INTIAMPS= 0x23e0,    /*!< flag_enbl_amp_int_in                               */
    TFA1_BF_INTIAREFS= 0x23f0,    /*!< flag_enbl_ref_int_in                               */
    TFA1_BF_INTIACK= 0x2501,    /*!< Interrupt register input                           */
    TFA1_BF_INTENVDDS= 0x2600,    /*!< flag_por_int_enable                                */
    TFA1_BF_INTENPLLS= 0x2610,    /*!< flag_pll_lock_int_enable                           */
    TFA1_BF_INTENOTDS= 0x2620,    /*!< flag_otpok_int_enable                              */
    TFA1_BF_INTENOVDS= 0x2630,    /*!< flag_ovpok_int_enable                              */
    TFA1_BF_INTENUVDS= 0x2640,    /*!< flag_uvpok_int_enable                              */
    TFA1_BF_INTENOCDS= 0x2650,    /*!< flag_ocp_alarm_int_enable                          */
    TFA1_BF_INTENCLKS= 0x2660,    /*!< flag_clocks_stable_int_enable                      */
    TFA1_BF_INTENCLIPS= 0x2670,    /*!< flag_clip_int_enable                               */
    TFA1_BF_INTENMTPB= 0x2680,    /*!< mtp_busy_int_enable                                */
    TFA1_BF_INTENNOCLK= 0x2690,    /*!< flag_lost_clk_int_enable                           */
    TFA1_BF_INTENSPKS= 0x26a0,    /*!< flag_cf_speakererror_int_enable                    */
    TFA1_BF_INTENACS= 0x26b0,    /*!< flag_cold_started_int_enable                       */
    TFA1_BF_INTENSWS= 0x26c0,    /*!< flag_engage_int_enable                             */
    TFA1_BF_INTENWDS= 0x26d0,    /*!< flag_watchdog_reset_int_enable                     */
    TFA1_BF_INTENAMPS= 0x26e0,    /*!< flag_enbl_amp_int_enable                           */
    TFA1_BF_INTENAREFS= 0x26f0,    /*!< flag_enbl_ref_int_enable                           */
    TFA1_BF_INTENACK= 0x2801,    /*!< Interrupt enable register                          */
    TFA1_BF_INTPOLVDDS= 0x2900,    /*!< flag_por_int_pol                                   */
    TFA1_BF_INTPOLPLLS= 0x2910,    /*!< flag_pll_lock_int_pol                              */
    TFA1_BF_INTPOLOTDS= 0x2920,    /*!< flag_otpok_int_pol                                 */
    TFA1_BF_INTPOLOVDS= 0x2930,    /*!< flag_ovpok_int_pol                                 */
    TFA1_BF_INTPOLUVDS= 0x2940,    /*!< flag_uvpok_int_pol                                 */
    TFA1_BF_INTPOLOCDS= 0x2950,    /*!< flag_ocp_alarm_int_pol                             */
    TFA1_BF_INTPOLCLKS= 0x2960,    /*!< flag_clocks_stable_int_pol                         */
    TFA1_BF_INTPOLCLIPS= 0x2970,    /*!< flag_clip_int_pol                                  */
    TFA1_BF_INTPOLMTPB= 0x2980,    /*!< mtp_busy_int_pol                                   */
    TFA1_BF_INTPOLNOCLK= 0x2990,    /*!< flag_lost_clk_int_pol                              */
    TFA1_BF_INTPOLSPKS= 0x29a0,    /*!< flag_cf_speakererror_int_pol                       */
    TFA1_BF_INTPOLACS= 0x29b0,    /*!< flag_cold_started_int_pol                          */
    TFA1_BF_INTPOLSWS= 0x29c0,    /*!< flag_engage_int_pol                                */
    TFA1_BF_INTPOLWDS= 0x29d0,    /*!< flag_watchdog_reset_int_pol                        */
    TFA1_BF_INTPOLAMPS= 0x29e0,    /*!< flag_enbl_amp_int_pol                              */
    TFA1_BF_INTPOLAREFS= 0x29f0,    /*!< flag_enbl_ref_int_pol                              */
    TFA1_BF_INTPOLACK= 0x2b01,    /*!< Interrupt status flags polarity register           */
    TFA1_BF_CLIP  = 0x4900,    /*!< Bypass clip control                                */
    TFA1_BF_CIMTP = 0x62b0,    /*!< start copying all the data from i2cregs_mtp to mtp [Key 2 protected] */
    TFA1_BF_RST   = 0x7000,    /*!< Reset CoolFlux DSP                                 */
    TFA1_BF_DMEM  = 0x7011,    /*!< Target memory for access                           */
    TFA1_BF_AIF   = 0x7030,    /*!< Autoincrement-flag for memory-address              */
    TFA1_BF_CFINT = 0x7040,    /*!< Interrupt CoolFlux DSP                             */
    TFA1_BF_REQ   = 0x7087,    /*!< request for access (8 channels)                    */
    TFA1_BF_REQCMD= 0x7080,    /*!< Firmware event request rpc command                 */
    TFA1_BF_REQRST= 0x7090,    /*!< Firmware event request reset restart               */
    TFA1_BF_REQMIPS= 0x70a0,    /*!< Firmware event request short on mips               */
    TFA1_BF_REQMUTED= 0x70b0,    /*!< Firmware event request mute sequence ready         */
    TFA1_BF_REQVOL= 0x70c0,    /*!< Firmware event request volume ready                */
    TFA1_BF_REQDMG= 0x70d0,    /*!< Firmware event request speaker damage detected     */
    TFA1_BF_REQCAL= 0x70e0,    /*!< Firmware event request calibration completed       */
    TFA1_BF_REQRSV= 0x70f0,    /*!< Firmware event request reserved                    */
    TFA1_BF_MADD  = 0x710f,    /*!< memory-address to be accessed                      */
    TFA1_BF_MEMA  = 0x720f,    /*!< activate memory access (24- or 32-bits data is written/read to/from memory */
    TFA1_BF_ERR   = 0x7307,    /*!< Coolflux error flags                               */
    TFA1_BF_ACK   = 0x7387,    /*!< acknowledge of requests (8 channels)               */
    TFA1_BF_MTPOTC= 0x8000,    /*!< Calibration schedule (key2 protected)              */
    TFA1_BF_MTPEX = 0x8010,    /*!< (key2 protected)                                   */
} nxpTfa1BfEnumList_t;
#define TFA1_NAMETABLE static tfaBfName_t Tfa1DatasheetNames[]= {\
   { 0x0, "VDDS"},    /* Power-on-reset flag                               , */\
   { 0x10, "PLLS"},    /* PLL lock                                          , */\
   { 0x20, "OTDS"},    /* Over Temperature Protection alarm                 , */\
   { 0x30, "OVDS"},    /* Over Voltage Protection alarm                     , */\
   { 0x40, "UVDS"},    /* Under Voltage Protection alarm                    , */\
   { 0x50, "OCDS"},    /* Over Current Protection alarm                     , */\
   { 0x60, "CLKS"},    /* Clocks stable flag                                , */\
   { 0x70, "CLIPS"},    /* Amplifier clipping                                , */\
   { 0x80, "MTPB"},    /* MTP busy                                          , */\
   { 0x90, "NOCLK"},    /* Flag lost clock from clock generation unit        , */\
   { 0xa0, "SPKS"},    /* Speaker error flag                                , */\
   { 0xb0, "ACS"},    /* Cold Start flag                                   , */\
   { 0xc0, "SWS"},    /* Flag Engage                                       , */\
   { 0xd0, "WDS"},    /* Flag watchdog reset                               , */\
   { 0xe0, "AMPS"},    /* Amplifier is enabled by manager                   , */\
   { 0xf0, "AREFS"},    /* References are enabled by manager                 , */\
   { 0x109, "BATS"},    /* Battery voltage readout; 0 .. 5.5 [V]             , */\
   { 0x208, "TEMPS"},    /* Temperature readout from the temperature sensor   , */\
   { 0x30b, "REV"},    /* Device type number is B97                         , */\
   { 0x420, "RCV"},    /* Enable Receiver Mode                              , */\
   { 0x431, "CHS12"},    /* Channel Selection TDM input for Coolflux          , */\
   { 0x450, "INPLVL"},    /* Input level selection control                     , */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "AUDFS"},    /* Audio sample rate setting                         , */\
   { 0x501, "SSCR"},    /* Protection Attack Time                            , */\
   { 0x523, "SST"},    /* ProtectionThreshold                               , */\
   { 0x561, "SSRL"},    /* Protection Maximum Reduction                      , */\
   { 0x582, "SSRR"},    /* Battery Protection Release Time                   , */\
   { 0x5b1, "SSHY"},    /* Battery Protection Hysteresis                     , */\
   { 0x5e0, "SSR"},    /* battery voltage for I2C read out only             , */\
   { 0x5f0, "SSBY"},    /* bypass clipper battery protection                 , */\
   { 0x600, "DPSA"},    /* Enable dynamic powerstage activation              , */\
   { 0x650, "CFSM"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "SSS"},    /* BatSenseSteepness                                 , */\
   { 0x687, "VOL"},    /* volume control (in CoolFlux)                      , */\
   { 0x702, "DCVO"},    /* Boost Voltage                                     , */\
   { 0x733, "DCMCC"},    /* Max boost coil current - step of 175 mA           , */\
   { 0x7a0, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "DCSR"},    /* Soft RampUp/Down mode for DCDC controller         , */\
   { 0x7c0, "DCPAVG"},    /* ctrl_peak2avg for analog part of DCDC             , */\
   { 0x800, "TROS"},    /* Select external temperature also the ext_temp will be put on the temp read out , */\
   { 0x818, "EXTTS"},    /* external temperature setting to be given by host  , */\
   { 0x900, "PWDN"},    /* Device Mode                                       , */\
   { 0x910, "I2CR"},    /* I2C Reset                                         , */\
   { 0x920, "CFE"},    /* Enable CoolFlux                                   , */\
   { 0x930, "AMPE"},    /* Enable Amplifier                                  , */\
   { 0x940, "DCA"},    /* EnableBoost                                       , */\
   { 0x950, "SBSL"},    /* Coolflux configured                               , */\
   { 0x960, "AMPC"},    /* Selection on how Amplifier is enabled             , */\
   { 0x970, "DCDIS"},    /* DCDC not connected                                , */\
   { 0x980, "PSDR"},    /* IDDQ test amplifier                               , */\
   { 0x991, "DCCV"},    /* Coil Value                                        , */\
   { 0x9b0, "CCFD"},    /* Selection CoolFlux Clock                          , */\
   { 0x9c1, "INTPAD"},    /* INT pad configuration control                     , */\
   { 0x9e0, "IPLL"},    /* PLL input reference clock selection               , */\
   { 0xb07, "MTPK"},    /* 5Ah, 90d To access KEY1_Protected registers (Default for engineering), */\
   { 0xc25, "CVFDLY"},    /* Fractional delay adjustment between current and voltage sense, */\
   { 0x1011, "TDMPRF"},    /* TDM_usecase                                       , */\
   { 0x1030, "TDMEN"},    /* TDM interface control                             , */\
   { 0x1040, "TDMCKINV"},    /* TDM clock inversion                               , */\
   { 0x1053, "TDMFSLN"},    /* TDM FS length                                     , */\
   { 0x1090, "TDMFSPOL"},    /* TDM FS polarity                                   , */\
   { 0x10a4, "TDMSAMSZ"},    /* TDM Sample Size for all tdm sinks/sources         , */\
   { 0x1103, "TDMSLOTS"},    /* Number of slots                                   , */\
   { 0x1144, "TDMSLLN"},    /* Slot length                                       , */\
   { 0x1194, "TDMBRMG"},    /* Bits remaining                                    , */\
   { 0x11e0, "TDMDDEL"},    /* Data delay                                        , */\
   { 0x11f0, "TDMDADJ"},    /* Data adjustment                                   , */\
   { 0x1201, "TDMTXFRM"},    /* TXDATA format                                     , */\
   { 0x1221, "TDMUUS0"},    /* TXDATA format unused slot sd0                     , */\
   { 0x1241, "TDMUUS1"},    /* TXDATA format unused slot sd1                     , */\
   { 0x1270, "TDMSI0EN"},    /* TDM sink0 enable                                  , */\
   { 0x1280, "TDMSI1EN"},    /* TDM sink1 enable                                  , */\
   { 0x1290, "TDMSI2EN"},    /* TDM sink2 enable                                  , */\
   { 0x12a0, "TDMSO0EN"},    /* TDM source0 enable                                , */\
   { 0x12b0, "TDMSO1EN"},    /* TDM source1 enable                                , */\
   { 0x12c0, "TDMSO2EN"},    /* TDM source2 enable                                , */\
   { 0x12d0, "TDMSI0IO"},    /* tdm_sink0_io                                      , */\
   { 0x12e0, "TDMSI1IO"},    /* tdm_sink1_io                                      , */\
   { 0x12f0, "TDMSI2IO"},    /* tdm_sink2_io                                      , */\
   { 0x1300, "TDMSO0IO"},    /* tdm_source0_io                                    , */\
   { 0x1310, "TDMSO1IO"},    /* tdm_source1_io                                    , */\
   { 0x1320, "TDMSO2IO"},    /* tdm_source2_io                                    , */\
   { 0x1333, "TDMSI0SL"},    /* sink0_slot [GAIN IN]                              , */\
   { 0x1373, "TDMSI1SL"},    /* sink1_slot [CH1 IN]                               , */\
   { 0x13b3, "TDMSI2SL"},    /* sink2_slot [CH2 IN]                               , */\
   { 0x1403, "TDMSO0SL"},    /* source0_slot [GAIN OUT]                           , */\
   { 0x1443, "TDMSO1SL"},    /* source1_slot [Voltage Sense]                      , */\
   { 0x1483, "TDMSO2SL"},    /* source2_slot [Current Sense]                      , */\
   { 0x14c3, "NBCK"},    /* NBCK                                              , */\
   { 0x2000, "INTOVDDS"},    /* flag_por_int_out                                  , */\
   { 0x2010, "INTOPLLS"},    /* flag_pll_lock_int_out                             , */\
   { 0x2020, "INTOOTDS"},    /* flag_otpok_int_out                                , */\
   { 0x2030, "INTOOVDS"},    /* flag_ovpok_int_out                                , */\
   { 0x2040, "INTOUVDS"},    /* flag_uvpok_int_out                                , */\
   { 0x2050, "INTOOCDS"},    /* flag_ocp_alarm_int_out                            , */\
   { 0x2060, "INTOCLKS"},    /* flag_clocks_stable_int_out                        , */\
   { 0x2070, "INTOCLIPS"},    /* flag_clip_int_out                                 , */\
   { 0x2080, "INTOMTPB"},    /* mtp_busy_int_out                                  , */\
   { 0x2090, "INTONOCLK"},    /* flag_lost_clk_int_out                             , */\
   { 0x20a0, "INTOSPKS"},    /* flag_cf_speakererror_int_out                      , */\
   { 0x20b0, "INTOACS"},    /* flag_cold_started_int_out                         , */\
   { 0x20c0, "INTOSWS"},    /* flag_engage_int_out                               , */\
   { 0x20d0, "INTOWDS"},    /* flag_watchdog_reset_int_out                       , */\
   { 0x20e0, "INTOAMPS"},    /* flag_enbl_amp_int_out                             , */\
   { 0x20f0, "INTOAREFS"},    /* flag_enbl_ref_int_out                             , */\
   { 0x2201, "INTOACK"},    /* Interrupt status register output - Corresponding flag, */\
   { 0x2300, "INTIVDDS"},    /* flag_por_int_in                                   , */\
   { 0x2310, "INTIPLLS"},    /* flag_pll_lock_int_in                              , */\
   { 0x2320, "INTIOTDS"},    /* flag_otpok_int_in                                 , */\
   { 0x2330, "INTIOVDS"},    /* flag_ovpok_int_in                                 , */\
   { 0x2340, "INTIUVDS"},    /* flag_uvpok_int_in                                 , */\
   { 0x2350, "INTIOCDS"},    /* flag_ocp_alarm_int_in                             , */\
   { 0x2360, "INTICLKS"},    /* flag_clocks_stable_int_in                         , */\
   { 0x2370, "INTICLIPS"},    /* flag_clip_int_in                                  , */\
   { 0x2380, "INTIMTPB"},    /* mtp_busy_int_in                                   , */\
   { 0x2390, "INTINOCLK"},    /* flag_lost_clk_int_in                              , */\
   { 0x23a0, "INTISPKS"},    /* flag_cf_speakererror_int_in                       , */\
   { 0x23b0, "INTIACS"},    /* flag_cold_started_int_in                          , */\
   { 0x23c0, "INTISWS"},    /* flag_engage_int_in                                , */\
   { 0x23d0, "INTIWDS"},    /* flag_watchdog_reset_int_in                        , */\
   { 0x23e0, "INTIAMPS"},    /* flag_enbl_amp_int_in                              , */\
   { 0x23f0, "INTIAREFS"},    /* flag_enbl_ref_int_in                              , */\
   { 0x2501, "INTIACK"},    /* Interrupt register input                          , */\
   { 0x2600, "INTENVDDS"},    /* flag_por_int_enable                               , */\
   { 0x2610, "INTENPLLS"},    /* flag_pll_lock_int_enable                          , */\
   { 0x2620, "INTENOTDS"},    /* flag_otpok_int_enable                             , */\
   { 0x2630, "INTENOVDS"},    /* flag_ovpok_int_enable                             , */\
   { 0x2640, "INTENUVDS"},    /* flag_uvpok_int_enable                             , */\
   { 0x2650, "INTENOCDS"},    /* flag_ocp_alarm_int_enable                         , */\
   { 0x2660, "INTENCLKS"},    /* flag_clocks_stable_int_enable                     , */\
   { 0x2670, "INTENCLIPS"},    /* flag_clip_int_enable                              , */\
   { 0x2680, "INTENMTPB"},    /* mtp_busy_int_enable                               , */\
   { 0x2690, "INTENNOCLK"},    /* flag_lost_clk_int_enable                          , */\
   { 0x26a0, "INTENSPKS"},    /* flag_cf_speakererror_int_enable                   , */\
   { 0x26b0, "INTENACS"},    /* flag_cold_started_int_enable                      , */\
   { 0x26c0, "INTENSWS"},    /* flag_engage_int_enable                            , */\
   { 0x26d0, "INTENWDS"},    /* flag_watchdog_reset_int_enable                    , */\
   { 0x26e0, "INTENAMPS"},    /* flag_enbl_amp_int_enable                          , */\
   { 0x26f0, "INTENAREFS"},    /* flag_enbl_ref_int_enable                          , */\
   { 0x2801, "INTENACK"},    /* Interrupt enable register                         , */\
   { 0x2900, "INTPOLVDDS"},    /* flag_por_int_pol                                  , */\
   { 0x2910, "INTPOLPLLS"},    /* flag_pll_lock_int_pol                             , */\
   { 0x2920, "INTPOLOTDS"},    /* flag_otpok_int_pol                                , */\
   { 0x2930, "INTPOLOVDS"},    /* flag_ovpok_int_pol                                , */\
   { 0x2940, "INTPOLUVDS"},    /* flag_uvpok_int_pol                                , */\
   { 0x2950, "INTPOLOCDS"},    /* flag_ocp_alarm_int_pol                            , */\
   { 0x2960, "INTPOLCLKS"},    /* flag_clocks_stable_int_pol                        , */\
   { 0x2970, "INTPOLCLIPS"},    /* flag_clip_int_pol                                 , */\
   { 0x2980, "INTPOLMTPB"},    /* mtp_busy_int_pol                                  , */\
   { 0x2990, "INTPOLNOCLK"},    /* flag_lost_clk_int_pol                             , */\
   { 0x29a0, "INTPOLSPKS"},    /* flag_cf_speakererror_int_pol                      , */\
   { 0x29b0, "INTPOLACS"},    /* flag_cold_started_int_pol                         , */\
   { 0x29c0, "INTPOLSWS"},    /* flag_engage_int_pol                               , */\
   { 0x29d0, "INTPOLWDS"},    /* flag_watchdog_reset_int_pol                       , */\
   { 0x29e0, "INTPOLAMPS"},    /* flag_enbl_amp_int_pol                             , */\
   { 0x29f0, "INTPOLAREFS"},    /* flag_enbl_ref_int_pol                             , */\
   { 0x2b01, "INTPOLACK"},    /* Interrupt status flags polarity register          , */\
   { 0x4900, "CLIP"},    /* Bypass clip control                               , */\
   { 0x62b0, "CIMTP"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
   { 0x7000, "RST"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "DMEM"},    /* Target memory for access                          , */\
   { 0x7030, "AIF"},    /* Autoincrement-flag for memory-address             , */\
   { 0x7040, "CFINT"},    /* Interrupt CoolFlux DSP                            , */\
   { 0x7087, "REQ"},    /* request for access (8 channels)                   , */\
   { 0x7080, "REQCMD"},    /* Firmware event request rpc command                , */\
   { 0x7090, "REQRST"},    /* Firmware event request reset restart              , */\
   { 0x70a0, "REQMIPS"},    /* Firmware event request short on mips              , */\
   { 0x70b0, "REQMUTED"},    /* Firmware event request mute sequence ready        , */\
   { 0x70c0, "REQVOL"},    /* Firmware event request volume ready               , */\
   { 0x70d0, "REQDMG"},    /* Firmware event request speaker damage detected    , */\
   { 0x70e0, "REQCAL"},    /* Firmware event request calibration completed      , */\
   { 0x70f0, "REQRSV"},    /* Firmware event request reserved                   , */\
   { 0x710f, "MADD"},    /* memory-address to be accessed                     , */\
   { 0x720f, "MEMA"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
   { 0x7307, "ERR"},    /* Coolflux error flags                              , */\
   { 0x7387, "ACK"},    /* acknowledge of requests (8 channels)              , */\
   { 0x7380, "ACKCMD"},    /* Firmware event acknowledge rpc command            , */\
   { 0x7390, "ACKRST"},    /* Firmware event acknowledge reset restart          , */\
   { 0x73a0, "ACKMIPS"},    /* Firmware event acknowledge short on mips          , */\
   { 0x73b0, "ACKMUTED"},    /* Firmware event acknowledge mute sequence ready    , */\
   { 0x73c0, "ACKVOL"},    /* Firmware event acknowledge volume ready           , */\
   { 0x73d0, "ACKDMG"},    /* Firmware event acknowledge speaker damage detected, */\
   { 0x73e0, "ACKCAL"},    /* Firmware event acknowledge calibration completed  , */\
   { 0x73f0, "ACKRSV"},    /* Firmware event acknowledge reserved               , */\
   { 0x8000, "MTPOTC"},    /* Calibration schedule (key2 protected)             , */\
   { 0x8010, "MTPEX"},    /* (key2 protected)                                  , */\
   { 0x8045, "SWPROFIL" },\
   { 0x80a5, "SWVSTEP" },\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

enum tfa1_irq {
	tfa1_irq_vdds = 0,
	tfa1_irq_plls = 1,
	tfa1_irq_ds = 2,
	tfa1_irq_vds = 3,
	tfa1_irq_uvds = 4,
	tfa1_irq_cds = 5,
	tfa1_irq_clks = 6,
	tfa1_irq_clips = 7,
	tfa1_irq_mtpb = 8,
	tfa1_irq_clk = 9,
	tfa1_irq_spks = 10,
	tfa1_irq_acs = 11,
	tfa1_irq_sws = 12,
	tfa1_irq_wds = 13,
	tfa1_irq_amps = 14,
	tfa1_irq_arefs = 15,
	tfa1_irq_ack = 32,
	tfa1_irq_max = 33,
	tfa1_irq_all = -1 /* all irqs */};

#define TFA1_IRQ_NAMETABLE static tfaIrqName_t Tfa1IrqNames[]= {\
	{ 0, "VDDS"},\
	{ 1, "PLLS"},\
	{ 2, "DS"},\
	{ 3, "VDS"},\
	{ 4, "UVDS"},\
	{ 5, "CDS"},\
	{ 6, "CLKS"},\
	{ 7, "CLIPS"},\
	{ 8, "MTPB"},\
	{ 9, "CLK"},\
	{ 10, "SPKS"},\
	{ 11, "ACS"},\
	{ 12, "SWS"},\
	{ 13, "WDS"},\
	{ 14, "AMPS"},\
	{ 15, "AREFS"},\
	{ 16, "16"},\
	{ 17, "17"},\
	{ 18, "18"},\
	{ 19, "19"},\
	{ 20, "20"},\
	{ 21, "21"},\
	{ 22, "22"},\
	{ 23, "23"},\
	{ 24, "24"},\
	{ 25, "25"},\
	{ 26, "26"},\
	{ 27, "27"},\
	{ 28, "28"},\
	{ 29, "29"},\
	{ 30, "30"},\
	{ 31, "31"},\
	{ 32, "ACK"},\
	{ 33, "33"},\
};
