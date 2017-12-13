/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9890_I2CVERSION    34
 
#define TFA9890_NAMETABLE static tfaBfName_t Tfa9890DatasheetNames[]= {\
   { 0x402, "I2SF"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "CHS3"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x481, "I2SDOC"},    /* selection data out                                , */\
   { 0x4a0, "DISP"},    /* idp protection                                    , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
   { 0x732, "DCMCC"},   /* Max boost coil current - step of 500 mA           , */\
   { 0x9c0, "CCFD"},    /* Selection CoolFlux Clock                          , */\
   { 0x9d0, "ISEL"},    /* selection input 1 or 2                            , */\
   { 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
   { 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
   { 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
   { 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
   { 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
   { 0xf00, "VDDD"},    /* mask flag_por for interupt generation             , */\
   { 0xf10, "OTDD"},    /* mask flag_otpok for interupt generation           , */\
   { 0xf20, "OVDD"},    /* mask flag_ovpok for interupt generation           , */\
   { 0xf30, "UVDD"},    /* mask flag_uvpok for interupt generation           , */\
   { 0xf40, "OCDD"},    /* mask flag_ocp_alarm for interupt generation       , */\
   { 0xf50, "CLKD"},    /* mask flag_clocks_stable for interupt generation   , */\
   { 0xf60, "DCCD"},    /* mask flag_pwrokbst for interupt generation        , */\
   { 0xf70, "SPKD"},    /* mask flag_cf_speakererror for interupt generation , */\
   { 0xf80, "WDD"},    /* mask flag_watchdog_reset for interupt generation  , */\
   { 0xf90, "LCLK"},    /* mask flag_lost_clk for interupt generation        , */\
   { 0xfe0, "INT"},    /* enabling interrupt                                , */\
   { 0xff0, "INTP"},    /* Setting polarity interupt                         , */\
   { 0x8f0f, "VERSION"},    /* (key1 protected)                                  , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};
