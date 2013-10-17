/*
 * 2013 Petteri Aimonen <jpa@gfx.mail.kapsi.fi>
 * This file is released to the public domain.
 */

/* Low-level E-ink panel driver routines for ED060SC4. */

#include "gfx.h"
#include "ed060sc4.h"

#if GFX_USE_GDISP

#include "gdisp/lld/emulation.c"

/* =================================
 *      Default configuration
 * ================================= */

#ifndef GDISP_SCREEN_HEIGHT
#       define GDISP_SCREEN_HEIGHT 600
#endif

#ifndef GDISP_SCREEN_WIDTH
#       define GDISP_SCREEN_WIDTH 800
#endif

/* Number of pixels per byte */
#ifndef EINK_PPB
#       define EINK_PPB 4
#endif

/* Delay for generating clock pulses.
 * Unit is approximate clock cycles of the CPU (0 to 15).
 * This should be atleast 50 ns.
 */
#ifndef EINK_CLOCKDELAY
#       define EINK_CLOCKDELAY 0
#endif

/* Width of one framebuffer block.
 * Must be divisible by EINK_PPB and evenly divide GDISP_SCREEN_WIDTH. */
#ifndef EINK_BLOCKWIDTH
#       define EINK_BLOCKWIDTH 20
#endif

/* Height of one framebuffer block.
 * Must evenly divide GDISP_SCREEN_WIDTH. */
#ifndef EINK_BLOCKHEIGHT
#       define EINK_BLOCKHEIGHT 20
#endif

/* Number of block buffers to use for framebuffer emulation. */
#ifndef EINK_NUMBUFFERS
#       define EINK_NUMBUFFERS 40
#endif

/* Do a "blinking" clear, i.e. clear to opposite polarity first.
 * This reduces the image persistence. */
#ifndef EINK_BLINKCLEAR
#       define EINK_BLINKCLEAR TRUE
#endif

/* Number of passes to use when clearing the display */
#ifndef EINK_CLEARCOUNT
#       define EINK_CLEARCOUNT 10
#endif

/* Number of passes to use when writing to the display */
#ifndef EINK_WRITECOUNT
#       define EINK_WRITECOUNT 4
#endif

/* ====================================
 *      Lower level driver functions
 * ==================================== */

#include "gdisp_lld_board.h"

/** Delay between signal changes, to give time for IO pins to change state. */
static inline void clockdelay()
{
#if EINK_CLOCKDELAY & 1
    asm("nop");
#endif
#if EINK_CLOCKDELAY & 2
    asm("nop");
    asm("nop");
#endif
#if EINK_CLOCKDELAY & 4
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
#endif
#if EINK_CLOCKDELAY & 8
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
    asm("nop");
#endif
}

/** Fast vertical clock pulse for gate driver, used during initializations */
static void vclock_quick()
{
    setpin_ckv(TRUE);
    eink_delay(1);
    setpin_ckv(FALSE);
    eink_delay(4);
}

/** Horizontal clock pulse for clocking data into source driver */
static void hclock()
{
    clockdelay();
    setpin_cl(TRUE);
    clockdelay();
    setpin_cl(FALSE);
}

/** Start a new vertical gate driver scan from top.
 * Note: Does not clear any previous bits in the shift register,
 *       so you should always scan through the whole display before
 *       starting a new scan.
 */
static void vscan_start()
{
    setpin_gmode(TRUE);
    vclock_quick();
    setpin_spv(FALSE);
    vclock_quick();
    setpin_spv(TRUE);
    vclock_quick();
}

/** Waveform for strobing a row of data onto the display.
 * Attempts to minimize the leaking of color to other rows by having
 * a long idle period after a medium-length strobe period.
 */
static void vscan_write()
{
    setpin_ckv(TRUE);
    setpin_oe(TRUE);
    eink_delay(5);
    setpin_oe(FALSE);
    setpin_ckv(FALSE);
    eink_delay(200);
}

/** Waveform used when clearing the display. Strobes a row of data to the
 * screen, but does not mind some of it leaking to other rows.
 */
