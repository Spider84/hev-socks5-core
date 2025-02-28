/*
 ============================================================================
 Name        : hev-socks5-misc.h
 Author      : Heiher <r@hev.cc>
 Copyright   : Copyright (c) 2021 hev
 Description : Socks5 Misc
 ============================================================================
 */

#ifndef __HEV_SOCKS5_MISC_H__
#define __HEV_SOCKS5_MISC_H__

#ifdef __cplusplus
extern "C" {
#endif

int hev_socks5_task_io_yielder (HevTaskYieldType type, void *data);

#ifdef __cplusplus
}
#endif

#endif /* __HEV_SOCKS5_MISC_H__ */
