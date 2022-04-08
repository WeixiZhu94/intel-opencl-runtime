/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/dll/devices/product_config_base.inl"

#ifdef SUPPORT_XE_HPG_CORE
#ifdef SUPPORT_DG2
DEVICE_CONFIG_REVISION(DG2_G10_A0, DG2_CONFIG, 0x00)
DEVICE_CONFIG_REVISION(DG2_G10_B0, DG2_CONFIG, 0x04)
#endif
#endif

#if SUPPORT_XE_HPC_CORE
#ifdef SUPPORT_PVC
DEVICE_CONFIG_REVISION(PVC_XT_A0, PVC_CONFIG, 0x03)
DEVICE_CONFIG_REVISION(PVC_XT_B0, PVC_CONFIG, 0x1E)
DEVICE_CONFIG_REVISION(PVC_XL_A0, PVC_CONFIG, 0x00)
DEVICE_CONFIG_REVISION(PVC_XL_B0, PVC_CONFIG, 0x01)
#endif
#endif