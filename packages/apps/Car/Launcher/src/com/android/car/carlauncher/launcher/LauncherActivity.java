/**
 * Copyright (c) 2018 The Android Open Source Project
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

package com.android.car.carlauncher.launcher;

import static com.android.car.carlauncher.launcher.PinnedAppListViewModel.PINNED_APPS_KEY;
import static android.car.Car.CAR_OCCUPANT_ZONE_SERVICE;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.app.ActivityOptions;
import android.app.AlertDialog;
import android.app.Application;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Display;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewAnimationUtils;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.GridView;
import android.widget.ImageButton;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.ViewModelProvider;
import androidx.lifecycle.ViewModelProvider.AndroidViewModelFactory;

import com.android.car.carlauncher.launcher.AppEntry;
import com.android.car.carlauncher.launcher.AppEntry;
import com.android.car.carlauncher.R;
import com.google.android.material.circularreveal.cardview.CircularRevealCardView;
import com.google.android.material.floatingactionbutton.FloatingActionButton;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import java.util.List;

import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.car.media.CarAudioManager;
import android.car.user.CarUserManager;
import android.car.user.CarUserManager.UserLifecycleListener;

/**
 * Main launcher activity. It's launch mode is configured as "singleTop" to allow showing on
 * multiple displays and to ensure a single instance per each display.
 */
