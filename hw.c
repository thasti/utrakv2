#include <msp430.h>
#include <inttypes.h>
#include "hw.h"
#include "main.h"

extern volatile uint16_t isr_flags;

void hw_init(void) {
    /* setup watchdog */
    WDTCTL = WDTPW + WDTHOLD;

    /* setup DCO */
    CSCTL0_H = 0xA5;
    CSCTL1 = DCOFSEL_0;
    CSCTL2 = SELA__DCOCLK + SELS__XT1CLK + SELM__DCOCLK;
    CSCTL3 = DIVA__2 + DIVS__32 + DIVM__4;
    CSCTL4 = XT1OFF + XT2OFF;

    /* setup voltage enable pins */
    POUT_VEN |= BIT_VEN_GPS + BIT_VEN_RF;
    PDIR_VEN |= BIT_VEN_GPS + BIT_VEN_RF;

    /* setup unused pins */
    PDIR_UNUSED1 |= BIT_UNUSED1_A + BIT_UNUSED1_B;
    POUT_UNUSED1 &= ~(BIT_UNUSED1_A + BIT_UNUSED1_B);
    PDIR_UNUSED2 |= BIT_UNUSED2_A + BIT_UNUSED2_B + BIT_UNUSED2_C +
        BIT_UNUSED2_D + BIT_UNUSED2_E;
    POUT_UNUSED2 &= ~(BIT_UNUSED2_A + BIT_UNUSED2_B + BIT_UNUSED2_C +
        BIT_UNUSED2_D + BIT_UNUSED2_E);

    /* setup ADC pins */
    PSEL0_ADC |= BIT_ADC_VSOL + BIT_ADC_VBAT; 

    /* setup GPS and RF pins */
    hw_gps_config(MODULE_DISABLE);
    hw_rf_config(MODULE_DISABLE);

    /* setup heartbeat timer
     *   * clock source: ACLK = FDCO/2
     *   * prescaler: normal 8, extended 8
     *   * clock speed: 5.37 MHz / 2 / 8 / 8 = ~42 kHz
     *   * heartbeat at 1 s intervals
     *   * continuous mode
     */
	TA0CCR0 = 0;
    TA0CTL = TASSEL_1 + MC_2 + ID_3 + TACLR;
    TA0EX0 = TAIDEX_7;
    TA0CCTL0 |= CCIE;

    /* setup WSPR baud rate timer
     *   * clock source. SMCLK = FXT1/32
     *   * prescaler 8
     *   * clock speed: 16.3676 MHz / 32 / 8 = ~64kHz
     *   * WSPR symbol time = 8192/12000 s
     *   * continuous mode
     */
    TB0CCR0 = 0;
    TB0CTL = TBSSEL_2 + MC_2 + ID_3 + TBCLR;
    TB0CCTL0 |= CCIE;
	
    /* enable interrupts globally */
    __bis_SR_register(GIE);
}

void hw_watchdog_feed(void) {
	WDTCTL = WDTPW + WDTCNTCL + WDTSSEL_1 + WDTIS_1;
}

void hw_gps_config(hw_module_state state) {
    switch (state) {
    case MODULE_DISABLE:
        /* disable GPS voltage */
        POUT_VEN |= BIT_VEN_GPS;

        /* disable UART */
        UCA0CTL1 = UCSWRST;

        /* set pins to GPIO */
        POUT_UART &= ~(BIT_UART_TXD + BIT_UART_RXD);
        PDIR_UART |= BIT_UART_TXD + BIT_UART_RXD;
        PSEL0_UART &= ~(BIT_UART_TXD + BIT_UART_RXD);
        PSEL1_UART &= ~(BIT_UART_TXD + BIT_UART_RXD);

        POUT_1PPS &= ~BIT_1PPS;
        PDIR_1PPS |= ~BIT_1PPS;
        break;

    case MODULE_ENABLE:
        /* set to USCI alternate function */
        PDIR_UART &= ~BIT_UART_RXD;
        PSEL0_UART &= ~(BIT_UART_TXD + BIT_UART_RXD);
        PSEL1_UART |= BIT_UART_TXD + BIT_UART_RXD;

        /* set 1PPS pin to input */
        PDIR_1PPS &= ~BIT_1PPS;

        /* configure and enable UART
         *   * ACLK = FDCO/2 is used for baud rate generation
         *   * FDCO = 5.37 MHz
         *   * N = 279,688, therefore OS16 enabled
         *   * BR = INT(N / 16) = 17
         *   * BRF = INT(((N/16) - INT(N/16)) * 16) = 7
         *   * BRS = table(N - INT(N)) = 0,688 -> 0xD6
         */
        UCA0CTL1 = UCSWRST;
        UCA0CTL1 |= UCSSEL_1;
        UCA0BR0 = 17;
        UCA0BR1 = 0;
        UCA0MCTLW = (0xD6<<8) + (7<<4) + UCOS16;
        UCA0CTL1 &= ~UCSWRST;
        
        /* enable GPS voltage */
        POUT_VEN &= ~BIT_VEN_GPS;
        break;
    }

}

