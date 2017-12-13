/*
 * Copyright (C) 2014 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define TFA9887_I2CVERSION		34
#define TFA9895_I2CVERSION		34

#define TFA9887_NAMETABLE static tfaBfName_t Tfa9887DatasheetNames[]= {\
   { 0x402, "I2SF"},    /* I2SFormat data 1 input:                           , */\
   { 0x431, "CHS12"},    /* ChannelSelection data1 input  (In CoolFlux)       , */\
   { 0x450, "CHS3"},    /* ChannelSelection data 2 input (coolflux input, the DCDC converter gets the other signal), */\
   { 0x461, "CHSA"},    /* Input selection for amplifier                     , */\
   { 0x4b0, "I2SDOE"},    /* Enable data output                                , */\
   { 0x4c3, "I2SSR"},    /* sample rate setting                               , */\
   { 0x500, "BSSBY"},    /*                                                   , */\
   { 0x511, "BSSCR"},    /* 00 = 0.56 dB/Sample                               , */\
   { 0x532, "BSST"},    /* 000 = 2.92V                                       , */\
   { 0x5f0, "I2SDOC"},    /* selection data out                                , */\
   { 0xa02, "DOLS"},    /* Output selection dataout left channel             , */\
   { 0xa32, "DORS"},    /* Output selection dataout right channel            , */\
   { 0xa62, "SPKL"},    /* Selection speaker induction                       , */\
   { 0xa91, "SPKR"},    /* Selection speaker impedance                       , */\
   { 0xab3, "DCFG"},    /* DCDC speaker current compensation gain            , */\
   { 0x4134, "PWMDEL"},    /* PWM DelayBits to set the delay                    , */\
   { 0x4180, "PWMSH"},    /* PWM Shape                                         , */\
   { 0x4190, "PWMRE"},    /* PWM Bitlength in noise shaper                     , */\
   { 0x48e1, "TCC"},    /* sample & hold track time:                         , */\
   { 0xffff,"Unknown bitfield enum" }   /* not found */\
};

