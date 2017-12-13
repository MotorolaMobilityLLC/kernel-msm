/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef TFA_INC_TFA9891_TFAFIELDNAMES_H_
#define TFA_INC_TFA9891_TFAFIELDNAMES_H_

#define TFA9891_I2CVERSION    13

#define TFA9891_NAMETABLE static tfaBfName_t Tfa9891DatasheetNames[]= {\
   { 0x0, "VDDS"},    /* POR                                               , */\
   { 0x10, "PLLS"},    /* PLL                                               , */\
   { 0x20, "OTDS"},    /* OTP                                               , */\
   { 0x30, "OVDS"},    /* OVP                                               , */\
   { 0x40, "UVDS"},    /* UVP                                               , */\
   { 0x50, "OCDS"},    /* OCP                                               , */\
   { 0x60, "CLKS"},    /* Clocks                                            , */\
   { 0x70, "CLIPS"},    /* CLIP                                              , */\
   { 0x80, "MTPB"},    /* MTP                                               , */\
   { 0x90, "DCCS"},    /* BOOST                                             , */\
   { 0xa0, "SPKS"},    /* Speaker                                           , */\
   { 0xb0, "ACS"},    /* cold start flag                                   , */\
   { 0xc0, "SWS"},    /* flag engage                                       , */\
   { 0xd0, "WDS"},    /* flag watchdog reset                               , */\
   { 0xe0, "AMPS"},    /* amplifier is enabled by manager                   , */\
   { 0xf0, "AREFS"},    /* references are enabled by manager                 , */\
   { 0x109, "BATS"},    /* Battery voltage readout; 0[V]..5.5[V]             , */\
   { 0x208, "TEMPS"},    /* Temperature readout                               , */\
   { 0x307, "REV"},    /* Device Revision                                   , */\
   { 0x402, "I2SF"},    /* I2SFormat data 1 input                            , */\
   { 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "CHS3"},    /* Channel Selection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x481, "I2SDOC"},    /* Selection for I2S data out                        , */\
   { 0x4a0, "DISP"},    /* idp protection                                    , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
   { 0x501, "BSSCR"},    /* ProtectionAttackTime                              , */\
   { 0x523, "BSST"},    /* ProtectionThreshold                               , */\
   { 0x561, "BSSRL"},    /* ProtectionMaximumReduction                        , */\
   { 0x582, "BSSRR"},    /* Protection Release Timer                          , */\
   { 0x5b1, "BSSHY"},    /* ProtectionHysterese                               , */\
   { 0x5e0, "BSSR"},    /* battery voltage for I2C read out only             , */\
   { 0x5f0, "BSSBY"},    /* bypass clipper battery protection                 , */\
   { 0x600, "DPSA"},    /* Enable dynamic powerstage activation              , */\
   { 0x613, "AMPSL"},    /* control slope                                     , */\
   { 0x650, "CFSM"},    /* Soft mute in CoolFlux                             , */\
   { 0x670, "BSSS"},    /* batsensesteepness                                 , */\
   { 0x687, "VOL"},    /* volume control (in CoolFlux)                      , */\
   { 0x702, "DCVO"},    /* Boost voltage                                     , */\
   { 0x732, "DCMCC"},    /* Max boost coil current                            , */\
   { 0x7a0, "DCIE"},    /* Adaptive boost mode                               , */\
   { 0x7b0, "DCSR"},    /* Soft RampUp/Down mode for DCDC controller         , */\
   { 0x800, "TROS"},    /* select external temperature also the ext_temp will be put on the temp read out , */\
   { 0x818, "EXTTS"},    /* external temperature setting to be given by host  , */\
   { 0x900, "PWDN"},    /* ON/OFF                                            , */\
   { 0x910, "I2CR"},    /* I2CReset                                          , */\
   { 0x920, "CFE"},    /* EnableCoolFlux                                    , */\
   { 0x930, "AMPE"},    /* EnableAmplifier                                   , */\
   { 0x940, "DCA"},    /* EnableBoost                                       , */\
   { 0x950, "SBSL"},    /* Coolflux configured                               , */\
   { 0x960, "AMPC"},    /* Selection on how AmplifierEnabling                , */\
   { 0x970, "DCDIS"},    /* DCDC not connected                                , */\
   { 0x980, "PSDR"},    /* Iddq test amplifier                               , */\
   { 0x991, "DCCV"},    /* Coil Value                                        , */\
   { 0x9b1, "CCFD"},    /* Selection CoolFluxClock                           , */\
   { 0x9d0, "ISEL"},    /* Interface Selection                               , */\
   { 0x9e0, "IPLL"},    /* selection input PLL for lock                      , */\
   { 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
   { 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
   { 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
   { 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
   { 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
   { 0xb07, "MTPK"},    /* MTP KEY2 register                                 , */\
   { 0xf00, "VDDD"},    /* mask flag_por for interupt generation             , */\
   { 0xf10, "OTDD"},    /* mask flag_otpok for interupt generation           , */\
   { 0xf20, "OVDD"},    /* mask flag_ovpok for interupt generation           , */\
   { 0xf30, "UVDD"},    /* mask flag_uvpok for interupt generation           , */\
   { 0xf40, "OCDD"},    /* mask flag_ocp_alarm for interupt generation       , */\
   { 0xf50, "CLKD"},    /* mask flag_clocks_stable for interupt generation   , */\
   { 0xf60, "DCCD"},    /* mask flag_pwrokbst for interupt generation        , */\
   { 0xf70, "SPKD"},    /* mask flag_cf_speakererror for interupt generation , */\
   { 0xf80, "WDD"},    /* mask flag_watchdog_reset for interupt generation  , */\
   { 0xfe0, "INT"},    /* enabling interrupt                                , */\
   { 0xff0, "INTP"},    /* Setting polarity interupt                         , */\
   { 0x1000, "PDMSEL"},    /* Audio input interface mode                        , */\
   { 0x1010, "I2SMOUTEN"},    /* I2S Master enable (CLK and WS pads)               , */\
   { 0x1021, "PDMORSEL"},    /* PDM Output right channel source selection         , */\
   { 0x1041, "PDMOLSEL"},    /* PDM Output Left/Mono channel source selection     , */\
   { 0x1061, "PADSEL"},    /* Output interface mode and ball selection          , */\
   { 0x1100, "PDMOSDEN"},    /* Secure delay Cell                                 , */\
   { 0x1110, "PDMOSDCF"},    /* Rising Falling Resync control Mux                 , */\
   { 0x1140, "SAAMEN"},    /* Speaker As a Mic feature ON/OFF                   , */\
   { 0x1150, "SAAMLPEN"},    /* speaker_as_mic low power mode (only in PDM_out mode), */\
   { 0x1160, "PDMOINTEN"},    /* PDM output interpolation ratio                    , */\
   { 0x1203, "PDMORG1"},    /* PDM Interpolator Right Channel DS4 G1 Gain Value  , */\
   { 0x1243, "PDMORG2"},    /* PDM Interpolator Right Channel DS4 G2 Gain Value  , */\
   { 0x1303, "PDMOLG1"},    /* PDM Interpolator Left Channel DS4 G1 Gain Value   , */\
   { 0x1343, "PDMOLG2"},    /* PDM Interpolator Left Channel DS4 G2 Gain Value   , */\
   { 0x2202, "SAAMGAIN"},    /* pga gain                                          , */\
   { 0x2250, "SAAMPGACTRL"},    /* 0 = active input common mode voltage source at the attenuator/PGA level, */\
   { 0x2500, "PLLCCOSEL"},    /* pll cco frequency                                 , */\
   { 0x4600, "CSBYPGC"},    /* bypass_gc, bypasses the CS gain correction        , */\
   { 0x4900, "CLIP"},    /* Bypass clip control (function depending on digimux clip_x), */\
   { 0x4910, "CLIP2"},    /* Bypass clip control (function depending on digimux clip_x), */\
   { 0x62b0, "CIMTP"},    /* start copying all the data from i2cregs_mtp to mtp [Key 2 protected], */\
   { 0x7000, "RST"},    /* Reset CoolFlux DSP                                , */\
   { 0x7011, "DMEM"},    /* Target memory for access                          , */\
   { 0x7030, "AIF"},    /* Autoincrement-flag for memory-address             , */\
   { 0x7040, "CFINT"},    /* Interrupt CoolFlux DSP                            , */\
   { 0x7087, "REQ"},    /* request for access (8 channels)                   , */\
   { 0x710f, "MADD"},    /* memory-address to be accessed                     , */\
   { 0x720f, "MEMA"},    /* activate memory access (24- or 32-bits data is written/read to/from memory, */\
   { 0x7307, "ERR"},    /* cf error Flags                                    , */\
   { 0x7387, "ACK"},    /* acknowledge of requests (8 channels")"            , */\
   { 0x8000, "MTPOTC"},    /* Calibration schedule (key2 protected)             , */\
   { 0x8010, "MTPEX"},    /* (key2 protected) calibration of Ron has been executed, */\
   { 0x8045, "SWPROFIL" },\
   { 0x80a5, "SWVSTEP" },\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

#endif /* TFA_INC_TFA9891_TFAFIELDNAMES_H_ */