void hw_rf_config(hw_module_state state) {
    switch (state) {
    case MODULE_DISABLE:
        /* disable XT1 oscillator */
        SFRIE1 &= ~OFIE;
        CSCTL0_H = 0xA5;
        CSCTL4 |= XT1BYPASS + XTS;
        CSCTL4 |= XT1OFF;

        /* disable RF voltage */
        POUT_VEN |= BIT_VEN_RF;

        /* disable I2C */
        UCB0CTLW0 |= UCSWRST;

        /* set pins to GPIO */
        POUT_I2C &= ~(BIT_I2C_SCL + BIT_I2C_SDA);
        PDIR_I2C |= BIT_I2C_SDA + BIT_I2C_SCL;
        PSEL0_I2C &= ~(BIT_I2C_SCL + BIT_I2C_SDA);
        PSEL1_I2C &= ~(BIT_I2C_SCL + BIT_I2C_SDA);

        POUT_CLKIN &= ~BIT_CLKIN;
        PDIR_CLKIN |= BIT_CLKIN;
        PSEL0_CLKIN &= ~BIT_CLKIN;
        PSEL1_CLKIN &= ~BIT_CLKIN;
        break;

    case MODULE_ENABLE:
        /* set to I2C alternate function */
        PSEL0_I2C &= ~(BIT_I2C_SCL + BIT_I2C_SDA);
        PSEL1_I2C |= BIT_I2C_SCL + BIT_I2C_SDA;

        /* set oscillator input to XT1 */
        PSEL0_CLKIN |= BIT_CLKIN;
        PSEL1_CLKIN &= ~BIT_CLKIN;

        /* configure I2C
         *   * clock source = ACLK = FDCO/16
         *   * FDCO = 5.37 MHz
         *   * max baud rate = ACLK/4 = ~84 kHz
         *   * don't release reset
         */
        UCB0CTLW0 |= UCSWRST;
        UCB0CTLW0 &= ~(UCSSEL_3);
        UCB0CTLW0 |= UCMODE_3 + UCMST + UCSYNC + UCSSEL_1;
        UCB0CTLW1 |= UCASTP_2;
        UCB0BRW    = 0x0004;
        
        /* enable RF voltage */
        POUT_VEN &= ~BIT_VEN_RF;

        /* enable XT1 oscillator */
        CSCTL0_H = 0xA5;
        CSCTL4 |= XT1BYPASS + XTS;
        CSCTL4 &= ~XT1OFF;
        do {
            CSCTL5 &= ~XT1OFFG;
            SFRIFG1 &= ~OFIFG;
        } while (SFRIFG1 & OFIFG);
        SFRIE1 |= OFIE;
        break;
    }
}

void hw_enter_low_power_mode(void) {
    __bis_SR_register(LPM3_bits);
}

void hw_reset_wspr_baud_timer(void) {
    TB0CCR0 = 0;
    TB0R = 65535;
}

/* CPU is running at MCLK = FDCO/4
 * MCLK = 5.37 MHz / 4 -> 0,74 us period
 * approximately waiting as many cycles as microseconds should be good enough
*/

void hw_delay_ms(uint16_t ms) {
    int i;
    
    for (i = 0; i < ms; i++) {
        hw_delay_us(1000);
    }
}

#pragma vector=UNMI_VECTOR
__interrupt void UNMI_ISR(void)
{
    /* just hang here until the fault is gone */
	do {
		CSCTL5 &= ~XT1OFFG;
		SFRIFG1 &= ~OFIFG;
        hw_delay_us(100);
	} while (SFRIFG1 & OFIFG);
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A (void)
{
	TA0CCR0 += (41953 - 1);
	isr_flags |= ISR_FLAG_HEARTBEAT;
    if (isr_flags & ISR_FLAG_WAKE_CPU) {
        __bic_SR_register_on_exit(LPM3_bits);
    }
}

#pragma vector = TIMER0_B0_VECTOR
__interrupt void Timer_B (void)
{
    TB0CCR0 += (43647 - 1);
    isr_flags |= ISR_FLAG_WSPR_BAUD;
}
