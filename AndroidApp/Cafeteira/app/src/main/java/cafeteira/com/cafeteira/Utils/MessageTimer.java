package cafeteira.com.cafeteira.Utils;

import android.util.Log;
import android.widget.Toast;

import java.util.Timer;
import java.util.TimerTask;

import cafeteira.com.cafeteira.Activity.MainActivityCafeteira;
import cafeteira.com.cafeteira.R;

public class MessageTimer {

    private static long TIMER_TASK = 10000;
    private static MessageTimer message = null;
    private MainActivityCafeteira mActivityCafeteira;

    public Timer timer;

    public MessageTimer(MainActivityCafeteira activityCafeteira) {
        this.mActivityCafeteira = activityCafeteira;
    }

    public static MessageTimer getInstance(MainActivityCafeteira activityCafeteira) {
        if (message == null) {
            message = new MessageTimer(activityCafeteira);
        }
        return message;
    }

    public void startTimerTask() {
        Log.d("DEBUG", "Iniciando o timer...");
        timer = new Timer();

        TimerTask timerTask = new TimerTask() {
            @Override
            public void run() {
                cancelTimerTask();
                mActivityCafeteira.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        ToastMessage.setToastMessage(mActivityCafeteira.getApplicationContext(),
                                mActivityCafeteira.getResources().
                                        getString(R.string.connection_error), Toast.LENGTH_SHORT);
                        mActivityCafeteira.turnOffSwitch();
                        mActivityCafeteira.setmConnectionDragonboard(false);
                    }
                });
            }
        };

        timer.scheduleAtFixedRate(timerTask, TIMER_TASK, TIMER_TASK);
    }

    /**
     * Cancel timer task.
     */
    public void cancelTimerTask() {
        if (timer != null) {
            timer.cancel();
        }
    }
}
