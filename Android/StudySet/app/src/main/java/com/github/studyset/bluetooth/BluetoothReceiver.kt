package com.github.studyset.bluetooth

import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log

// 单例模式
class BluetoothReceiver private constructor(): BroadcastReceiver()  {

    companion object {
        val instance: BluetoothReceiver by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
            BluetoothReceiver()
        }
    }

    override fun onReceive(p0: Context, p1: Intent) {
        val action = p1.action.toString()
        when (action) {
            BluetoothDevice.ACTION_FOUND -> {
                val device: BluetoothDevice? = p1.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                device?.let {
                    val deviceName = device.name
                    val deviceMac = device.address
                }
            }
        }
    }
}