static void vscan_bulkwrite()
{
    setpin_ckv(TRUE);
    eink_delay(20);
    setpin_ckv(FALSE);
    eink_delay(200);
}

/** Waveform for skipping a vertical row without writing anything.
 * Attempts to minimize the amount of change in any row.
 */
static void vscan_skip()
{
    setpin_ckv(TRUE);
    eink_delay(1);
    setpin_ckv(FALSE);
    eink_delay(100);
}

/** Stop the vertical scan. The significance of this escapes me, but it seems
 * necessary or the next vertical scan may be corrupted.
 */
static void vscan_stop()
{
    setpin_gmode(FALSE);
    vclock_quick();
    vclock_quick();
    vclock_quick();
    vclock_quick();
    vclock_quick();
}

/** Start updating the source driver data (from left to right). */
static void hscan_start()
{
    /* Disable latching and output enable while we are modifying the row. */
    setpin_le(FALSE);
    setpin_oe(FALSE);
    
    /* The start pulse should remain low for the duration of the row. */
    setpin_sph(FALSE);
}

/** Write data to the horizontal row. */
static void hscan_write(const uint8_t *data, int count)
{
    while (count--)
    {
        /* Set the next byte on the data pins */
        setpins_data(*data++);
        
        /* Give a clock pulse to the shift register */
        hclock();
    }
}

/** Finish and transfer the row to the source drivers.
 * Does not set the output enable, so the drivers are not yet active. */
static void hscan_stop()
{
    /* End the scan */
    setpin_sph(TRUE);
    hclock();
    
    /* Latch the new data */
    setpin_le(TRUE);
    clockdelay();
    setpin_le(FALSE);
}

/** Turn on the power to the E-Ink panel, observing proper power sequencing. */
static void power_on()
{
    unsigned i;
    
    /* First the digital power supply and signal levels. */
    setpower_vdd(TRUE);
    setpin_le(FALSE);
    setpin_oe(FALSE);
    setpin_cl(FALSE);
    setpin_sph(TRUE);
    setpins_data(0);
    setpin_ckv(FALSE);
    setpin_gmode(FALSE);
    setpin_spv(TRUE);
    
    /* Min. 100 microsecond delay after digital supply */
    gfxSleepMicroseconds(100);
    
    /* Then negative voltages and min. 1000 microsecond delay. */
    setpower_vneg(TRUE);
    gfxSleepMicroseconds(1000);
    
    /* Finally the positive voltages. */
    setpower_vpos(TRUE);
    
    /* Clear the vscan shift register */
    vscan_start();
    for (i = 0; i < GDISP_SCREEN_HEIGHT; i++)
        vclock_quick();
    vscan_stop();
}

/** Turn off the power, observing proper power sequencing. */
static void power_off()
{
    /* First the high voltages */
    setpower_vpos(FALSE);
    setpower_vneg(FALSE);
    
    /* Wait for any capacitors to drain */
    gfxSleepMilliseconds(100);
    
    /* Then put all signals and digital supply to ground. */
    setpin_le(FALSE);
    setpin_oe(FALSE);
    setpin_cl(FALSE);
    setpin_sph(FALSE);
    setpins_data(0);
    setpin_ckv(FALSE);
    setpin_gmode(FALSE);
    setpin_spv(FALSE);
    setpower_vdd(FALSE);
}

/* ====================================
 *      Framebuffer emulation layer
 * ==================================== */

#if EINK_PPB == 4
#define PIXELMASK 3
#define PIXEL_WHITE 2
#define PIXEL_BLACK 1
#define BYTE_WHITE 0xAA
#define BYTE_BLACK 0x55
#else
#error Unsupported EINK_PPB value.
#endif

#if GDISP_SCREEN_HEIGHT % EINK_BLOCKHEIGHT != 0
#error GDISP_SCREEN_HEIGHT must be evenly divisible by EINK_BLOCKHEIGHT
#endif

#if GDISP_SCREEN_WIDTH % EINK_BLOCKWIDTH != 0
#error GDISP_SCREEN_WIDTH must be evenly divisible by EINK_BLOCKWIDTH
#endif

