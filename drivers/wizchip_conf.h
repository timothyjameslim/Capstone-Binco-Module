#ifndef _WIZCHIP_CONF_H_
#define _WIZCHIP_CONF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* --- Chip selection (W5500 only) --------------------------------------- */
#define W5500 5500

#ifndef _WIZCHIP_
#define _WIZCHIP_ W5500
#endif

/* --- IO modes (keep only what W5500 uses) ------------------------------ */
#define _WIZCHIP_IO_MODE_NONE_     0x0000
#define _WIZCHIP_IO_MODE_SPI_      0x0200
#define _WIZCHIP_IO_MODE_SPI_VDM_  (_WIZCHIP_IO_MODE_SPI_ + 1)
#define _WIZCHIP_IO_MODE_SPI_FDM_  (_WIZCHIP_IO_MODE_SPI_ + 2)

/* we just default to SPI */
#ifndef _WIZCHIP_IO_MODE_
#define _WIZCHIP_IO_MODE_ _WIZCHIP_IO_MODE_SPI_
#endif

/* --- W5500 block -------------------------------------------------------- */
#if (_WIZCHIP_ == W5500)

#define _WIZCHIP_ID_ "W5500\0"
typedef uint8_t iodata_t;
#include "w5500.h"

#else
#error "This project is W5500-only."
#endif

/* --- socket count (W5500 has 8) ---------------------------------------- */
#define _WIZCHIP_SOCK_NUM_ 8

/* ----------------------------------------------------------------------- */
/* Core WIZCHIP descriptor (kept close to original so ioLibrary works)     */
/* ----------------------------------------------------------------------- */
typedef struct __WIZCHIP
{
    uint16_t  if_mode;     /* host interface mode */
    uint8_t   id[8];       /* "W5500" */

    struct {
        void (*_enter)(void);
        void (*_exit)(void);
    } CRIS;

    struct {
        void (*_select)(void);
        void (*_deselect)(void);
    } CS;

    union {
        struct {
            uint8_t (*_read_byte)(void);
            void    (*_write_byte)(uint8_t wb);
            void    (*_read_burst)(uint8_t* pBuf, uint16_t len);
            void    (*_write_burst)(uint8_t* pBuf, uint16_t len);
        } SPI;
    } IF;
} _WIZCHIP;

extern _WIZCHIP WIZCHIP;

/* ----------------------------------------------------------------------- */
/* control enums – keep only ones W5500 code actually uses                 */
/* ----------------------------------------------------------------------- */
typedef enum {
    CW_RESET_WIZCHIP,
    CW_INIT_WIZCHIP,
    CW_GET_INTERRUPT,
    CW_CLR_INTERRUPT,
    CW_SET_INTRMASK,
    CW_GET_INTRMASK,
    CW_SET_INTRTIME,
    CW_GET_INTRTIME,
    CW_GET_ID,
    CW_RESET_PHY,
    CW_SET_PHYCONF,
    CW_GET_PHYCONF,
    CW_GET_PHYSTATUS,
    CW_SET_PHYPOWMODE,
    CW_GET_PHYPOWMODE,
} ctlwizchip_type;

typedef enum {
    CN_SET_NETINFO,
    CN_GET_NETINFO,
    CN_SET_NETMODE,
    CN_GET_NETMODE,
    CN_SET_TIMEOUT,
    CN_GET_TIMEOUT,
} ctlnetwork_type;

/* ----------------------------------------------------------------------- */
/* W5500 interrupt bits only                                               */
/* ----------------------------------------------------------------------- */
typedef enum {
    IK_WOL              = (1 << 4),
    IK_PPPOE_TERMINATED = (1 << 5),
    IK_DEST_UNREACH     = (1 << 6),
    IK_IP_CONFLICT      = (1 << 7),

    IK_SOCK_0           = (1 << 8),
    IK_SOCK_1           = (1 << 9),
    IK_SOCK_2           = (1 << 10),
    IK_SOCK_3           = (1 << 11),
    IK_SOCK_4           = (1 << 12),
    IK_SOCK_5           = (1 << 13),
    IK_SOCK_6           = (1 << 14),
    IK_SOCK_7           = (1 << 15),

    IK_SOCK_ALL         = (0xFFu << 8)
} intr_kind;

