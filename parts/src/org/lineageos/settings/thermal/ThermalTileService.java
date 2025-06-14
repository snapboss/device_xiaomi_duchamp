/*
 * Copyright (C) 2024 The LineageOS Project
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

package org.lineageos.settings.thermal;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.PowerManager;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.util.Log;
import android.view.View;

import androidx.preference.PreferenceManager;

import org.lineageos.settings.R;
import org.lineageos.settings.utils.FileUtils;

public class ThermalTileService extends TileService {
    private static final String TAG = "ThermalTileService";
    private static final String THERMAL_SCONFIG = "/sys/class/thermal/thermal_message/sconfig";
    private static final String THERMAL_ENABLED_KEY = "thermal_enabled";

    private String[] modes;
    private int currentMode = 0; // Default mode index
    private SharedPreferences mSharedPrefs;
    private NotificationManager mNotificationManager;
    private Notification mNotification;
    
    @Override
    public void onCreate() {
        super.onCreate();
        mSharedPrefs = PreferenceManager.getDefaultSharedPreferences(this);
        mNotificationManager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        // Ensure a default value for the master switch
        if (!mSharedPrefs.contains(THERMAL_ENABLED_KEY)) {
            mSharedPrefs.edit().putBoolean(THERMAL_ENABLED_KEY, false).apply();
        }
        setupNotificationChannel();
    }

    @Override
    public void onStartListening() {
        super.onStartListening();
        modes = new String[]{
                getString(R.string.thermal_mode_default),
                getString(R.string.thermal_mode_performance),
                getString(R.string.thermal_mode_gaming),
                getString(R.string.thermal_mode_battery_saver),
                getString(R.string.thermal_mode_unknown)
        };

        // Check the state of the master switch
        boolean isMasterEnabled = mSharedPrefs.getBoolean(THERMAL_ENABLED_KEY, false);
        if (isMasterEnabled) {
            updateTileDisabled();
        } else {
            currentMode = getCurrentThermalMode();
            // Reset to Default if mode is Unknown
            if (currentMode == 4) {
                currentMode = 0;
                setThermalMode(currentMode);
            }
            updateTile();
        }
    }

    @Override
    public void onClick() {
        boolean isMasterEnabled = mSharedPrefs.getBoolean(THERMAL_ENABLED_KEY, false);
        if (isMasterEnabled) {
            // Tile is disabled; ignore click events
            return;
        }
        toggleThermalMode();
    }

    private void toggleThermalMode() {
        if (currentMode == 4) {
            // If in Unknown mode, reset to Default
            currentMode = 0;
        } else {
            // Cycle through the order: Default → Performance → Battery Saver → Default
            currentMode = (currentMode + 1) % 4;
        }
        setThermalMode(currentMode);
        updateTile();
    }

    private int getCurrentThermalMode() {
        String line = FileUtils.readOneLine(THERMAL_SCONFIG);
        if (line != null) {
            try {
                int value = Integer.parseInt(line.trim());
                switch (value) {
                    case 0: return 0; // Default
                    case 6: return 1; // Performance
                    case 19: return 2; // Gaming
                    case 1: return 3; // Battery Saver
                    default: return 4; // Unknown mode
                }
            } catch (NumberFormatException e) {
                Log.e(TAG, "Error parsing thermal mode value: ", e);
            }
        }
        return 3; // Treat invalid or missing values as Unknown
    }

	private void setThermalMode(int mode) {
	    int thermalValue;
	    switch (mode) {
        	case 0: thermalValue = 0; break;  // Default
        	case 1: thermalValue = 6; break;  // Performance
        	case 2: thermalValue = 19; break; // Gaming
	        case 3: thermalValue = 1; break;  // Battery Saver
	        default: thermalValue = 0; break; // Reset to Default for Unknown
	    }

	    boolean success = FileUtils.writeLine(THERMAL_SCONFIG, String.valueOf(thermalValue));
	    Log.d(TAG, "Thermal mode changed to " + modes[mode] + ": " + success);

	    // Show or cancel performance notification
	    if (mode == 1) { // Performance mode
	        showPerformanceNotification();
	    } else {
	        cancelPerformanceNotification();
	    }
	}


    private void updateTile() {
        Tile tile = getQsTile();
        if (tile != null) {
            // Set tile state based on current mode
            if (currentMode == 1 || currentMode == 2 || currentMode == 3) { // Performance or Gaming
                tile.setState(Tile.STATE_ACTIVE);
            } else {
                tile.setState(Tile.STATE_INACTIVE);
            }
            // Update label and subtitle based on current mode
            tile.setLabel(getString(R.string.thermal_tile_label));
            tile.setSubtitle(modes[currentMode]);
            tile.updateTile();
        }
    }

    private void updateTileDisabled() {
        Tile tile = getQsTile();
        if (tile != null) {
            tile.setState(Tile.STATE_UNAVAILABLE); // Tile is greyed out
            tile.setLabel(getString(R.string.thermal_tile_label));
            tile.setSubtitle(getString(R.string.thermal_tile_disabled_subtitle));
            tile.updateTile();
        }
    }

    private void setupNotificationChannel() {
        NotificationChannel channel = new NotificationChannel(TAG, getString(R.string.perf_mode_title), NotificationManager.IMPORTANCE_DEFAULT);
        channel.setBlockable(true);
        mNotificationManager.createNotificationChannel(channel);
    }

    private void showPerformanceNotification() {
        Intent intent = new Intent(Intent.ACTION_POWER_USAGE_SUMMARY).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, intent, PendingIntent.FLAG_IMMUTABLE);
        mNotification = new Notification.Builder(this, TAG)
                .setContentTitle(getString(R.string.perf_mode_title))
                .setContentText(getString(R.string.perf_mode_notification))
                .setSmallIcon(R.drawable.ic_thermal_tile)
                .setContentIntent(pendingIntent)
                .setOngoing(true)
                .setFlag(Notification.FLAG_NO_CLEAR, true)
                .build();
        mNotificationManager.notify(1, mNotification);
    }

    private void cancelPerformanceNotification() {
        mNotificationManager.cancel(1);
    }
}
