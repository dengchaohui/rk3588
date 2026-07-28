package com.example.myapplication;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;

import java.util.HashMap;
import java.util.Iterator;
import android.content.Context;
public class ReqActivity extends AppCompatActivity {
    private int REQ_PERMISSION_USB = 16;
    private int REQ_PERMISSION_CAMERA = 18;
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_req);

        boolean hasPermission= ActivityCompat.checkSelfPermission(this,
                Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Android 9 and later needs CAMERA permission to access UVC devices
            if (hasPermission) {
                Log.d("ReqActivity","111 already has permission.");
                scanAttachedDevice();
            } else {
                Log.d("ReqActivity","requestCameraPermission");
                requestCameraPermission();
            }
        } else {
            Log.d("ReqActivity","before Android 9,CAMERA permission");
        }
    }

    public static void verifyStoragePermissions(Activity activity) {
        final int REQUEST_EXTERNAL_STORAGE = 1;
        String[] PERMISSIONS_STORAGE = {
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE};
        // Check if we have write permission
        int permission = ActivityCompat.checkSelfPermission(activity,
                Manifest.permission.WRITE_EXTERNAL_STORAGE);

        if (permission != PackageManager.PERMISSION_GRANTED) {
            // We don't have permission so prompt the user
            ActivityCompat.requestPermissions(activity, PERMISSIONS_STORAGE,
                    REQUEST_EXTERNAL_STORAGE);
        }
    }

    private void requestCameraPermission() {
        String[] strArray = {Manifest.permission.CAMERA};
        ActivityCompat.requestPermissions(ReqActivity.this, strArray, REQ_PERMISSION_CAMERA);
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        for (int i = 0; i < permissions.length; i++) {
            handlePermissionResult(permissions[i],
                    grantResults[i] == PackageManager.PERMISSION_GRANTED);
        }
    }
    private void handlePermissionResult(String permission, Boolean result) {
        if (result) {
            if (permission == Manifest.permission.CAMERA) {
                Log.d("ReqActivity", "CAMERA permission granted");
                scanAttachedDevice();
            }
        } else {
            Log.d("ReqActivity", "CAMERA permission denied");
        }
    }

    private void scanAttachedDevice() {
        UsbManager manager = (UsbManager)getSystemService(Context.USB_SERVICE);
        HashMap<String, UsbDevice> deviceList = manager.getDeviceList();
        Iterator<UsbDevice> deviceIterator = deviceList.values().iterator();

        while (deviceIterator.hasNext()) {
            UsbDevice device = deviceIterator.next();
            Log.d("ReqActivity", device.getDeviceName());
            if(isUVC(device)){
                if (!manager.hasPermission(device)) {
                    requestUsbPermission(manager, device);
                } else {
                    Log.d("ReqActivity", "already has usb permission " + device.getDeviceName());
                }
            }
        }
        Intent intent = new Intent();
        intent.setClass(ReqActivity.this, MainActivity.class);
        ReqActivity.this.startActivity(intent);
        ReqActivity.this.finish();
    }

    private void requestUsbPermission(UsbManager manager, UsbDevice device) {
        String ACTION_USB_PERMISSION = "ACTION_USB_PERMISSION";
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                PendingIntent permissionIntent = PendingIntent.getBroadcast(ReqActivity.this, REQ_PERMISSION_USB, new Intent(ACTION_USB_PERMISSION), 0);
                manager.requestPermission(device, permissionIntent);
            }
        });
    }
    /**
     * check whether the specific device or one of its interfaces is VIDEO class
     */
    private boolean isUVC(UsbDevice device) {
        boolean result = false;
        if (device != null) {
            if (device.getDeviceClass() == UsbConstants.USB_CLASS_VIDEO) {
                result = true;
            }
        }
        return result;
    }

    class MyPermissionListener extends BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent != null) {
                handleIntent(intent);
            }
        }
    }
    private void handleIntent(Intent intent) {
        boolean hasPermission = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);

        if(hasPermission) {
        }
    }
}