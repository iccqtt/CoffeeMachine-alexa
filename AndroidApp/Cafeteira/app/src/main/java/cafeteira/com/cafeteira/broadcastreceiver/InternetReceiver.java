package cafeteira.com.cafeteira.broadcastreceiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import cafeteira.com.cafeteira.Utils.InternetUtils;

/**
 *
 */
public class InternetReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        InternetUtils.hasInternetConnection(context);
    }
}