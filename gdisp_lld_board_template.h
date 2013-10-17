/*
 * 2013 Petteri Aimonen <jpa@gfx.mail.kapsi.fi>
 * This file is released to the public domain.
 */

/* Board interface definitions for ED060SC4 PrimeView E-ink panel.
 *
 * You should implement the following functions to define the interface to
 * the panel on your board.
 */

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

/* Set up IO pins for the panel connection. */
static inline void init_board(void) {
#error Unimplemented
}

/* Delay for display waveforms. Should be an accurate microsecond delay. */
static void eink_delay(int us)
{
#error Unimplemented
}

/* Turn the E-ink panel Vdd supply (+3.3V) on or off. */
static inline void setpower_vdd(bool_t on) {
#error Unimplemented
}

/* Turn the E-ink panel negative supplies (-15V, -20V) on or off. */
static inline void setpower_vneg(bool_t on) {
#error Unimplemented
}

/* Turn the E-ink panel positive supplies (-15V, -20V) on or off. */
static inline void setpower_vpos(bool_t on) {
#error Unimplemented
}

/* Set the state of the LE (source driver Latch Enable) pin. */
static inline void setpin_le(bool_t on) {
#error Unimplemented
}

/* Set the state of the OE (source driver Output Enable) pin. */
static inline void setpin_oe(bool_t on) {
#error Unimplemented
}

/* Set the state of the CL (source driver Clock) pin. */
static inline void setpin_cl(bool_t on) {
#error Unimplemented
}

/* Set the state of the SPH (source driver Start Pulse Horizontal) pin. */
static inline void setpin_sph(bool_t on) {
#error Unimplemented
}

/* Set the state of the D0-D7 (source driver Data) pins. */
static inline void setpins_data(uint8_t value) {
#error Unimplemented
}

/* Set the state of the CKV (gate driver Clock Vertical) pin. */
static inline void setpin_ckv(bool_t on) {
#error Unimplemented
}

/* Set the state of the GMODE (gate driver Gate Mode) pin. */
static inline void setpin_gmode(bool_t on) {
#error Unimplemented
}

/* Set the state of the SPV (gate driver Start Pulse Vertical) pin. */
static inline void setpin_spv(bool_t on) {
#error Unimplemented
}

#endif
