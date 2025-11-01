//*****************************************************************************
// wizchip_conf.c (W5500-only cleaned version for RP2040/Pico)
// edited the original as there were some code in the original that was interfering compilation
//*****************************************************************************

#include <stddef.h>
#include "wizchip_conf.h"

// -----------------------------------------------------------------------------
// 1. Default callbacks
// -----------------------------------------------------------------------------
void wizchip_cris_enter(void) {}
void wizchip_cris_exit(void)  {}

void wizchip_cs_select(void)   {}
void wizchip_cs_deselect(void) {}

// SPI default (safe no-op)
uint8_t wizchip_spi_readbyte(void) {
    return 0;
}

void wizchip_spi_writebyte(uint8_t wb) {
    (void)wb;
}

// Simple burst using the single-byte fns (works even if user didn’t register)
void wizchip_spi_readburst(uint8_t *pBuf, uint16_t len) {
    while (len--) {
        *pBuf++ = WIZCHIP.IF.SPI._read_byte();
    }
}

void wizchip_spi_writeburst(uint8_t *pBuf, uint16_t len) {
    while (len--) {
        WIZCHIP.IF.SPI._write_byte(*pBuf++);
    }
}

// -----------------------------------------------------------------------------
// 2. Global WIZCHIP instance (W5500 only)
// -----------------------------------------------------------------------------
_WIZCHIP WIZCHIP = {
        .if_mode = _WIZCHIP_IO_MODE_,
        .id      = _WIZCHIP_ID_,
        .CRIS    = { wizchip_cris_enter, wizchip_cris_exit },
        .CS      = { wizchip_cs_select,  wizchip_cs_deselect },
        .IF      = {
                .SPI = {
                        ._read_byte   = wizchip_spi_readbyte,
                        ._write_byte  = wizchip_spi_writebyte,
                        ._read_burst  = wizchip_spi_readburst,
                        ._write_burst = wizchip_spi_writeburst,
                }
        }
};

// -----------------------------------------------------------------------------
// 3. W5500-only network state
// -----------------------------------------------------------------------------
static uint8_t   _DNS_[4];
static dhcp_mode _DHCP_;

// -----------------------------------------------------------------------------
// 4. Registration helpers
// -----------------------------------------------------------------------------
void reg_wizchip_cris_cbfunc(void(*cris_en)(void), void(*cris_ex)(void)) {
    WIZCHIP.CRIS._enter = cris_en ? cris_en : wizchip_cris_enter;
    WIZCHIP.CRIS._exit  = cris_ex ? cris_ex : wizchip_cris_exit;
}

void reg_wizchip_cs_cbfunc(void(*cs_sel)(void), void(*cs_desel)(void)) {
    WIZCHIP.CS._select   = cs_sel   ? cs_sel   : wizchip_cs_select;
    WIZCHIP.CS._deselect = cs_desel ? cs_desel : wizchip_cs_deselect;
}

void reg_wizchip_spi_cbfunc(uint8_t (*spi_rb)(void), void (*spi_wb)(uint8_t wb)) {
    // SPI must be enabled in build
    while (!(WIZCHIP.if_mode & _WIZCHIP_IO_MODE_SPI_));

    if (!spi_rb || !spi_wb) {
        WIZCHIP.IF.SPI._read_byte  = wizchip_spi_readbyte;
        WIZCHIP.IF.SPI._write_byte = wizchip_spi_writebyte;
    } else {
        WIZCHIP.IF.SPI._read_byte  = spi_rb;
        WIZCHIP.IF.SPI._write_byte = spi_wb;
    }
}

void reg_wizchip_spiburst_cbfunc(void (*spi_rb)(uint8_t *pBuf, uint16_t len),
                                 void (*spi_wb)(uint8_t *pBuf, uint16_t len)) {
    while (!(WIZCHIP.if_mode & _WIZCHIP_IO_MODE_SPI_));
    WIZCHIP.IF.SPI._read_burst  = spi_rb ? spi_rb : wizchip_spi_readburst;
    WIZCHIP.IF.SPI._write_burst = spi_wb ? spi_wb : wizchip_spi_writeburst;
}

