package com.github.studyset.bluetooth

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.Toast
import androidx.fragment.app.Fragment
import com.github.studyset.R
import kotlinx.android.synthetic.main.bluetooth_fragment_layout.*

class BluetoothFragment : Fragment() {

    companion object {
        private const val REQUEST_ENABLE_BT = 1
        private const val TAG = "BluetoothFragment"
    }


    private var isRegistered = false


    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View = inflater.inflate(R.layout.bluetooth_fragment_layout, container, false)



    override fun onResume() {
        super.onResume()
        if (context?.checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            Log.i(TAG, "没有权限")
            requestPermissions(listOf(Manifest.permission.ACCESS_COARSE_LOCATION).toTypedArray(), 0x01)
        }

        processView.setSpeed(1024)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {

        when (requestCode) {
            REQUEST_ENABLE_BT -> {
                if (resultCode == Activity.RESULT_OK) {
                    Toast.makeText(context, "开启蓝牙成功", Toast.LENGTH_SHORT).show()
                } else {
                    Toast.makeText(context, "开启蓝牙失败", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }


    override fun onPause() {
        super.onPause()

        if (isRegistered) {
            context?.unregisterReceiver(BluetoothReceiver.instance)
            isRegistered = false
        }
    }


    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        when (requestCode) {
            0x01 -> {
                if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                }
            }
            else -> {
                super.onRequestPermissionsResult(requestCode, permissions, grantResults)
            }
        }
    }
}