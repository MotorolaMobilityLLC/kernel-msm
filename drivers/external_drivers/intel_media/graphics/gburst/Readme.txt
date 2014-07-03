High level flow
---------------

================================================================================

The following diagram provides the high level flow of graphics driver
interaction with other system components for burst entry/exit and
notification of changes to GPU frequency.

                                                   |-----------------|
                                                   |     Android     |
             ---(ADC Input)----------------------->|     Thermal     |
                                                   |     Manager     |
                                                   |-----------------|
                                                            |
             Thermal State via                              |
             Sysfs/class/system/thermal/coolingdevice/      |
             {maxstate,cstate}                              |
             3 -- Critical Thermal Condition                |
             2 -- Alert                                     |
             1 -- Warning                                   |
             0 -- Normal Thermal Condition                  |
                                                            |
             Graphics Driver                                v
|---------|  must request MSI    |------------|    |-----------------|
|         |  for IRQ TBD         |            |    |                 |
|   SCU   |                      |  Graphics  |    |    Frequency    |
|         |--(MSI Interrupt)---->|   Driver   |--->|      Change     |
|         |                      |            |    |       ISR       |
|---------|                      |------------|    |-----------------|
     ^                                  |                   ^
     |                                  |                   |
     |   Frequency Change               |                   |
     |   1.  boost up/down              |                   |
     |   2.  throttling                 |                   |
     |                                  |                   |
|---------|                             |                   |
|         |<--(MBI Write PWRGT_CNT)-----|                   |
|  PUnit  |                                                 |
|         |----------------------------(MBI Read PWRGT_STS)--
|---------|


In the above diagram, the graphics driver requires the following:

1. Initialization of the PUnit PWRGT_CNT register to receive interrupts
   notification of frequency change events.  Note: this bit must be set on
   every write to this register when interrupt notification is desired.
   For the Clovertrail this is every write to the register other than
   D3 entry.
2. MSI interrupt support from the SCU to route PUnit interrupt events
   via virtual ioapic.
3. Monitoring of Android Thermal Management controlled state changes to
   the cooling device.
4. Reading the frequency and power status from the PUnit PWRGT_STS register.

================================================================================
Registers:
================================================================================
Register PWRGT_CNT -- write-only (except to determine toggle bit state)
================================================================================
--  The graphics driver must remember the last-written value to this register
    and should restore the register value after all S0ix transitions.
--  This register is write only by the graphics driver (except to determine
    toggle bit state)and is accessed via
    Message Bus Interface (MBI).  See MBI write below for more details.
--  PWRGT_CNT[31] (the toggle bit) is toggled on every write (so the firmware
    can detect that the register has been written).
    The value of the toggle bit is preserved across D3.
--  During driver initialization (and exit from D3) the following values
    are written to the PWRGT_CNT register:
--  Upon D3 entry, all bits are set to 0 except toggle.
    See the CLV Gfx Burst HAS for more details.  (FIXME)
--  Upon D3 exit (transition from D3 to S0), entry, all bits are set to 0 except
    toggle and PWRGT_CNT[30] (which is interrupt enable).
    Because the PWRGT_CNT[27:24] is being set to 0, frequency will be
    0000b == 400 MHz (e.g., burst mode off).

PWRGT_CNT bits          Description
--------------          -------------------------------------------------------
PWRGT_CNT[31]           Required to be set by the graphics driver for each
                        write to the PWRGT_CNT register to signal the PUnit.
                        During initialization this bit is set to 1.
                        Each subsequent write to the PWRGT_CNT register must
                        toggle this bit.
                        The driver always preserves the state of this bit
                        including across D3 (Power) Entry/Exit.
PWRGT_CNT[30]           Enables notification of graphics frequency changes to
                        the graphics driver via SCU.  Always set to 1 by
                        the graphics driver unless D3 entry or Driver Unload,
                        in which case, set to 0.
PWRGT_CNT[29]           Reserved the graphics driver will always set to 0
PWRGT_CNT[28]           Enable/disable automatic burst entry.  The graphics
                        driver will set this bit to 1 at initialization and
                        to 0 upon driver unload.

FIXME - This is *automatic* burst mode.  Not wanted when driver is involved, right?

PWRGT_CNT[27:24]        Burst Entry/Exit request.  The graphics driver will
                        set these bits to 0000b (400MHz operation) at
                        initialization and upon driver unload.
                        0000b -- Burst Exit request and IA SW preference
                            for 400 MHz Graphics Clock.
                        0001b -- Burst Entry request and IA SW preference
                            for 533 MHz Graphics Clock
                        Anything else -- reserved
PWRGT_CNT[23:0]         Reserved.  The graphics must set these bits to 0 always.