// -----------------------------------------------------------------------------
// 5. Core control
// -----------------------------------------------------------------------------
void wizchip_sw_reset(void) {
    uint8_t gw[4], sn[4], sip[4];
    uint8_t mac[6];

    getSHAR(mac);
    getGAR(gw);
    getSUBR(sn);
    getSIPR(sip);

    setMR(MR_RST);
    (void)getMR();  // small delay

    setSHAR(mac);
    setGAR(gw);
    setSUBR(sn);
    setSIPR(sip);
}

int8_t wizchip_init(uint8_t *txsize, uint8_t *rxsize) {
    int8_t i;
    int8_t total;

    wizchip_sw_reset();

    // TX
    if (txsize) {
        total = 0;
        for (i = 0; i < _WIZCHIP_SOCK_NUM_; i++) {
            total += txsize[i];
            if (total > 16) return -1;   // W5500 total 16 KB
        }
        for (i = 0; i < _WIZCHIP_SOCK_NUM_; i++)
            setSn_TXBUF_SIZE(i, txsize[i]);
    }

    // RX
    if (rxsize) {
        total = 0;
        for (i = 0; i < _WIZCHIP_SOCK_NUM_; i++) {
            total += rxsize[i];
            if (total > 16) return -1;
        }
        for (i = 0; i < _WIZCHIP_SOCK_NUM_; i++)
            setSn_RXBUF_SIZE(i, rxsize[i]);
    }

    return 0;
}

// interrupt helpers (W5500)
void wizchip_clrinterrupt(intr_kind intr) {
    uint8_t ir  = (uint8_t)intr;
    uint8_t sir = (uint8_t)((uint16_t)intr >> 8);

    setIR(ir);
    for (uint8_t s = 0; s < 8; s++) {
        if (sir & (1u << s)) {
            setSn_IR(s, 0xFF);
        }
    }
}

intr_kind wizchip_getinterrupt(void) {
    uint8_t ir  = getIR();
    uint8_t sir = getSIR();
    uint16_t ret = ((uint16_t)sir << 8) | ir;
    return (intr_kind)ret;
}

void wizchip_setinterruptmask(intr_kind intr) {
    uint8_t imr  = (uint8_t)intr;
    uint8_t simr = (uint8_t)((uint16_t)intr >> 8);
    setIMR(imr);
    setSIMR(simr);
}

intr_kind wizchip_getinterruptmask(void) {
    uint8_t imr  = getIMR();
    uint8_t simr = getSIMR();
    uint16_t ret = ((uint16_t)simr << 8) | imr;
    return (intr_kind)ret;
}

// -----------------------------------------------------------------------------
// 6. ctlwizchip() – W5500 only
// -----------------------------------------------------------------------------
int8_t ctlwizchip(ctlwizchip_type cwtype, void *arg) {
    uint8_t *pbuf[2] = {0, 0};

    switch (cwtype) {
        case CW_RESET_WIZCHIP:
            wizchip_sw_reset();
            break;

        case CW_INIT_WIZCHIP:
            if (arg) {
                pbuf[0] = (uint8_t *)arg;
                pbuf[1] = pbuf[0] + _WIZCHIP_SOCK_NUM_;
            }
            return wizchip_init(pbuf[0], pbuf[1]);

        case CW_CLR_INTERRUPT:
            wizchip_clrinterrupt(*((intr_kind *)arg));
            break;

        case CW_GET_INTERRUPT:
            *((intr_kind *)arg) = wizchip_getinterrupt();
            break;

        case CW_SET_INTRMASK:
            wizchip_setinterruptmask(*((intr_kind *)arg));
            break;

        case CW_GET_INTRMASK:
            *((intr_kind *)arg) = wizchip_getinterruptmask();
            break;

        case CW_SET_INTRTIME:
        setINTLEVEL(*(uint16_t *)arg);
            break;

        case CW_GET_INTRTIME:
            *(uint16_t *)arg = getINTLEVEL();
            break;

        case CW_GET_ID: {
            uint8_t *p = (uint8_t *)arg;
            for (int i = 0; i < 6; i++) p[i] = WIZCHIP.id[i];
            p[6] = 0;
            break;
        }

            // ---- PHY (W5500) ----
        case CW_RESET_PHY:
            wizphy_reset();
            break;

        case CW_SET_PHYCONF:
            wizphy_setphyconf((wiz_PhyConf *)arg);
            break;

        case CW_GET_PHYCONF:
            wizphy_getphyconf((wiz_PhyConf *)arg);
            break;

        case CW_GET_PHYSTATUS:
            wizphy_getphystat((wiz_PhyConf *)arg);
            break;

        case CW_SET_PHYPOWMODE:
            return wizphy_setphypmode(*(uint8_t *)arg);

        case CW_GET_PHYPOWMODE: {
            int8_t pm = wizphy_getphypmode();
            if (pm < 0) return -1;
            *(uint8_t *)arg = (uint8_t)pm;
            break;
        }

            // NOTE: we intentionally do NOT handle CW_GET_PHYLINK here,
            // because your current header doesn’t define it.
        default:
            return -1;
    }

    return 0;
}