/* ----------------------------------------------------------------------- */
/* PHY config – valid for W5500                                            */
/* ----------------------------------------------------------------------- */
#define PHY_CONFBY_HW    0
#define PHY_CONFBY_SW    1
#define PHY_MODE_MANUAL  0
#define PHY_MODE_AUTONEGO 1
#define PHY_SPEED_10     0
#define PHY_SPEED_100    1
#define PHY_DUPLEX_HALF  0
#define PHY_DUPLEX_FULL  1
#define PHY_LINK_OFF     0
#define PHY_LINK_ON      1
#define PHY_POWER_NORM   0
#define PHY_POWER_DOWN   1

typedef struct wiz_PhyConf_t {
    uint8_t by;
    uint8_t mode;
    uint8_t speed;
    uint8_t duplex;
} wiz_PhyConf;

/* ----------------------------------------------------------------------- */
/* IPv4-only network structs (W5500 style)                                 */
/* ----------------------------------------------------------------------- */
typedef enum {
    NETINFO_STATIC = 1,
    NETINFO_DHCP
} dhcp_mode;

typedef struct wiz_NetInfo_t {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t sn[4];
    uint8_t gw[4];
    uint8_t dns[4];
    dhcp_mode dhcp;
} wiz_NetInfo;

typedef enum {
    NM_FORCEARP  = (1 << 1),  /* W5500-only */
    NM_WAKEONLAN = (1 << 5),
    NM_PINGBLOCK = (1 << 4),
    NM_PPPOE     = (1 << 3),
} netmode_type;

typedef struct wiz_NetTimeout_t {
    uint8_t  retry_cnt;
    uint16_t time_100us;
} wiz_NetTimeout;

/* ----------------------------------------------------------------------- */
/* API declarations we still want                                          */
/* ----------------------------------------------------------------------- */
void   reg_wizchip_cris_cbfunc(void(*cris_en)(void), void(*cris_ex)(void));
void   reg_wizchip_cs_cbfunc(void(*cs_sel)(void), void(*cs_desel)(void));
void   reg_wizchip_spi_cbfunc(uint8_t (*spi_rb)(void), void (*spi_wb)(uint8_t wb));
void   reg_wizchip_spiburst_cbfunc(void (*spi_rb)(uint8_t* pBuf, uint16_t len),
                                   void (*spi_wb)(uint8_t* pBuf, uint16_t len));

int8_t ctlwizchip(ctlwizchip_type cwtype, void* arg);
int8_t ctlnetwork(ctlnetwork_type cntype, void* arg);

void   wizchip_sw_reset(void);
int8_t wizchip_init(uint8_t* txsize, uint8_t* rxsize);

void      wizchip_clrinterrupt(intr_kind intr);
intr_kind wizchip_getinterrupt(void);
void      wizchip_setinterruptmask(intr_kind intr);
intr_kind wizchip_getinterruptmask(void);

int8_t wizphy_getphylink(void);
int8_t wizphy_getphypmode(void);
void   wizphy_reset(void);
void   wizphy_setphyconf(wiz_PhyConf* phyconf);
void   wizphy_getphyconf(wiz_PhyConf* phyconf);
void   wizphy_getphystat(wiz_PhyConf* phyconf);
int8_t wizphy_setphypmode(uint8_t pmode);

void wizchip_setnetinfo(wiz_NetInfo* pnetinfo);
void wizchip_getnetinfo(wiz_NetInfo* pnetinfo);
int8_t wizchip_setnetmode(netmode_type netmode);
netmode_type wizchip_getnetmode(void);
void wizchip_settimeout(wiz_NetTimeout* nettime);
void wizchip_gettimeout(wiz_NetTimeout* nettime);

#ifdef __cplusplus
}
#endif

#endif /* _WIZCHIP_CONF_H_ */
