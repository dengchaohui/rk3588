/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.example.partnersupportsampletvinput;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;

import android.media.tv.TvInputService;

import android.media.tv.TvInputManager;
import android.media.tv.TvInputManager.Hardware;
import android.media.tv.TvInputManager.HardwareCallback;
import android.media.tv.TvInputHardwareInfo;
import android.hardware.hdmi.HdmiDeviceInfo;
import android.media.tv.TvInputInfo;
import android.media.tv.TvStreamConfig;
import android.view.KeyEvent;
import android.text.TextUtils;
import android.net.Uri;
import android.os.Bundle;
import android.view.Surface;
import android.util.Log;
import android.content.BroadcastReceiver;
import android.content.IntentFilter;
import java.util.List;

/** SampleTvInputService */
public class SampleTvInputService extends TvInputService {
    public static final String INPUT_ID =
            "com.example.partnersupportsampletvinput/.SampleTvInputService";

    public static final String TAG = "SampleTvInputService";
    private TvInputManager mTvInputManager;
    private Hardware mHardware;
    private TvStreamConfig[] mConfigs;
    private Surface mSurface;
    private BaseTvInputSessionImpl mSession;
    private boolean isTuneFinished = false;
    private int nStreamConfigGeneration = 0;
    private static int mStreamConfigGeneration = 2;//user define, fix it for now.

    private BroadcastReceiver mHomeKeyEventBroadCastReceiver;

    private HardwareCallback mHardwareCallback = new HardwareCallback() {
        @Override
        public void onReleased() {
            Log.e(TAG, "onReleased");
        }
        @Override
        public void onStreamConfigChanged(TvStreamConfig[] configs) {
            for (TvStreamConfig config : configs) {
                Log.e(TAG, "onStreamConfigChanged: " + config.toString() + ", maxWidth = " + config.getMaxWidth() + ", maxHeight = " + config.getMaxHeight() + ", generation=" + config.getGeneration());
            }
            Log.d(TAG, "nStreamConfigGeneration = " + nStreamConfigGeneration);
            if (isTuneFinished) {// && mConfigs != null && mConfigs.length != 0 && mConfigs[0].getMaxWidth() != 0 && mConfigs[0].getMaxWidth() != configs[0].getMaxWidth()) {
                new TvInputStreamChangeThread(configs[0]).run();
                sendPrivCmdBroadcast("sourcechange", new Bundle());
            }
            mConfigs = configs;
        }
        @Override
        public void onPrivCmdToApp(String action, Bundle data) {
            Log.d(TAG, "onPrivCmdToApp " + action);
            sendPrivCmdBroadcast(action, data);
        }
    };

