/*
 * Copyright (c) 2020 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HIETH_H
#define __HIETH_H

#include <linux/spinlock.h>
#include <los_event.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define HIETH_TSO_SUPPORTED
#define CONFIG_HIETH_HWQ_XMIT_DEPTH 12
#define HIETH_HWQ_TXQ_SIZE          (2 * CONFIG_HIETH_HWQ_XMIT_DEPTH)
#define HIETH_DRIVER_NAME           "hieth"
#define HIETH_MDIO_FRQDIV           2

/* Interface Mode definitions */
typedef enum {
    PHY_INTERFACE_MODE_MII,
    PHY_INTERFACE_MODE_RMII,
    PHY_INTERFACE_MODE_MAX,
} phy_interface_t;

/* port */
#define UP_PORT     0
#define DOWN_PORT   1

typedef struct {
    spinlock_t lock;
    unsigned long flags;
} HISI_NET_SPINLOCK_T;

#define HISI_NET_LOCK_T(net_lock)    HISI_NET_SPINLOCK_T net_lock
#define HISI_NET_LOCK_INIT(net_lock)         \
    do {                                     \
        spin_lock_init(&((net_lock)->lock)); \
    } while (0)
#define HISI_NET_LOCK_GET(net_lock)                                \
    do {                                                           \
        spin_lock_irqsave(&((net_lock)->lock), (net_lock)->flags); \
    } while (0)
#define HISI_NET_LOCK_PUT(net_lock)                                   \
    do {                                                              \
        spin_unlock_irqrestore(&(net_lock)->lock, (net_lock)->flags); \
    } while (0)

struct hieth_netdev_local {
#ifdef HIETH_TSO_SUPPORTED
    struct dma_tx_desc *dma_tx;
    unsigned long dma_tx_phy;
    unsigned int sg_head;
    unsigned int sg_tail;
#endif
    char *iobase; /* virtual io addr */
    unsigned long iobase_phys; /* physical io addr */
    int port; /* 0 => up port, 1 => down port */

#ifdef HIETH_TSO_SUPPORTED
    struct tx_pkt_info *txq;
    unsigned int txq_head;
    unsigned int txq_tail;
    int q_size;
#endif
    int tx_hw_cnt;

    struct {
        int hw_xmitq;
    } depth;

#define SKB_SIZE (HIETH_MAX_FRAME_SIZE)
    unsigned int phy_id;
    unsigned int mdio_frqdiv;
    int link_stat;
    int tx_busy;

    int phy_mode;
    int phy_addr;
    HISI_NET_LOCK_T(tx_lock); /*lint !e19*/
    HISI_NET_LOCK_T(rx_lock); /*lint !e19*/
};

// hieth context
struct hieth_platform_data {
    struct hieth_netdev_local stNetdevLocal;
    EVENT_CB_S stEvent;
};

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif

/* vim: set ts=8 sw=8 tw=78: */
