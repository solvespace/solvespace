package com.solvespace;
import android.os.*;

public class MainHandler extends Handler
{

    public static final int HD_NRUN = 1, HD_RET = 2;
    @Override
    public void handleMessage(Message msg) {
        if (msg.what == HD_NRUN)
            nativeRun((Long)msg.obj, false);
        else if (msg.what == HD_RET)
            throw new RuntimeException();
    }

    public final static native void nativeRun(long obj, boolean frame);
}