================================================================================
Register PWRGT_STS -- read-only
================================================================================

On successful read of the PWRGT_STS register the bits below are returned.
See CLV GFX Burst HAS for more details on register values.

PWRGT_STS bits          Description
--------------          -------------------------------------------------------
PWRGT_STS[31]           Fuse status to indicate if bursting is supported on
                        the SKU.
                        1b = burst available.  0 = burst not available.
PWRGT_STS[30]           Graphics Clock Change Interrupt setting.
                        1b = interrupt enabled for graphics driver.
PWRGT_STS[29]           Reserved.  Graphics driver should ignore this bit.
PWRGT_STS[28]           Automatic Burst Entry Enable Setting.
                        1b = PUnit FW performs automatic burst entry under
                        appropriate conditions.
PWRGT_STS[27:24]        Graphics Driver Burst Request Setting.
                        0001b = Burst Entry Request has been processed and
                            graphics frequency preference is for 533Mhz.
                        0000b = Burst Exit request has been processed and
                            graphics frequency preference is 400Mhz.

PWRGT_STS[23:20]        Graphics Clock / Throttle Status
    0001b = Graphics Clock is 533 MHz Unthrottled
    0000b = Graphics Clock is 400 MHz Unthrottled
    1001b = Graphics Clock is 400 MHz at 12.5% Throttled (350 MHz effective)
    1010b = Graphics Clock is 400 MHz at 25% Throttled (300 MHz effective)
    1011b = Graphics Clock is 400 MHz at 37.5% Throttled (250 MHz effective)
    1100b = Graphics Clock is 400 MHz at 50% Throttled (200 MHz effective)
    1101b = Graphics Clock is 400 MHz at 62.5% Throttled (150 MHz effective)
    1110b = Graphics Clock is 400 MHz at 75% Throttled (100 MHz effective)
    1111b = Graphics Clock is 400 MHz at 87.5% Throttled (50 MHz effective)
PWRGT_STS[19:0]         Reserved

================================================================================
Thermal cooling device interface
================================================================================
The following taken from Document/thermal/sysfs-api.txt in the Linux kernel
describes device registration interface:

1.2.1 struct thermal_cooling_device *thermal_cooling_device_register(char *name,
                void *devdata, struct thermal_cooling_device_ops *)

    This interface function adds a new thermal cooling device
    (fan/processor/...) to /sys/class/thermal/ folder as cooling_device[0-*].
    It tries to bind itself to all the thermal zone devices register
    at the same time.
    name: the cooling device name.
    devdata: device private data.
    ops: thermal cooling devices call-backs.
        .get_max_state: get the Maximum throttle state of the cooling device.
        .get_cur_state: get the Current throttle state of the cooling device.
        .set_cur_state: set the Current throttle state of the cooling device.

================================================================================
Burst Determination Flow
================================================================================

/----------\   |-----------|   |------------|
|  Linux   |   |  cpugpu_  |   |  Read      |
|  Kernel  |-->|  thread   |-->|  PWRGT_STS |
|  Timer   |   |           |   |  Graphics  |
|   5ms    |   |           |   |            |
\----------/   |-----------|   |------------|
                                      |
       |------------------------------|
       v
  /----------\
 /  If        \  FIXME - If already active, then exit burst
|  Burst       |---(Yes)--->---------------------------|
|  Entry       |                                       |
 \ Prohibited /                                        |
  \----------/                                         |
       |                                               |
       v                                               |
|--------------|                                       |
|  Read        |                                       |
|  Performance |                                       |
|  Counters    |                                       |
|--------------|                                       |
       |                                               |
       v                                               |
|--------------|                                       |
|  Compute Max |    [Don't know if this is             |
|  Utilization |    precisely what is done.]           |
|  Over last   |                                       |
|  10 samples  |                                       |
|--------------|                                       |
       |                                               |
       v                                               |
  /------------\              /-----------\            |
 /  Burst not   \            /  Burst      \           |
|  Active and    |--(No)--->|  Active and   |--(No)--->v
|  Utilization > |          | Utilization < |          |
 \ Threshold    /            \ Threshold   /           |
  \------------/              \-----------/            |
       |                           |                   |
     (Yes)                       (Yes)                 |
       |                           |                   |
       v                           v                   |
|--------------|            |--------------|           |
|  Request     |            |  Request     |           |
|  Burst       |            |  Burst       |           |
|  Entry       |            |  Exit        |           |
|--------------|            |--------------|           |
       |                           |                   |
       |---------------------------|-------------------|
                                   v
                            |--------------|
                            |  Exit        |
                            |--------------|
================================================================================