public class LauncherActivity extends FragmentActivity implements AppPickedCallback,
        PopupMenu.OnMenuItemClickListener {

    private static final String TAG = "CarLauncher";

    private Spinner mDisplaySpinner;
    private ArrayAdapter<DisplayItem> mDisplayAdapter;
    private int mSelectedDisplayId = Display.INVALID_DISPLAY;
    private View mScrimView;
    private AppListAdapter mAppListAdapter;
    private AppListAdapter mPinnedAppListAdapter;
    private CircularRevealCardView mAppDrawerView;
    private FloatingActionButton mFab;
    private CheckBox mNewInstanceCheckBox;
    private AppListViewModel appListViewModel;
    private PinnedAppListViewModel pinnedAppListViewModel;

    private Car mCarApi;
    private CarOccupantZoneManager mOccupantZoneManager;
    private CarAudioManager mCarAudioManager;
    private CarUserManager mCarUserManager;

    private boolean isUserUnlocked = false;

    private Handler mHandler = new Handler(Looper.getMainLooper());

    private Runnable mGetApp = () -> {
        UserManager mUserManager = (UserManager) getSystemService(USER_SERVICE);
        int currentUserId = getOtherUserId();
        boolean isUnLock = mUserManager.isUserUnlocked(currentUserId);
        if (isUnLock) {
            Log.d(TAG, "getAppList currentUserId = " + currentUserId + " isUnLock = " + isUnLock);
            appListViewModel.setUserId(currentUserId);
            mAppListAdapter.setData(getOtherUserAppData());
        } else {
            Log.d(TAG, "currentUserId: " + currentUserId + " is locked!");
        }
    };

    private boolean mAppDrawerShown;

    private final UserLifecycleListener mUserLifecycleListener = (event) -> {
        Log.d(TAG, "UserLifecycleListener.onEvent: event= " + event);
        if (event.getEventType() == CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED && !isUserUnlocked) {
            int currentUserId = getOtherUserId();
            if (event.getUserId() == currentUserId && !mHandler.hasCallbacks(mGetApp)) {
                Log.d(TAG, "CurrentUserId : " + currentUserId + " is unlocked!");
                mHandler.post(mGetApp);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE)) {
            initCarApi();
        }

        mScrimView = findViewById(R.id.Scrim);
        mAppDrawerView = findViewById(R.id.FloatingSheet);
        mFab = findViewById(R.id.FloatingActionButton);

        mFab.setOnClickListener((View v) -> {
            showAppDrawer(true);
        });

        mScrimView.setOnClickListener((View v) -> {
            showAppDrawer(false);
        });

        mDisplaySpinner = findViewById(R.id.spinner);
        mDisplaySpinner.setOnItemSelectedListener(new OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
                mSelectedDisplayId = mDisplayAdapter.getItem(i).mId;
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                mSelectedDisplayId = Display.INVALID_DISPLAY;
            }
        });
        mDisplayAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item,
                new ArrayList<DisplayItem>());
        mDisplayAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mDisplaySpinner.setAdapter(mDisplayAdapter);

        final ViewModelProvider viewModelProvider = new ViewModelProvider(getViewModelStore(),
                new AndroidViewModelFactory((Application) getApplicationContext()));

        mPinnedAppListAdapter = new AppListAdapter(this);
        final GridView pinnedAppGridView = findViewById(R.id.pinned_app_grid);
        pinnedAppGridView.setAdapter(mPinnedAppListAdapter);
        pinnedAppGridView.setOnItemClickListener((adapterView, view, position, id) -> {
            final AppEntry entry = mPinnedAppListAdapter.getItem(position);
            launch(entry.getLaunchIntent(), entry.getPackageName());
        });
        pinnedAppListViewModel =
                viewModelProvider.get(PinnedAppListViewModel.class);
        pinnedAppListViewModel.getPinnedAppList().observe(this, data -> {
            mPinnedAppListAdapter.setData(data);
        });

        mAppListAdapter = new AppListAdapter(this);
        final GridView appGridView = findViewById(R.id.app_grid);
        appGridView.setAdapter(mAppListAdapter);
        appGridView.setOnItemClickListener((adapterView, view, position, id) -> {
            final AppEntry entry = mAppListAdapter.getItem(position);
            launch(entry.getLaunchIntent(), entry.getPackageName());
        });
        appListViewModel = viewModelProvider.get(AppListViewModel.class);
        appListViewModel.getAppList().observe(this, data -> {
            // mAppListAdapter.setData(data);
            mHandler.post(() -> {
                UserManager mUserManager = (UserManager) getSystemService(USER_SERVICE);
                int otherUserId = getOtherUserId();
                boolean isUnLock = mUserManager.isUserUnlocked(otherUserId);
                Log.d(TAG, "currentUserId = " + otherUserId + ", isUnLock = " + isUnLock);
                if (isUnLock) {
                    isUserUnlocked = true;
                    Log.d(TAG, "getAppList currentUserId = " + otherUserId + " isUnLock = " + isUnLock);
                    appListViewModel.setUserId(otherUserId);
                    mAppListAdapter.setData(getOtherUserAppData());
                } else {
                    Log.d(TAG, "getAppList currentUserId = " + otherUserId + " isUnLock = " + isUnLock);
                }
            });
        });


        findViewById(R.id.RefreshButton).setOnClickListener(this::refreshDisplayPicker);
        mNewInstanceCheckBox = findViewById(R.id.NewInstanceCheckBox);

        ImageButton optionsButton = findViewById(R.id.OptionsButton);
        optionsButton.setOnClickListener((View v) -> {
            PopupMenu popup = new PopupMenu(this, v);
            popup.setOnMenuItemClickListener(this);
            MenuInflater inflater = popup.getMenuInflater();
            inflater.inflate(R.menu.context_menu, popup.getMenu());
            popup.show();
        });

    }

    @Override
    protected void onResume() {
        super.onResume();
        mHandler.post(() -> {
            pinnedAppListViewModel.setUserId(getOtherUserId());
        });
    }

    private void initCarApi() {
        if (mCarApi != null && mCarApi.isConnected()) {
            mCarApi.disconnect();
            mCarApi = null;
        }
        mCarApi = Car.createCar(this, null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                (Car car, boolean ready) -> {
                    if (!ready) {
                        return;
                    }
                    mOccupantZoneManager = (CarOccupantZoneManager) car.getCarManager(CAR_OCCUPANT_ZONE_SERVICE);
                    mCarAudioManager = (CarAudioManager) car.getCarManager(Car.AUDIO_SERVICE);
                    mCarUserManager = (CarUserManager) car.getCarManager(Car.CAR_USER_SERVICE);
                    mCarUserManager.addListener(getMainExecutor(), mUserLifecycleListener);
                });
    }

    @Override
    protected void onDestroy() {
        mHandler.removeCallbacksAndMessages(0);
        if (mCarApi != null) {
            mCarApi.disconnect();
        }
        Log.i(TAG, "onDestroy mCarApi ");
        super.onDestroy();
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        // Respond to picking one of the popup menu items.
        if (item.getItemId() == R.id.add_app_shortcut) {
            FragmentManager fm = getSupportFragmentManager();
            PinnedAppPickerDialog pickerDialogFragment =
                    PinnedAppPickerDialog.newInstance(mAppListAdapter, this);
            pickerDialogFragment.show(fm, "fragment_app_picker");
            return true;
        }/*  else if (item.getItemId() == R.id.set_wallpaper) {
            Intent intent = new Intent(Intent.ACTION_SET_WALLPAPER);
            startActivity(Intent.createChooser(intent, getString(R.string.set_wallpaper)));
            return true;
        }  */else {
            return true;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        showAppDrawer(false);
    }

    public void onBackPressed() {
        // If the app drawer was shown - hide it. Otherwise, not doing anything since we don't want
        // to close the launcher.
        showAppDrawer(false);
    }

    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);

        if (Intent.ACTION_MAIN.equals(intent.getAction())) {
            // Hide keyboard.
            final View v = getWindow().peekDecorView();
            if (v != null && v.getWindowToken() != null) {
                getSystemService(InputMethodManager.class).hideSoftInputFromWindow(
                        v.getWindowToken(), 0);
            }
        }

        // A new intent will bring the launcher to top. Hide the app drawer to reset the state.
        showAppDrawer(false);
    }

    private OccupantZoneInfo getOccupantZoneForDisplay(int displayID) {
        List<OccupantZoneInfo> occupantZoneInfos = mOccupantZoneManager.getAllOccupantZones();
        for (int index = 0; index < occupantZoneInfos.size(); index++) {
            OccupantZoneInfo occupantZoneInfo = occupantZoneInfos.get(index);
            List<Display> displays = mOccupantZoneManager.getAllDisplaysForOccupant(occupantZoneInfo);
            for (int displayIndex = 0; displayIndex < displays.size(); displayIndex++) {
                if (displays.get(displayIndex).getDisplayId() == displayID) {
                    return occupantZoneInfo;
                }
            }
        }
        return null;
    }

    void launch(Intent launchIntent, String packageName) {
        launchIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (mNewInstanceCheckBox.isChecked()) {
            launchIntent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        }
        final ActivityOptions options = ActivityOptions.makeBasic();

        int currentDisplayId = mAppDrawerView.getDisplay().getDisplayId();
        if (mSelectedDisplayId != Display.INVALID_DISPLAY) {
            options.setLaunchDisplayId(mSelectedDisplayId);
            currentDisplayId = mSelectedDisplayId;
        }

        OccupantZoneInfo zoneInfo = getOccupantZoneForDisplay(currentDisplayId);
        if (zoneInfo == null) {
            Log.e(TAG, "Could not start activity, no occupant zone for display");
            return;
        }

        int userId = mOccupantZoneManager.getUserForOccupant(zoneInfo);
        if (userId == UserHandle.USER_NULL) {
            Log.e(TAG, "Could not start activity, no userId for occupant zone");
            return;
        }

        Log.d(TAG, " > display id:" + currentDisplayId + " ; occupantZoneId:" + zoneInfo.zoneId + " ; userId:" + userId);

        try {
            /// Setup Audio Zone
            ///  If mapping occupantZoneId in car_audio_configuration.xml, please disable this code
            // int uid = getApplicationContext().getPackageManager()
            //         .getApplicationInfo(packageName, 0)
            //         .uid;

            // Log.d(TAG, "CarAudioManager: set zoneID:" + zoneInfo.zoneId + " to App:" + packageName + " with uid:" + uid);
            // if (mCarAudioManager.setZoneIdForUid(zoneInfo.zoneId, uid)) {
            //     Log.d(TAG, "Zone successfully updated");
            // } else {
            //     Log.e(TAG, "Failed to change zone");
            // }

            // Start Activity
            Log.d(TAG, "Launch APP in displayid=" + currentDisplayId + " with userid=" + userId);
            startActivityAsUser(launchIntent, options.toBundle(), UserHandle.of(userId));

        } catch (Exception e) {
            final AlertDialog.Builder builder =
                    new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert);
            builder.setTitle(R.string.couldnt_launch)
                    .setMessage(e.getLocalizedMessage())
                    .setIcon(android.R.drawable.ic_dialog_alert)
                    .show();
        }
    }

    private void refreshDisplayPicker() {
        refreshDisplayPicker(mAppDrawerView);
    }

    private void refreshDisplayPicker(View view) {
        final int currentDisplayId = view.getDisplay().getDisplayId();
        final DisplayManager dm = getSystemService(DisplayManager.class);
        mDisplayAdapter.setNotifyOnChange(false);
        mDisplayAdapter.clear();
        mDisplayAdapter.add(new DisplayItem(Display.INVALID_DISPLAY, "Do not specify display"));

        for (Display display : dm.getDisplays()) {
            final int id = display.getDisplayId();
            final boolean isDisplayPrivate = (display.getFlags() & Display.FLAG_PRIVATE) != 0;
            final boolean isCurrentDisplay = id == currentDisplayId;
            final StringBuilder sb = new StringBuilder();
            sb.append(id).append(": ").append(display.getName());
            if (isDisplayPrivate) {
                sb.append(" (private)");
            }
            if (isCurrentDisplay) {
                sb.append(" [Current display]");
            }
            mDisplayAdapter.add(new DisplayItem(id, sb.toString()));
        }

        mDisplayAdapter.notifyDataSetChanged();
    }

    /**
     * Store the picked app to persistent pinned list and update the loader.
     */
    @Override
    public void onAppPicked(AppEntry appEntry) {
        final SharedPreferences sp = getSharedPreferences(PINNED_APPS_KEY, 0);
        Set<String> pinnedApps = sp.getStringSet(getOtherUserId() + "-" + PINNED_APPS_KEY, null);
        if (pinnedApps == null) {
            pinnedApps = new HashSet<String>();
        } else {
            // Always need to create a new object to make sure that the changes are persisted.
            pinnedApps = new HashSet<String>(pinnedApps);
        }
        pinnedApps.add(appEntry.getComponentName().flattenToString());

        final SharedPreferences.Editor editor = sp.edit();
        editor.putStringSet(getOtherUserId() + "-" + PINNED_APPS_KEY, pinnedApps);
        editor.apply();
    }

    /**
     * Show/hide app drawer card with animation.
     */
    private void showAppDrawer(boolean show) {
        if (show == mAppDrawerShown) {
            return;
        }

        final Animator animator = revealAnimator(mAppDrawerView, show);
        if (show) {
            mAppDrawerShown = true;
            mAppDrawerView.setVisibility(View.VISIBLE);
            mScrimView.setVisibility(View.VISIBLE);
            mFab.setVisibility(View.INVISIBLE);
            refreshDisplayPicker();
        } else {
            mAppDrawerShown = false;
            mScrimView.setVisibility(View.INVISIBLE);
            animator.addListener(new AnimatorListenerAdapter() {
                @Override
                public void onAnimationEnd(Animator animation) {
                    super.onAnimationEnd(animation);
                    mAppDrawerView.setVisibility(View.INVISIBLE);
                    mFab.setVisibility(View.VISIBLE);
                }
            });
        }
        animator.start();
    }

    /**
     * Create reveal/hide animator for app list card.
     */
    private Animator revealAnimator(View view, boolean open) {
        final int radius = (int) Math.hypot((double) view.getWidth(), (double) view.getHeight());
        return ViewAnimationUtils.createCircularReveal(view, view.getRight(), view.getBottom(),
                open ? 0 : radius, open ? radius : 0);
    }

    private static class DisplayItem {
        final int mId;
        final String mDescription;

        DisplayItem(int displayId, String description) {
            mId = displayId;
            mDescription = description;
        }

        @Override
        public String toString() {
            return mDescription;
        }
    }

    //Get App Data with queryIntentActivitiesAsUser in secondry launcher
    private List<AppEntry> getOtherUserAppData() {
        if (mAppDrawerView.getDisplay() == null) return null;
        int currentDisplayId = mAppDrawerView.getDisplay().getDisplayId();
        if (mSelectedDisplayId != Display.INVALID_DISPLAY) {
            currentDisplayId = mSelectedDisplayId;
        }

        OccupantZoneInfo zoneInfo = getOccupantZoneForDisplay(currentDisplayId);
        if (zoneInfo == null) {
            Log.e(TAG, "currentDisplayId don't get zoneInfo");
            return null;
        }

        int userId = mOccupantZoneManager.getUserForOccupant(zoneInfo);
        if (userId == UserHandle.USER_NULL) {
            Log.e(TAG, "zoneInfo don't get userId");
            return null;
        }

        Intent mainIntent = new Intent(Intent.ACTION_MAIN, null);
        mainIntent.addCategory(Intent.CATEGORY_LAUNCHER);

        List<ResolveInfo> apps = getPackageManager().queryIntentActivitiesAsUser(mainIntent,
                PackageManager.GET_META_DATA, userId);

        List<AppEntry> entries = new ArrayList<>();
        if (apps != null) {
            for (ResolveInfo app : apps) {
                AppEntry entry = new AppEntry(app, getPackageManager());
                entries.add(entry);
                Log.d(TAG, "currentDisplayId = " + currentDisplayId + ", userId = " + userId + ", getOtherUserAppData entry = " + entry.toString());
            }
        }
        return entries;
    }

    private int getOtherUserId() {
        if (getDisplay() == null) return -1;
        int currentDisplayId = getDisplay().getDisplayId();
        if (mSelectedDisplayId != Display.INVALID_DISPLAY) {
            currentDisplayId = mSelectedDisplayId;
        }

        OccupantZoneInfo zoneInfo = getOccupantZoneForDisplay(currentDisplayId);
        if (zoneInfo == null) {
            Log.e(TAG, "currentDisplayId don't get zoneInfo");
            return -1;
        }

        int userId = mOccupantZoneManager.getUserForOccupant(zoneInfo);
        if (userId == UserHandle.USER_NULL) {
            Log.e(TAG, "zoneInfo don't get userId");
            return -1;
        }
        Log.d(TAG, "getOtherUserId currentDisplayId = " + currentDisplayId + ", userId = " + userId);
        return userId;
    }
}
