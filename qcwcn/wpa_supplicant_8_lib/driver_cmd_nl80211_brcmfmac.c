/*
 * Driver interaction with extended Linux CFG80211
 * Modified for mainline brcmfmac drivers (C) 2020
 * 
 * THIS IS WORK IN POROGRESS / UNSTABLE 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 */

#include "includes.h"
#include <sys/types.h>
#include <fcntl.h>
#include <net/if.h>

#include "common.h"
#include "driver_nl80211.h"
#include "wpa_supplicant_i.h"
#include "config.h"
#ifdef ANDROID
#include "android_drv.h"
#endif

#define WPA_PS_ENABLED		0
#define WPA_PS_DISABLED		1
#define UNUSED(x)	(void)(x)


/* Return type for setBand*/
enum {
	SEND_CHANNEL_CHANGE_EVENT = 0,
	DO_NOT_SEND_CHANNEL_CHANGE_EVENT,
};

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

static void wpa_driver_notify_country_change(void *ctx, char *cmd)
{
	if ((os_strncasecmp(cmd, "COUNTRY", 7) == 0) ||
	    (os_strncasecmp(cmd, "SETBAND", 7) == 0)) {
		union wpa_event_data event;

		os_memset(&event, 0, sizeof(event));
		event.channel_list_changed.initiator = REGDOM_SET_BY_USER;
		if (os_strncasecmp(cmd, "COUNTRY", 7) == 0) {
			event.channel_list_changed.type = REGDOM_TYPE_COUNTRY;
			if (os_strlen(cmd) > 9) {
				event.channel_list_changed.alpha2[0] = cmd[8];
				event.channel_list_changed.alpha2[1] = cmd[9];
			}
		} else {
			event.channel_list_changed.type = REGDOM_TYPE_UNKNOWN;
		}
		wpa_printf(MSG_INFO, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: Calling wpa_supplicant_event with EVENT_CHANNEL_LIST_CHANGED now\n", __func__);
		wpa_supplicant_event(ctx, EVENT_CHANNEL_LIST_CHANGED, &event);
	}
}

int wpa_driver_nl80211_driver_cmd(void *priv, char *cmd, char *buf,
				  size_t buf_len )
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret = -99;
	char alpha2_arg[2] = "00";

	if (os_strcasecmp(cmd, "START") == 0) {
		wpa_msg(drv->ctx, MSG_ERROR, WPA_EVENT_DRIVER_STATE "TTT-driver-cmd-nl80211_for_brcmfmac command identified: START NOT STARTED TTT!");
	} else if (os_strcasecmp(cmd, "MACADDR") == 0) {

		struct i802_bss *bss = priv;
		const u8* addr =  bss->addr;

		if (addr) {
			ret = os_snprintf(buf, buf_len,
					  "Macaddr = " MACSTR "\n", MAC2STR(addr));
			wpa_msg(drv->ctx, MSG_INFO, "TTT-driver-cmd-nl80211_for_brcmfmac: MACADDR set, ret = %d", ret);
			} else {
				wpa_msg(drv->ctx, MSG_ERROR, "TTT-driver-cmd-nl80211_for_brcmfmac: addr is NULL");
			}
		} else { /* Use private command */
			wpa_printf(MSG_INFO, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: private command detected: %s\n",
				   __func__, cmd);
			if(os_strncasecmp(cmd, "COUNTRY", 7) == 0) {
				wpa_printf(MSG_INFO, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: Trying to set Contry Code via nl80211 \n", __func__);
				if (os_strlen(cmd) > 9) {
					alpha2_arg[0] = cmd[8];
					alpha2_arg[1] = cmd[9];
				}
				ret = wpa_driver_nl80211_ops.set_country(priv, alpha2_arg);
				wpa_driver_notify_country_change(drv->ctx, cmd);
				} else {
					wpa_printf(MSG_ERROR, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: no handler to issue private command\n", __func__);
				
				}
			if(os_strncasecmp(cmd, "BTCOEXMODE", 10) == 0) {
				wpa_printf(MSG_INFO, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: command identified: SetBtCoexistenceMode but ignored.\n", __func__);
				// nl80211_btcoex_mode was suggested to be implemented in brcmfmac in 2013 but it seems it was not considered to be required
			}
			if(os_strncasecmp(cmd, "SETSUSPENDMODE", 14) == 0) {
				wpa_printf(MSG_INFO, "%s: TTT-driver-cmd-nl80211_for_brcmfmac: command identified: SetSupendMode %s but ignored.\n", __func__, cmd);
				// no idea how to transmit this
			}

		}
	return ret;
}


int wpa_driver_set_p2p_noa(void *priv, u8 count, int start, int duration)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_NOA %d %d %d", count, start, duration);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf)+1);
}

int wpa_driver_get_p2p_noa(void *priv, u8 *buf, size_t len)
{
	UNUSED(priv), UNUSED(buf), UNUSED(len);
	/* Return 0 till we handle p2p_presence request completely in the driver */
	return 0;
}

int wpa_driver_set_p2p_ps(void *priv, int legacy_ps, int opp_ps, int ctwindow)
{
	char buf[MAX_DRV_CMD_SIZE];

	memset(buf, 0, sizeof(buf));
	wpa_printf(MSG_DEBUG, "%s: Entry", __func__);
	snprintf(buf, sizeof(buf), "P2P_SET_PS %d %d %d", legacy_ps, opp_ps, ctwindow);
	return wpa_driver_nl80211_driver_cmd(priv, buf, buf, strlen(buf) + 1);
}

int wpa_driver_set_ap_wps_p2p_ie(void *priv, const struct wpabuf *beacon,
				 const struct wpabuf *proberesp,
				 const struct wpabuf *assocresp)
{
	UNUSED(priv), UNUSED(beacon), UNUSED(proberesp), UNUSED(assocresp);
	return 0;
}