// -----------------------------------------------------------------------------
// 7. ctlnetwork() – W5500 only
// -----------------------------------------------------------------------------
int8_t ctlnetwork(ctlnetwork_type cntype, void *arg) {
    switch (cntype) {
        case CN_SET_NETINFO:
            wizchip_setnetinfo((wiz_NetInfo *)arg);
            break;
        case CN_GET_NETINFO:
            wizchip_getnetinfo((wiz_NetInfo *)arg);
            break;
        case CN_SET_NETMODE:
            return wizchip_setnetmode(*(netmode_type *)arg);
        case CN_GET_NETMODE:
            *(netmode_type *)arg = wizchip_getnetmode();
            break;
        case CN_SET_TIMEOUT:
            wizchip_settimeout((wiz_NetTimeout *)arg);
            break;
        case CN_GET_TIMEOUT:
            wizchip_gettimeout((wiz_NetTimeout *)arg);
            break;
        default:
            return -1;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// 8. W5500 network helpers
// -----------------------------------------------------------------------------
void wizchip_setnetinfo(wiz_NetInfo *pnetinfo) {
    setSHAR(pnetinfo->mac);
    setGAR(pnetinfo->gw);
    setSUBR(pnetinfo->sn);
    setSIPR(pnetinfo->ip);

    _DNS_[0] = pnetinfo->dns[0];
    _DNS_[1] = pnetinfo->dns[1];
    _DNS_[2] = pnetinfo->dns[2];
    _DNS_[3] = pnetinfo->dns[3];
    _DHCP_   = pnetinfo->dhcp;
}

void wizchip_getnetinfo(wiz_NetInfo *pnetinfo) {
    getSHAR(pnetinfo->mac);
    getGAR(pnetinfo->gw);
    getSUBR(pnetinfo->sn);
    getSIPR(pnetinfo->ip);

    pnetinfo->dns[0] = _DNS_[0];
    pnetinfo->dns[1] = _DNS_[1];
    pnetinfo->dns[2] = _DNS_[2];
    pnetinfo->dns[3] = _DNS_[3];
    pnetinfo->dhcp   = _DHCP_;
}

int8_t wizchip_setnetmode(netmode_type netmode) {
#if 1
    if (netmode & ~(NM_WAKEONLAN | NM_PPPOE | NM_PINGBLOCK | NM_FORCEARP))
        return -1;
#endif
    uint8_t mr = getMR();
    mr |= (uint8_t)netmode;
    setMR(mr);
    return 0;
}

netmode_type wizchip_getnetmode(void) {
    return (netmode_type)getMR();
}

void wizchip_settimeout(wiz_NetTimeout *nettime) {
    setRCR(nettime->retry_cnt);
    setRTR(nettime->time_100us);
}

void wizchip_gettimeout(wiz_NetTimeout *nettime) {
    nettime->retry_cnt  = getRCR();
    nettime->time_100us = getRTR();
}

// -----------------------------------------------------------------------------
// 9. W5500 PHY fns (unchanged from Wiznet code, but only for 5500)
// -----------------------------------------------------------------------------
void wizphy_reset(void) {
    uint8_t tmp = getPHYCFGR();
    tmp &= PHYCFGR_RST;
    setPHYCFGR(tmp);
    tmp = getPHYCFGR();
    tmp |= ~PHYCFGR_RST;
    setPHYCFGR(tmp);
}

void wizphy_setphyconf(wiz_PhyConf *phyconf) {
    uint8_t tmp = 0;
    if (phyconf->by == PHY_CONFBY_SW) tmp |= PHYCFGR_OPMD;
    else                              tmp &= ~PHYCFGR_OPMD;

    if (phyconf->mode == PHY_MODE_AUTONEGO) {
        tmp |= PHYCFGR_OPMDC_ALLA;
    } else {
        if (phyconf->duplex == PHY_DUPLEX_FULL) {
            tmp |= (phyconf->speed == PHY_SPEED_100) ? PHYCFGR_OPMDC_100F : PHYCFGR_OPMDC_10F;
        } else {
            tmp |= (phyconf->speed == PHY_SPEED_100) ? PHYCFGR_OPMDC_100H : PHYCFGR_OPMDC_10H;
        }
    }
    setPHYCFGR(tmp);
    wizphy_reset();
}

void wizphy_getphyconf(wiz_PhyConf *phyconf) {
    uint8_t tmp = getPHYCFGR();
    phyconf->by   = (tmp & PHYCFGR_OPMD) ? PHY_CONFBY_SW : PHY_CONFBY_HW;

    switch (tmp & PHYCFGR_OPMDC_ALLA) {
        case PHYCFGR_OPMDC_ALLA:
        case PHYCFGR_OPMDC_100FA:
            phyconf->mode = PHY_MODE_AUTONEGO;
            break;
        default:
            phyconf->mode = PHY_MODE_MANUAL;
            break;
    }

    switch (tmp & PHYCFGR_OPMDC_ALLA) {
        case PHYCFGR_OPMDC_100FA:
        case PHYCFGR_OPMDC_100F:
        case PHYCFGR_OPMDC_100H:
            phyconf->speed = PHY_SPEED_100;
            break;
        default:
            phyconf->speed = PHY_SPEED_10;
            break;
    }

    switch (tmp & PHYCFGR_OPMDC_ALLA) {
        case PHYCFGR_OPMDC_100FA:
        case PHYCFGR_OPMDC_100F:
        case PHYCFGR_OPMDC_10F:
            phyconf->duplex = PHY_DUPLEX_FULL;
            break;
        default:
            phyconf->duplex = PHY_DUPLEX_HALF;
            break;
    }
}

void wizphy_getphystat(wiz_PhyConf *phyconf) {
    uint8_t tmp = getPHYCFGR();
    phyconf->duplex = (tmp & PHYCFGR_DPX_FULL) ? PHY_DUPLEX_FULL : PHY_DUPLEX_HALF;
    phyconf->speed  = (tmp & PHYCFGR_SPD_100)  ? PHY_SPEED_100   : PHY_SPEED_10;
}

int8_t wizphy_setphypmode(uint8_t pmode) {
    uint8_t tmp = getPHYCFGR();
    if ((tmp & PHYCFGR_OPMD) == 0)
        return -1;
    tmp &= ~PHYCFGR_OPMDC_ALLA;
    if (pmode == PHY_POWER_DOWN)
        tmp |= PHYCFGR_OPMDC_PDOWN;
    else
        tmp |= PHYCFGR_OPMDC_ALLA;
    setPHYCFGR(tmp);
    wizphy_reset();
    return 0;
}

int8_t wizphy_getphypmode(void) {
    uint8_t tmp = getPHYCFGR();
    if ((tmp & PHYCFGR_OPMDC_ALLA) == PHYCFGR_OPMDC_PDOWN)
        return PHY_POWER_DOWN;
    return PHY_POWER_NORM;
}
