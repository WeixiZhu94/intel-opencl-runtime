/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<XE_HPG_COREFamily>;
template class WddmCommandStreamReceiver<XE_HPG_COREFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<XE_HPG_COREFamily>>;
} // namespace NEO