    private void sendPrivCmdBroadcast(String action, Bundle data) {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_HDMIIN_RK_PRIV_CMD);
        intent.putExtra("action", action);
        intent.putExtra("data", data);
        sendBroadcast(intent);
    }

    class TvInputStreamChangeThread extends Thread {
        private TvStreamConfig config;

        public TvInputStreamChangeThread(TvStreamConfig streamConfig) {
            Log.d(TAG, "TvInputStreamChangeThread() streamConfig=" + streamConfig.toString());
            config = new TvStreamConfig.Builder()
                    .streamId(streamConfig.getStreamId())
                    .type(streamConfig.getType())
                    .maxWidth(streamConfig.getMaxWidth())
                    .maxHeight(streamConfig.getMaxHeight())
                    .generation(streamConfig.getGeneration())
                    .build();
        }

        public void run() {
            Log.d(TAG, "config = " + config.toString());
            mHardware.setSurface(mSurface, config);
            isTuneFinished = true;
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "onCreate() in");
        mTvInputManager = (TvInputManager)this.getSystemService(Context.TV_INPUT_SERVICE);

        mHomeKeyEventBroadCastReceiver = new HomeKeyEventBroadCastReceiver();
        registerReceiver(mHomeKeyEventBroadCastReceiver, new IntentFilter(Intent.ACTION_CLOSE_SYSTEM_DIALOGS));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy() in");
        if (mHomeKeyEventBroadCastReceiver != null)
            unregisterReceiver(mHomeKeyEventBroadCastReceiver);
    }

    @Override
    public Session onCreateSession(String s) {
        Log.d(TAG, "onCreateSession() in, trace is " + Log.getStackTraceString(new Throwable()));
        mSession = new BaseTvInputSessionImpl(this);
        return mSession;
    }

    class BaseTvInputSessionImpl extends Session {

        public BaseTvInputSessionImpl(Context context) {
            super(context);
        }

        public void onRelease() {
            Log.d(TAG, "===================onRelease()===========================");
            mHardware.setSurface(null, null);
            isTuneFinished = false;
            nStreamConfigGeneration = 0;
        }

        public boolean onSetSurface(Surface surface) {
            Log.e(TAG, "onSetSurface!   surface = " + surface);
            if (surface == null) {
                mHardware.setSurface(null, null);
                isTuneFinished = false;
                nStreamConfigGeneration = 0;
                return true;
            }
            mSurface = surface;
            return true;
        }

        public void onSetStreamVolume(float v) {}

        @Override
        public boolean onTune(Uri uri) {
            Log.e(TAG, "onTune!     getStreamId = " + mConfigs[0].getStreamId() + ", generation=" + mConfigs[0].getGeneration() + ", width=" + mConfigs[0].getMaxWidth());
            mHardware.setSurface(mSurface, mConfigs[0]);
            isTuneFinished = true;
            nStreamConfigGeneration = mConfigs[0].getGeneration();
            return true;
        }

        @Override
        public void onAppPrivateCommand(String action, Bundle data) {
            if (null != mHardware) {
                mHardware.sendAppPrivateCommand(action, data);
            }
        }

        public void onSetCaptionEnabled(boolean b) {}

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            Log.d(TAG, "onKeyDown keyCode = " + keyCode + ", event =" + event);
//            if (keyCode == KeyEvent.KEYCODE_BACK) {
//                mHardware.setSurface(null, null);
//                isTuneFinished = false;
//                nStreamConfigGeneration = 0;
//                return true;
//            }
            return false;
        }
    }

    private String generateLabel(TvInputHardwareInfo info) {
        int id = info.getDeviceId();
        int type = info.getType();
        switch (type) {
        case TvInputHardwareInfo.TV_INPUT_TYPE_COMPOSITE:
        case TvInputHardwareInfo.TV_INPUT_TYPE_TUNER:
            return "TUNER-" + id;
        case TvInputHardwareInfo.TV_INPUT_TYPE_HDMI:
            return "HDMI-" + id;
        default:
            return "OTHER-" + id;
        }
    }

    @Override
    public TvInputInfo onHardwareAdded(TvInputHardwareInfo hardwareInfo) {
        super.onHardwareAdded(hardwareInfo);
        Log.e(TAG, "+++++++++++++added+++++++++");
        TvInputInfo info = null;
        ResolveInfo rInfo = getResolveInfo(SampleTvInputService.class.getName());
        if (rInfo != null) {
            try {
                info = TvInputInfo.createTvInputInfo(
                        getApplicationContext(),
                        rInfo,
                        hardwareInfo,
                        generateLabel(hardwareInfo),
                        null);
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        }
        // Acquire hardware
        int device_id = hardwareInfo.getDeviceId();
        Log.e(TAG, "device_id: " + device_id);
        mHardware = mTvInputManager.acquireTvInputHardware(device_id, info, mHardwareCallback);
        Log.e(TAG, "+++++++++++ acquireTvInputHardware done. +++++++++++ " + info.toString());
        return info;
    }

    @Override
    public TvInputInfo onHdmiDeviceAdded(HdmiDeviceInfo deviceInfo) {
        super.onHdmiDeviceAdded(deviceInfo);
        int phyAddr = deviceInfo.getPhysicalAddress();
        Log.e(TAG, "+++++++HdmiDeviceInfo++++++added+++++++++ " + phyAddr + ", ");
        TvInputInfo info = null;
        String parentId = info.getId();
        ResolveInfo rInfo = getResolveInfo(SampleTvInputService.class.getName());
        if (rInfo != null) {
            try {
                info = TvInputInfo.createTvInputInfo(
                        getApplicationContext(),
                        rInfo,
                        deviceInfo,
                        parentId,
                        deviceInfo.getDisplayName(),
                        null);
            } catch (Exception e) {
                Log.e(TAG, e.toString());
            }
        }
        // Acquire hardware
        int device_id = deviceInfo.getId();
        Log.e(TAG, "device_id: " + device_id);
        mHardware = mTvInputManager.acquireTvInputHardware(device_id, info, mHardwareCallback);
        Log.e(TAG, "+++++++++++ acquire HDMI TvInputHardware done. +++++++++++ " + info.toString());
        return info;
    }

    private ResolveInfo getResolveInfo(String clazz_name) {
        if (TextUtils.isEmpty(clazz_name))
            return null;
        ResolveInfo ret = null;
        PackageManager pm = getApplicationContext().getPackageManager();
        List<ResolveInfo> services = pm.queryIntentServices(
                new Intent(TvInputService.SERVICE_INTERFACE),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        for (ResolveInfo ri : services) {
            ServiceInfo si = ri.serviceInfo;
            if (!android.Manifest.permission.BIND_TV_INPUT.equals(si.permission)) {
                continue;
            }

            Log.e(TAG, "clazz_name = " + clazz_name + ", si.name = " + si.name);

            if (clazz_name.equals(si.name)) {
                ret = ri;
                break;
            }
        }
        return ret;
    }

    class HomeKeyEventBroadCastReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            Log.e(TAG, "SampleTvInputService HomeKeyEventBroadCastReceiver");
            mHardware.setSurface(null, null);
            isTuneFinished = false;
            nStreamConfigGeneration = 0;
        }
    }
}