#if EINK_BLOCKWIDTH % EINK_PPB != 0
#error EINK_BLOCKWIDTH must be evenly divisible by EINK_PPB
#endif

#if EINK_NUMBUFFERS > 254
#error EINK_NUMBUFFERS must be at most 254.
#endif

#define BLOCKS_Y (GDISP_SCREEN_HEIGHT / EINK_BLOCKHEIGHT)
#define BLOCKS_X (GDISP_SCREEN_WIDTH / EINK_BLOCKWIDTH)
#define WIDTH_BYTES (EINK_BLOCKWIDTH / EINK_PPB)

/* Buffers that store the data for a small area of the display. */
typedef struct {
    uint8_t data[EINK_BLOCKHEIGHT][WIDTH_BYTES];
} block_t;

static uint8_t g_next_block; /* Index of the next free block buffer. */
static block_t g_blocks[EINK_NUMBUFFERS];

/* Map that stores the buffers associated to each area of the display.
 * Value of 0 means that the block is not allocated.
 * Other values are the index in g_blocks + 1.
 */
static uint8_t g_blockmap[BLOCKS_Y][BLOCKS_X]; 

/** Check if the row contains any allocated blocks. */
static bool_t blocks_on_row(unsigned by)
{
    unsigned bx;
    for (bx = 0; bx < BLOCKS_X; bx++)
    {
        if (g_blockmap[by][bx] != 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

/** Write out a block row. */
static void write_block_row(unsigned by)
{
    unsigned bx, dy, dx;
    for (dy = 0; dy < EINK_BLOCKHEIGHT; dy++)
    {
        hscan_start();
        for (bx = 0; bx < BLOCKS_X; bx++)
        {
            if (g_blockmap[by][bx] == 0)
            {
                for (dx = 0; dx < WIDTH_BYTES; dx++)
                {
                    const uint8_t dummy = 0;
                    hscan_write(&dummy, 1);
                }
            }
            else
            {
                block_t *block = &g_blocks[g_blockmap[by][bx] - 1];
                hscan_write(&block->data[dy][0], WIDTH_BYTES);
            }
        }
        hscan_stop();
        
        vscan_write();
    }
}

/** Clear the block map, i.e. deallocate all blocks */
static void clear_block_map()
{
    unsigned bx, by;
    for (by = 0; by < BLOCKS_Y; by++)
    {
        for (bx = 0; bx < BLOCKS_X; bx++)
        {
            g_blockmap[by][bx] = 0;
        }
    }
    
    g_next_block = 0;
}

/** Flush all the buffered rows to display. */
static void flush_buffers()
{
    unsigned by, dy, i;
    
    for (i = 0; i < EINK_WRITECOUNT; i++)
    {
        vscan_start();
        
        for (by = 0; by < BLOCKS_Y; by++)
        {
            if (!blocks_on_row(by))
            {
                /* Skip the whole row of blocks. */
                for (dy = 0; dy < EINK_BLOCKHEIGHT; dy++)
                {
                    vscan_skip();
                }
            }
            else
            {
                /* Write out the blocks. */
                write_block_row(by);
            }
        }
        
        vscan_stop();
    }
    
    clear_block_map();
}

/** Initialize a newly allocated block. */
static void zero_block(block_t *block)
{
    unsigned dx, dy;
    for (dy = 0; dy < EINK_BLOCKHEIGHT; dy++)
    {
        for (dx = 0; dx < WIDTH_BYTES; dx++)
        {
            block->data[dy][dx] = 0;
        }
    }
}

/** Allocate a buffer
 * Automatically flushes if all buffers are full. */
static block_t *alloc_buffer(unsigned bx, unsigned by)
{
    block_t *result;
    if (g_blockmap[by][bx] == 0)
    {
        if (g_next_block >= EINK_NUMBUFFERS)
        {
            flush_buffers();
        }
        
        result = &g_blocks[g_next_block];
        g_blockmap[by][bx] = g_next_block + 1;
        g_next_block++;
        zero_block(result);
        return result;
    }
    else
    {
        result = &g_blocks[g_blockmap[by][bx] - 1];
        return result;
    }
}

/* ===============================
 *         Public functions
 * =============================== */

bool_t gdisp_lld_init(void)
{
    init_board();
    
    /* Make sure that all the pins are in "off" state.
     * Having any pin high could cause voltage leaking to the
     * display, which in turn causes the image to leak slowly away.
     */
    power_off();
    
    clear_block_map();
    
    /* Initialize the global GDISP structure */
    GDISP.Width = GDISP_SCREEN_WIDTH;
    GDISP.Height = GDISP_SCREEN_HEIGHT;
    GDISP.Orientation = GDISP_ROTATE_0;
    GDISP.Powermode = powerOff;
    GDISP.Backlight = 0;
    GDISP.Contrast = 0;
#if GDISP_NEED_VALIDATION || GDISP_NEED_CLIP
    GDISP.clipx0 = 0;
    GDISP.clipy0 = 0;
    GDISP.clipx1 = GDISP.Width;
    GDISP.clipy1 = GDISP.Height;
#endif
    
    return TRUE;
}

void gdisp_lld_draw_pixel(coord_t x, coord_t y, color_t color)
{
    block_t *block;
    uint8_t byte;
    unsigned bx, by, dx, dy;
    uint8_t bitpos;
    
    bx = x / EINK_BLOCKWIDTH;
    by = y / EINK_BLOCKHEIGHT;
    dx = x % EINK_BLOCKWIDTH;
    dy = y % EINK_BLOCKHEIGHT;
    
    if (bx < 0 || bx >= BLOCKS_X || by < 0 || by >= BLOCKS_Y)
        return;
    
    block = alloc_buffer(bx, by);
    
    bitpos = (6 - 2 * (dx % EINK_PPB));
    byte = block->data[dy][dx / EINK_PPB];
    byte &= ~(PIXELMASK << bitpos);
    if (color)
    {
        byte |= PIXEL_WHITE << bitpos;
    }
    else
    {
        byte |= PIXEL_BLACK << bitpos;   
    }
    block->data[dy][dx / EINK_PPB] = byte;
}

#if !GDISP_NEED_CONTROL
#error You must enable GDISP_NEED_CONTROL for the E-Ink driver.
#endif

void gdisp_lld_control(unsigned what, void *value) {
    gdisp_powermode_t newmode;
    
    switch(what)
    {
        case GDISP_CONTROL_POWER:
            newmode = (gdisp_powermode_t)value;
            
            if (GDISP.Powermode == newmode)
                return;
            
            if (newmode == powerOn)
            {
                power_on();
            }
            else
            {
                flush_buffers();
                power_off();
            }
            GDISP.Powermode = newmode;
            break;
        
        case GDISP_CONTROL_FLUSH:
            flush_buffers();
            break;
    }
}

/* ===============================
 *       Accelerated routines
 * =============================== */

#if GDISP_HARDWARE_CLEARS

static void subclear(color_t color)
{
    unsigned x, y;
    uint8_t byte;
    
    hscan_start();
    byte = color ? BYTE_WHITE : BYTE_BLACK;
    for (x = 0; x < GDISP_SCREEN_WIDTH; x++)
    {
        hscan_write(&byte, 1);
    }
    hscan_stop();
    
    setpin_oe(TRUE);
    vscan_start();
    for (y = 0; y < GDISP_SCREEN_HEIGHT; y++)
    {
        vscan_bulkwrite();
    }
    vscan_stop();
    setpin_oe(FALSE);
}

void gdisp_lld_clear(color_t color)
{
    unsigned i;
    clear_block_map();
    
    if (EINK_BLINKCLEAR)
    {
        subclear(!color);
        gfxSleepMilliseconds(50);
    }
    
    for (i = 0; i < EINK_CLEARCOUNT; i++)
    {
        subclear(color);
        gfxSleepMilliseconds(10);
    }
    
}
#endif

#endif
