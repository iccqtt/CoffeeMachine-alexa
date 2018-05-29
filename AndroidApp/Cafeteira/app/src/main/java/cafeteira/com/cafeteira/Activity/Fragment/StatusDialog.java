package cafeteira.com.cafeteira.Activity.Fragment;

import android.app.AlertDialog;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import cafeteira.com.cafeteira.R;
import cafeteira.com.cafeteira.Utils.Constants;


public class StatusDialog extends DialogFragment {

    private TextView mTextStatus;

    private String mStatus;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        Bundle bundle = this.getArguments();
        if (bundle != null) {
            this.mStatus = bundle.getString(Constants.STATUS_TAG);
        }
        return createDialog();
    }

    private Dialog createDialog() {

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());

        LayoutInflater inflater = (LayoutInflater)
                getActivity().getSystemService(Context.LAYOUT_INFLATER_SERVICE);

        View layout = inflater.inflate(R.layout.dialog_layout, null);

        builder.setTitle(getString(R.string.statusDialog));

        mTextStatus = (TextView) layout.findViewById(R.id.TextStatus);

        if (mStatus != null) {
            mTextStatus.setText(mStatus);
        }

        builder.setPositiveButton(getString(R.string.ok), new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.cancel();
            }
        });

        builder.setView(layout);

        return builder.create();
    }
}
