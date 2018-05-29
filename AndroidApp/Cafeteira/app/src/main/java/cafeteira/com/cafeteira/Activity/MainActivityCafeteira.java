package cafeteira.com.cafeteira.Activity;

import android.Manifest;
import android.app.Activity;
import android.app.Application;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.ProgressDialog;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.content.DialogInterface;
import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.speech.RecognizerIntent;
import android.support.v4.app.ActivityCompat;
import android.support.v4.app.NotificationCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.Locale;

import cafeteira.com.cafeteira.Activity.Fragment.StatusDialog;
import cafeteira.com.cafeteira.Controller.AWSConnection;
import cafeteira.com.cafeteira.R;
import cafeteira.com.cafeteira.Utils.CoffeeMachinePreferences;
import cafeteira.com.cafeteira.Utils.Constants;
import cafeteira.com.cafeteira.Utils.InternetUtils;
import cafeteira.com.cafeteira.Utils.MessageTimer;
import cafeteira.com.cafeteira.Utils.ToastMessage;
import cafeteira.com.cafeteira.broadcastreceiver.InternetReceiver;

import static android.app.PendingIntent.FLAG_UPDATE_CURRENT;

public class MainActivityCafeteira
        extends AppCompatActivity
        implements CompoundButton.OnCheckedChangeListener {

    //Notification IDs
    private static final int SHORT_COFFEE_ID = 001;
    private static final int LONG_COFFEE_ID = 002;
    private static final int IN_PROGRESS_ID = 003;

    //Activity status callbacks
    private final Application.ActivityLifecycleCallbacks mActivityCallbacks =
            new ActivityLifecycleCallbackReceiver();

    //Activity status callbacks variables
    private boolean hasActivityPaused, hasActivityStopped, hasActivityResumed;

    //Internet Receiver
    private InternetReceiver mInternetReceiver;

    //Tag to debug
    private static final String LOG_TAG = MainActivityCafeteira.class.getCanonicalName();

    //Turns on/off the Coffee Maker
    private Switch mOnOff;

    //Provides the Activity context
    private Context mContext;

    //String to store the Toast text
    private String mText = "";

    //Dialog and Notification IDs
    private int mShortCoffeeNotifId, mLongCoffeeNotifId, mCoffeeProgressNotifId;

    //NotificationBuilder to create the notifications
    private NotificationCompat.Builder mBuilder;

    //Short and long coffee buttons
    private Button mShortCoffeeButton, mLongCoffeeButton;

    //Progress bar to indicate the coffee level
    private ProgressBar mCoffeeProgressBar;

    //Value of coffee's progress bar
    private int mCoffeeProgressStatus = 0;

    //Boolean to check if the level is enough to make coffee
    private boolean mProgressBarPermission = false;

    //Handler to update the progress bar
    private Handler mCoffeeHandler;

    //TextViews used to indicate the status of each reservoir
    private TextView mCoffeeProgressText, mWaterProgressText, mGlassStatusText;

    //ImageView to show if the cup was positioned
    private ImageView mGlassStatusImage;

    //NotificationManager to manage the notifications
    private NotificationManager mNotifyMgr;

    //Boolean to indicate if the cup was positioned
    private boolean mGlassStatus;

    //Handler used to implement the ProgressDialog timer
    private Handler mHandlerTimer;

    //Values to set the preparation time of each kind of coffee
    private int mShortCoffeeTotalTime = 20;
    private int mLongCoffeeTotalTime = 30;

    //Value to count the time to finish ProgressDialog
    private int mTotalTime;

    //ProgressDialog after clicking on a coffee button
    private ProgressDialog mCoffeeProgressDialog;

    //AWS connection manager object
    private AWSConnection mAWSConnection;

    //Timer class instance
    private MessageTimer mMessageTimer;

    //Boolean to check if Bluetooth Low Energy is connected
    private boolean mBluetoothIsConnected = true;

    //Boolean to check if the coffee requested was finished
    private boolean mCoffeeIsFinished = false;

    //Boolean to check if the coffee is in progress
    private boolean mCoffeeInProgress = false;

    //Boolean to check if the app is being launched for the first time
    private boolean mIsFirstLaunch = true;

    //Boolean to check if the permission was granted or not
    private boolean mIsPermissionDenied = false;

    //Status Layout
    private LinearLayout mStatusLayout;

    private ImageView mImageStatusError;

    //Status with dragonboard
    private boolean mConnectionDragonboard = false;

    //Speech button.
    private ImageButton mSpeechButton;

    /**
     * Android onCreate method to call actions when the Activity is launched.
     *
     * @param savedInstanceState
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initializeCallbacks();
        initializeComponents();
        initInternetReceiver();
    }

    /**
     * Android onDestroy method called right before the application is closed.
     */
    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mNotifyMgr != null) mNotifyMgr.cancelAll();
        unregisterReceiver(mInternetReceiver);
        getApplication().unregisterActivityLifecycleCallbacks(mActivityCallbacks);
    }

    /**
     * Method to initialize all UI components.
     */
    private void initializeComponents() {
        mContext = getApplicationContext();
        setContentView(R.layout.activity_main_cafeteira);

        startSpeechButton();
        startSwitch();
        startNotificationModel();
        startCoffeeButtons();
        startConnectionAWS();
        startCheckStatusComponets();
        getTimerInstance();

        mOnOff.setEnabled(false);
        mShortCoffeeButton.setEnabled(false);
        mLongCoffeeButton.setEnabled(false);
        mSpeechButton.setEnabled(false);
        mSpeechButton.setAlpha(0.1f);

    }

    private void startSpeechButton() {
        mSpeechButton = (ImageButton) findViewById(R.id.button_speech);
        mSpeechButton.setOnClickListener(new View.OnClickListener() {

            @Override
            public void onClick(View v) {
                mSpeechButton.setAlpha(0.1f);
                Intent intent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
                intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,
                        RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
                intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE, Locale.getDefault());
                intent.putExtra(RecognizerIntent.EXTRA_PROMPT,
                        getString(R.string.speech_prompt));
                try {
                    startActivityForResult(intent, Constants.REQ_CODE_SPEECH_INPUT);
                } catch (ActivityNotFoundException a) {
                    Toast.makeText(getApplicationContext(),
                            getString(R.string.speech_not_supported),
                            Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        mSpeechButton.setAlpha(1.0f);
        switch (requestCode) {
            case Constants.REQ_CODE_SPEECH_INPUT: {
                if (resultCode == RESULT_OK && null != data) {
                    ArrayList<String> result = data
                            .getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS);
                    String resultTranslate = result.get(0);
                    if(resultTranslate.contains(getString(R.string.turn_on_coffee_machine))  || resultTranslate.contains(getString(R.string.turn_coffee_machine_on)) ||
                            resultTranslate.contains(getString(R.string.on_coffee_machine)) ) {
                        mOnOff.setChecked(true);

                    } else  if(resultTranslate.contains(getString(R.string.turn_off_coffee_machine))
                            || resultTranslate.contains(getString(R.string.turn_coffee_machine_off))
                            || resultTranslate.contains(getString(R.string.off_coffee_machine))) {
                        mOnOff.setChecked(false);
                    } else if(resultTranslate.contains(getString(R.string.make_long_coffee))) {
                        mLongCoffeeButton.callOnClick();

                    } else if(resultTranslate.contains(getString(R.string.make_short_coffee))) {
                        mShortCoffeeButton.callOnClick();
                    } else {
                        ToastMessage.setToastMessage(mContext,
                                getString(R.string.voice_not_recognized),
                                Toast.LENGTH_LONG);
                    }
                }
                break;
            }
        }
    }

    /**
     * Method to start the Internet receiver and check if there is Internet connection.
     */
    private void initInternetReceiver() {
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction("android.net.conn.CONNECTIVITY_CHANGE");
        mInternetReceiver = new InternetReceiver();
        registerReceiver(mInternetReceiver, intentFilter);
    }

    /**
     * Method to get all ActivityLifecycleCallbacks.
     */
    public void initializeCallbacks() {
        getApplication().registerActivityLifecycleCallbacks(mActivityCallbacks);
    }

    /**
     * Method to connect with AWS if there is Internet connection.
     */
    private void startConnectionAWS() {
        mAWSConnection = AWSConnection.getInstance(this);
        if (InternetUtils.hasInternetConnection(this)) {
            mAWSConnection.getConnection();
        }
        mAWSConnection.setLastStatus();
    }

    /**
     * Method to get MessageTimer instance using Singleton pattern.
     */
    private void getTimerInstance() {
        mMessageTimer = MessageTimer.getInstance(this);
    }

    /**
     * Method to set the dialog according to the status of the coffee. This method is called when
     * a coffee is requested or finished.
     *
     * @param status is a variable which indicates if a coffee is ready or not and also notifies if
     *               an error occurs.
     */
    private void coffeeReadyDialog(int status) {
        AlertDialog.Builder readyDialog = new AlertDialog.Builder(MainActivityCafeteira.this);

        if (status == 0) { //error
            readyDialog.setTitle(R.string.ops).setMessage(R.string.connection_error);
        } else if (status == 1) { //coffee ready
            readyDialog.setTitle(R.string.coffee_ready).setMessage(R.string.coffee_ready_message);
        } else if (status == 2) { //coffee in progress
            readyDialog.setTitle(getString(R.string.app_name)).
                    setMessage(getString(R.string.machine_in_use));
        }

        readyDialog.setCancelable(false)
                .setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialogInterface, int i) {
                        //When OK is pressed, all notifications are closed
                        if (mNotifyMgr != null) mNotifyMgr.cancelAll();
                    }
                });
        AlertDialog dialog = readyDialog.create();
        dialog.show();
    }

    /**
     * Method to set the switch according to the Coffee Maker status. If the Coffee Maker is turned
     * on by another phone, the status is updated on the whole connected devices.
     *
     * @param status is a String which tells if the Coffee Maker is on or off.
     */
    public void setCheckSwitch(String status) {
        mOnOff.setClickable(true);
        if ((status.equals(Constants.ON))) {
            if ((CoffeeMachinePreferences.getTurnOnOff(getApplicationContext()).equals(Constants.OFF))) {
                mOnOff.setOnCheckedChangeListener(null);
                mOnOff.setChecked(true);
                mOnOff.setOnCheckedChangeListener(this);
                ToastMessage.setToastMessage(mContext,
                        getString(R.string.coffee_machine_on),
                        Toast.LENGTH_LONG);
            } else if (mIsFirstLaunch && (status.equals(Constants.ON)) &&
                    (!mOnOff.isChecked())) {
                mOnOff.setOnCheckedChangeListener(null);
                mOnOff.setChecked(true);
                mOnOff.setOnCheckedChangeListener(this);
                ToastMessage.setToastMessage(mContext,
                        getString(R.string.coffee_machine_on),
                        Toast.LENGTH_LONG);
            }
        } else if ((status.equals(Constants.OFF))) {
            if ((CoffeeMachinePreferences.getTurnOnOff(getApplicationContext()).equals(Constants.ON))) {
                mOnOff.setOnCheckedChangeListener(null);
                mOnOff.setChecked(false);
                mOnOff.setOnCheckedChangeListener(this);
                ToastMessage.setToastMessage(mContext,
                        getString(R.string.coffee_machine_off),
                        Toast.LENGTH_LONG);
            } else if (mIsFirstLaunch) {
                mOnOff.setOnCheckedChangeListener(null);
                mOnOff.setChecked(false);
                mOnOff.setOnCheckedChangeListener(this);
                ToastMessage.setToastMessage(mContext,
                        getString(R.string.coffee_machine_off),
                        Toast.LENGTH_LONG);
            }
        } else if (status.equals("busy")) {
            mOnOff.setChecked(true);
        }
        if (mIsFirstLaunch && !mIsPermissionDenied) {
            enableComponents();
            mIsFirstLaunch = false;
        }
    }

    /**
     * Method which enables all the components to allow the user to use the Coffee Maker.
     */
    public void enableComponents() {
        mOnOff.setEnabled(true);
        mShortCoffeeButton.setEnabled(true);
        mLongCoffeeButton.setEnabled(true);
        mSpeechButton.setEnabled(true);
        mSpeechButton.setAlpha(1.0f);
    }

    /**
     * Method which disables all the components to do not allow the user to use the Coffee Maker;
     */
    public void disableComponents() {
        mOnOff.setEnabled(false);
        mShortCoffeeButton.setEnabled(false);
        mLongCoffeeButton.setEnabled(false);
        mSpeechButton.setEnabled(false);
        mSpeechButton.setAlpha(0.1f);
    }

    /**
     * Method to instantiate the Switch which turns on/off the Coffee Maker.
     */
    private void startSwitch() {
        mOnOff = (Switch) findViewById(R.id.on_off_switch);
        mOnOff.setClickable(false);
        mOnOff.setOnCheckedChangeListener(this);
    }

    public void startCheckStatusComponets() {
        mStatusLayout = (LinearLayout) findViewById(R.id.statusLayout);
        mImageStatusError = (ImageView) findViewById(R.id.status);

        mStatusLayout.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                StatusDialog dialog = new StatusDialog();
                Bundle bundle = new Bundle();
                if(!hasBluetoothConnection()) {
                    bundle.putString(Constants.STATUS_TAG, getString(R.string.ble_error_message));
                }
                else if(!hasConnectionDragonboard()) {
                    bundle.putString(Constants.STATUS_TAG, getString(R.string.wifi_error_message));
                } else {
                    bundle.putString(Constants.STATUS_TAG, getString(R.string.connections_ok));
                }

                dialog.setArguments(bundle);
                dialog.show(getFragmentManager(), null);
            }
        });

    }

    /**
     * Method to check (inside the application) the Bluetooth connection.
     *
     * @return the status of the Bluetooth connection.
     */
    public boolean hasBluetoothConnection() {
        return mBluetoothIsConnected;
    }

    /**
     * set value to mConnectionDragonboard
     * @param status
     */
    public void setmConnectionDragonboard(boolean status) {
        mConnectionDragonboard = status;
        setImageError();
    }

    /**
     * get value of mConnectionDragonboard
     * @return
     */
    public boolean hasConnectionDragonboard() {
        return mConnectionDragonboard;
    }

    public void setImageError() {
        if(hasBluetoothConnection() && hasConnectionDragonboard()) {
            mImageStatusError.setImageDrawable(getDrawable(R.drawable.status_round_green));
            mStatusLayout.setClickable(false);
        } else {
            mImageStatusError.setImageDrawable(getDrawable(R.drawable.status_round_red));
            mStatusLayout.setClickable(true);
        }
    }

    /**
     * Method to instantiate and set the OnClickListeners for each coffee button. When a button is
     * pressed, a message is sent on TOPIC_SHORT_COFFE or TOPIC_LONG_COFFE to notify the other
     * users that coffee is being made and to wait for an answer from the DragonBoard.
     */
    private void startCoffeeButtons() {
        mShortCoffeeButton = (Button) findViewById(R.id.short_coffee_button);
        mLongCoffeeButton = (Button) findViewById(R.id.long_coffee_button);

        mShortCoffeeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {

                if (isEverythingOK()) {
                    mOnOff.setEnabled(false);
                    mLongCoffeeButton.setEnabled(false);
                    mSpeechButton.setEnabled(false);
                    mSpeechButton.setAlpha(0.1f);
                    coffeeProgressDialog(mShortCoffeeNotifId);

                    Log.d(LOG_TAG, "O botão de café curto foi pressionado.");
                    mAWSConnection.topicPublish(Constants.SHORT, Constants.TOPIC_MAKE_COFFEE + Constants.ANDROID);
                }
            }
        });

        mLongCoffeeButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (isEverythingOK()) {
                    mOnOff.setEnabled(false);
                    mShortCoffeeButton.setEnabled(false);
                    mSpeechButton.setEnabled(false);
                    mSpeechButton.setAlpha(0.1f);
                    coffeeProgressDialog(mLongCoffeeNotifId);
                    Log.d(LOG_TAG, "O botão de café longo foi pressionado.");
                    mAWSConnection.topicPublish(Constants.LONG, Constants.TOPIC_MAKE_COFFEE + Constants.ANDROID);
                }
            }
        });
    }

    /**
     * Method to instantiate and update the ProgressBar which displays the amount of coffee
     * available. The app will only accept coffee requests if mCoffeeProgressStatus is above 5%.
     *
     * @param data variable where the message containing the progress value is stored.
     */
    public void startCoffeeProgressBar(final int data) {
        mCoffeeProgressBar = (ProgressBar) findViewById(R.id.coffee_progress_bar);
        mCoffeeHandler = new Handler();
        mCoffeeProgressText = (TextView) findViewById(R.id.coffee_status_text);

        new Thread(new Runnable() {
            public void run() {
                mProgressBarPermission = true;

                mCoffeeProgressStatus = data;

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {

                        if (mCoffeeProgressStatus <= 100 && mCoffeeProgressStatus >= 50) {
                            mCoffeeProgressBar.getProgressDrawable().
                                    setColorFilter(ContextCompat.getColor(getApplicationContext()
                                            , R.color.progressGreen),
                                            PorterDuff.Mode.SRC_IN);
                        } else if (mCoffeeProgressStatus < 50 && mCoffeeProgressStatus >= 20) {
                            mCoffeeProgressBar.getProgressDrawable().
                                    setColorFilter(ContextCompat.getColor(getApplicationContext()
                                            , R.color.progressYellow),
                                            PorterDuff.Mode.SRC_IN);
                        } else if (mCoffeeProgressStatus < 20 && mCoffeeProgressStatus >= 5) {
                            mCoffeeProgressBar.getProgressDrawable().
                                    setColorFilter(ContextCompat.getColor(getApplicationContext()
                                            , R.color.progressRed),
                                            PorterDuff.Mode.SRC_IN);
                        } else {
                            mCoffeeProgressBar.getProgressDrawable().
                                    setColorFilter(ContextCompat.getColor(getApplicationContext()
                                            , R.color.progressRed),
                                            PorterDuff.Mode.SRC_IN);
                            mProgressBarPermission = false;
                        }

                        mCoffeeProgressText.setText(mCoffeeProgressStatus + "%");
                    }
                });

                //Updates the progress bar
                mCoffeeHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        mCoffeeProgressBar.setProgress(mCoffeeProgressStatus);
                    }
                });
            }
        }).start();
    }

    /**
     * Method to set the TextView which notifies the user about the status of the water reservoir.
     *
     * @param waterProgressStatus is the variable which contains the status of the water reservoir.
     *                            It can be FULL, or EMPTY, according to what is sent by the
     *                            Dragonboard.
     */
    public void startWaterStatus(int waterProgressStatus) {
        mWaterProgressText = (TextView) findViewById(R.id.water_status_text);

        if (waterProgressStatus == Constants.FULL) {
            mWaterProgressText.setText(R.string.full);
            mWaterProgressText.setTextColor(ContextCompat.getColor(this, R.color.progressGreen));
        } else if (waterProgressStatus == Constants.EMPTY) {
            mWaterProgressText.setText(R.string.empty);
            mWaterProgressText.setTextColor(ContextCompat.getColor(this, R.color.progressRed));
        }
    }

    /**
     * Method to create the NotificationCompat.Builder object and set a notification model to use
     * on the app notifications.
     */
    private void startNotificationModel() {
        mShortCoffeeNotifId = SHORT_COFFEE_ID;
        mLongCoffeeNotifId = LONG_COFFEE_ID;
        mCoffeeProgressNotifId = IN_PROGRESS_ID;

        mBuilder = new NotificationCompat.Builder(MainActivityCafeteira.this)
                .setSmallIcon(R.mipmap.icon_launcher)
                .setContentTitle(getString(R.string.app_name))
                .setAutoCancel(true);

        //Pending Intent is used to define the action that will occur after clicking on notification
        Intent resultIntent = new Intent(this, MainActivityCafeteira.class);

        PendingIntent resultPendingIntent =
                PendingIntent.getActivity(
                        this,
                        0,
                        resultIntent,
                        FLAG_UPDATE_CURRENT
                );

        mBuilder.setContentIntent(resultPendingIntent);
    }

    /**
     * Method to send a notification according to its ID. The variables mCoffeeInProgress and
     * mCoffeeIsFinished are set as false to indicate that the progress dialog was dismissed and
     * the process can be started again.
     *
     * @param mNotifId is a variable which contains the notification ID. The values can be:
     *                 - mShortCoffeeNotifId    = SHORT_COFFEE_ID
     *                 - mLongCoffeeNotifId     = LONG_COFFEE_ID
     *                 - mCoffeeProgressNotifId = IN_PROGRESS_ID
     */
    private void sendNotification(int mNotifId) {
        mCoffeeInProgress = false;
        mCoffeeIsFinished = false;

        mNotifyMgr = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        mNotifyMgr.notify(mNotifId, mBuilder.build());
    }

    /**
     * Method to instantiate and set the UI elements related to the cup status indication.
     *
     * @param status is a variable to notify the cup status. It can be POSITIONED or NOT_POSITIONED.
     */
    public void startGlassStatus(int status) {
        mGlassStatusText = (TextView) findViewById(R.id.glass_indicator_text);
        mGlassStatusImage = (ImageView) findViewById(R.id.glass_indicator_image);

        if (status == Constants.POSITIONED) {
            mGlassStatus = true;
        } else if (status == Constants.NOT_POSITIONED) {
            mGlassStatus = false;
        }

        if (mGlassStatus) {
            //If the cup is positioned
            mGlassStatusText.setText(R.string.yes_glass);
            mGlassStatusText.setTextColor(ContextCompat.getColor(this, R.color.progressGreen));

            mGlassStatusImage.setAlpha(1.0f);
        } else {
            //If the cup is not positioned
            mGlassStatusText.setText(R.string.no_glass);
            mGlassStatusText.setTextColor(ContextCompat.getColor(this, R.color.progressRed));

            mGlassStatusImage.setAlpha(0.1f);
        }
    }

    /**
     * Method to instantiate and set the dialog which indicates that the coffee is being prepared.
     * Then, the Runnable class is called to start the timer for each kind of coffee and when the
     * coffee is finished, the notification and the dialog are set according to what was asked to
     * the user.
     *
     * @param id is a variable to indicate what kind of information will be handled on the dialog
     *           and the notification.
     */
    private void coffeeProgressDialog(int id) {
        final int coffeeId = id;

        mCoffeeProgressDialog = new ProgressDialog(this);
        mCoffeeProgressDialog.setTitle(R.string.app_name);

        switch (coffeeId) {
            case SHORT_COFFEE_ID:
                mTotalTime = mShortCoffeeTotalTime;
                mCoffeeProgressDialog.setMessage(getString(R.string.making_short_coffee));
                break;
            case LONG_COFFEE_ID:
                mTotalTime = mLongCoffeeTotalTime;
                mCoffeeProgressDialog.setMessage(getString(R.string.making_long_coffee));
                break;
            default:
                break;
        }

        mCoffeeProgressDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);
        mCoffeeProgressDialog.setCancelable(false);
        mCoffeeProgressDialog.show();

        mBuilder.setContentText(getString(R.string.making_coffee));
        mBuilder.setOngoing(false);

        sendNotification(mCoffeeProgressNotifId);

        mHandlerTimer = new Handler();
        Runnable runnable = new Runnable() {
            @Override
            public void run() {
                try {
                    mTotalTime--;

                    if (wasCoffeeFinished()) {
                        //Coffee ready
                        mCoffeeProgressDialog.dismiss();
                        coffeeReadyDialog(1); //Notifies that the coffee is ready

                        if (mNotifyMgr != null) mNotifyMgr.cancelAll();

                        if (coffeeId == SHORT_COFFEE_ID) mBuilder.
                                setContentText(getString(R.string.short_coffee_ready));
                        else if (coffeeId == LONG_COFFEE_ID) mBuilder.
                                setContentText(getString(R.string.long_coffee_ready));

                        mOnOff.setEnabled(true);
                        mShortCoffeeButton.setEnabled(true);
                        mLongCoffeeButton.setEnabled(true);
                        mSpeechButton.setEnabled(true);
                        mSpeechButton.setAlpha(1.0f);
                        sendNotification(coffeeId);
                    } else if (mTotalTime <= 0) {
                        //Time limit exceeded
                        mCoffeeProgressDialog.dismiss();
                        coffeeReadyDialog(0); //Notifies the error

                        if (mNotifyMgr != null) mNotifyMgr.cancelAll();

                        mBuilder.setContentText(getString(R.string.error_communication));

                        mOnOff.setEnabled(true);
                        mShortCoffeeButton.setEnabled(true);
                        mLongCoffeeButton.setEnabled(true);
                        mSpeechButton.setEnabled(true);
                        mSpeechButton.setAlpha(1.0f);

                        sendNotification(coffeeId);
                    } else if (isCoffeeInProgress()) {
                        //Coffee machine in use
                        mCoffeeProgressDialog.dismiss();
                        coffeeReadyDialog(2); //Notifies that the coffee machine is being used

                        if (mNotifyMgr != null) mNotifyMgr.cancelAll();

                        mBuilder.setContentText(getString(R.string.machine_in_use));

                        mOnOff.setEnabled(true);
                        mShortCoffeeButton.setEnabled(true);
                        mLongCoffeeButton.setEnabled(true);
                        mSpeechButton.setEnabled(true);
                        mSpeechButton.setAlpha(1.0f);

                        sendNotification(coffeeId);
                    } else {
                        //Timer error
                        mHandlerTimer.postDelayed(this, 1000);
                    }
                }
                catch (Exception e) {
                    //The Runnable thread was interrupted.
                }
            }
        };
        mHandlerTimer.postDelayed(runnable, 1000);
    }

    /**
     * Method to turn off the switch.
     */
    public void turnOffSwitch() {
        Log.d("DEBUG", "Não foi possível estabelecer conexão entre AWS-DB. Tente novamente.");
        mOnOff.setChecked(false);
        mOnOff.setEnabled(true);
    }

    /**
     * Method to cancel the TimerTask and enable the switch.
     */
    public void coffeeMachineState() {
        mMessageTimer.cancelTimerTask();
        mOnOff.setEnabled(true);
    }

    /**
     * Before making coffee, the system checks if everything is ok. This verification is divided on
     * two levels.
     *
     * @return
     */
    public boolean isEverythingOK() {
        //First level
        if (systemIsReady()) {

            //Second level
            if (coffeeMakerIsReady()) {
                return true;
            } else {
                Log.d(LOG_TAG, "Erro da cafeteira.");
            }
        } else {
            Log.d(LOG_TAG, "Erro do sistema.");
            ToastMessage.setToastMessage(mContext,
                    getString(R.string.connection_error), Toast.LENGTH_SHORT);

        }
        return false;
    }

    /**
     * The first level of verification before making coffee. It checks:
     * - If there is Internet connection
     * - If there is AWS connection
     * - If there is Bluetooth connection
     *
     * @return is the status of the first level of verification
     */
    public boolean systemIsReady() {
        if (InternetUtils.hasInternetConnection(this)) {
            Log.d(LOG_TAG, "Internet OK.");

            if (mAWSConnection.isConnected()) {
                Log.d(LOG_TAG, "AWS OK.");

                if (hasBluetoothConnection()) {
                    Log.d(LOG_TAG, "BLE OK.");
                    return true;

                } else {
                    Log.d(LOG_TAG, "Bluetooth desconectado.");
                }
            } else {
                Log.d(LOG_TAG, "AWS desconectado.");
            }
        } else {
            Log.d(LOG_TAG, "Internet desconectada.");
        }
        return false;
    }

    /**
     * The second level of verification before making coffee. It checks:
     * - If coffee machine was turned on
     * - If the coffee machine is not being used by another user
     * - If the cup is positioned
     * - If there is enough coffee
     * - If there is enough water
     *
     * @return is the status of the second level of verification
     */
    public boolean coffeeMakerIsReady() {
        if (mOnOff.isChecked()) {

            if (!isCoffeeInProgress()) {
                Log.d(LOG_TAG, "Cafeteira livre.");

                if (mAWSConnection.isGlassPositioned()) {
                    Log.d(LOG_TAG, "Copo OK.");

                    if (isCoffeeLevelEnough()) {
                        Log.d(LOG_TAG, "Nível de café OK");

                        if (!mAWSConnection.isWaterLevelEmpty()) {
                            Log.d(LOG_TAG, "Nível de água OK");
                            return true;

                        } else {
                            Log.d(LOG_TAG, "Nível de água insuficiente.");
                            ToastMessage.setToastMessage(mContext,
                                    getString(R.string.water_level_empty), Toast.LENGTH_SHORT);
                        }
                    } else {
                        Log.d(LOG_TAG, "Nível de café insuficiente.");
                        ToastMessage.setToastMessage(mContext,
                                getString(R.string.coffee_level_empty), Toast.LENGTH_SHORT);
                    }
                } else {
                    Log.d(LOG_TAG, "Copo não posicionado.");
                    ToastMessage.setToastMessage(mContext,
                            getString(R.string.no_glass), Toast.LENGTH_SHORT);
                }
            } else {
                Log.d(LOG_TAG, "Cafeteira em uso.");
                ToastMessage.setToastMessage(mContext,
                        getString(R.string.doing_coffee), Toast.LENGTH_LONG);
            }
        } else {
            Log.d(LOG_TAG, "Cafeteira desligada.");
            ToastMessage.setToastMessage(mContext,
                    getString(R.string.coffee_machine_off), Toast.LENGTH_LONG);
        }

        return false;
    }



    /**
     * Method to set the variable which notifies if there is Bluetooth connection. The DragonBoard
     * sends a message through TOPIC_ERROR if it cannot connects to CSR1011. The two acceptable
     * messages to assume that there is no Bluetooth connection are "write_char_error" and
     * "connection_error".
     *
     * @param message is a variable which contains the message sent by the DragonBoard.
     */
    public void checkBluetoothConnection(String message) {
        boolean isBluetoothConnected;

        if (message.equals("write_char_error") || message.equals("connection_error")) {
            Log.d(LOG_TAG, "Bluetooth desconectado.");
            isBluetoothConnected = false;
            disableComponents();
        } else {
            Log.d(LOG_TAG, "Bluetooth OK.");
            isBluetoothConnected = true;
            enableComponents();
        }

        this.mBluetoothIsConnected = isBluetoothConnected;
        setImageError();
    }

    /**
     * Method to check if the coffee is ready.
     *
     * @param message
     */
    public void checkIfCoffeeIsFinished(String message) {
        boolean isCoffeeFinished = false, isCoffeeInProgress = false;

        if (message.equals("coffee_ready")) {
            isCoffeeFinished = true;
            isCoffeeInProgress = false;
        } else if (message.equals(Constants.TOPIC_ERROR + Constants.IOT)) {
            isCoffeeFinished = false;
            isCoffeeInProgress = false;
        } else if (message.equals("qualcomm/CoffeeMachine/TurnOnOff/Android")) {
            ToastMessage.setToastMessage(this, getString(R.string.coffee_maker_error),      
                    Toast.LENGTH_SHORT);
        }

        this.mCoffeeIsFinished = isCoffeeFinished;
        this.mCoffeeInProgress = isCoffeeInProgress;
    }

    /**
     * @return
     */
    public boolean wasCoffeeFinished() {
        return mCoffeeIsFinished;
    }

    /**
     * @return
     */
    public boolean isCoffeeInProgress() {
        return mCoffeeInProgress;
    }

    /**
     * @return
     */
    public boolean isCoffeeLevelEnough() {
        if (mAWSConnection.isCoffeeLevelFull() && mProgressBarPermission) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * @param buttonView
     * @param isChecked
     */
    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
        if (InternetUtils.hasInternetConnection(this)) {
            if (isChecked) {
                if (mAWSConnection.isConnected() && hasBluetoothConnection()) {
                    Log.d(LOG_TAG, "Starting coffee machine...");
                    mOnOff.setEnabled(false);
                    mMessageTimer.startTimerTask();
                    mAWSConnection.topicPublish(String.valueOf(Constants.ON),
                            Constants.TOPIC_TURN_COFFEE_MACHINE + Constants.ANDROID);
                } else {
                    mText = getString(R.string.lost_connection);
                    mOnOff.setChecked(false);
                    setImageError();
                }
            } else {
                Log.d(LOG_TAG, "Desligando cafeteira...");
                if (mAWSConnection.isConnected() && hasBluetoothConnection()) {
                    mAWSConnection.topicPublish(String.valueOf(Constants.OFF),
                            Constants.TOPIC_TURN_COFFEE_MACHINE + Constants.ANDROID);
                } else {
                    mText = getString(R.string.lost_connection);
                    mOnOff.setChecked(true);
                    setImageError();
                }
            }
        } else {
            if (isChecked) {
                mText = getString(R.string.lost_connection);
                mOnOff.setChecked(false);
            } else {
                mText = getString(R.string.lost_connection);
                mOnOff.setChecked(true);
            }
        }
    }

    /**
     *
     */
    public class ActivityLifecycleCallbackReceiver implements
            Application.ActivityLifecycleCallbacks {
        @Override
        public void onActivityCreated(Activity activity, Bundle savedInstanceState) {

        }

        @Override
        public void onActivityStarted(Activity activity) {

        }

        @Override
        public void onActivityResumed(Activity activity) {
            hasActivityResumed = true;
            hasActivityPaused = false;
            //hasActivityResumed = false;
            if (!mOnOff.isEnabled() && !mShortCoffeeButton.isEnabled()
                    && !mLongCoffeeButton.isEnabled() && !mIsFirstLaunch) {
                enableComponents();
                mIsFirstLaunch = true;
                mIsPermissionDenied = false;
            }
        }

        @Override
        public void onActivityPaused(Activity activity) {
            hasActivityPaused = true;
            hasActivityResumed = false;
        }

        @Override
        public void onActivitySaveInstanceState(Activity activity, Bundle outState) {

        }

        @Override
        public void onActivityStopped(Activity activity) {
            hasActivityStopped = true;
            hasActivityResumed = false;
            mIsFirstLaunch = false;
        }

        @Override
        public void onActivityDestroyed(Activity activity) {

        }
    }


}
