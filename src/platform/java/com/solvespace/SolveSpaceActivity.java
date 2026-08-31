package com.solvespace;

import android.app.*;
import android.content.*;
import android.content.res.*;
import android.graphics.*;
import android.graphics.drawable.*;
import android.graphics.fonts.*;
import android.net.*;
import android.os.*;
import android.provider.DocumentsContract;
import android.text.*;
import android.util.*;
import android.view.*;
import android.view.inputmethod.*;
import android.widget.*;
import java.io.*;
import java.util.*;

public class SolveSpaceActivity extends Activity
implements SurfaceHolder.Callback2, ActionBar.OnNavigationListener,
View.OnTouchListener, View.OnGenericMotionListener, TextView.OnEditorActionListener,
DialogInterface.OnCancelListener, DialogInterface.OnClickListener,
PopupWindow.OnDismissListener, DialogInterface.OnDismissListener
{
    private final static String TAG = "SolveSpace";
    private final static int
    BTN_NO = 1,
    BTN_YES = 2,
    BTN_CANCEL = 4,
    BTN_OK = 8;
    static {
        System.loadLibrary("solvespace");
    }

    private Handler hand = new MainHandler();
    private ArrayList<String> wins;
    private ArrayAdapter<String> sa;
    private SurfaceView sv;
    private PopupWindow pw;
    private long pmenu;
    private boolean fromCreate;

    // Vertical scrollbar (rendered as the SurfaceView foreground).
    private GradientDrawable mScrollbar;
    private int mScrollbarWidth = 6;
    private int mViewWidth;
    private int mViewHeight;
    // Scrollbar geometry in normalized units (matching the C++ side).
    private double mScrollMin;
    private double mScrollMax;
    private double mScrollPage;
    private double mScrollPos;
    private boolean mScrollVisible;
    // True while the user is actively dragging the scrollbar thumb.
    private boolean mScrollDragging;

    // Mouse states
    private float currX, currY;
    private int mouseDown;

    // Layout constants for the scrollbar.
    private static final int SCROLLBAR_TRACK_PADDING = 6; // dp

    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        Configuration conf = getResources().getConfiguration();
        notifyOrientation(conf);
        super.onCreate(savedInstanceState);
        SurfaceView sv = new SurfaceView(this);
        this.sv = sv;

        // Build the foreground as a LayerDrawable containing the vertical
        // scrollbar thumb pinned to the right edge (gravity=right). The C++
        // side updates the thumb's top/bottom insets via JNI to reflect the
        // current scroll position and length.
        float density = getResources().getDisplayMetrics().density;
        int sbWidth = (int)(SCROLLBAR_TRACK_PADDING * density);
        mScrollbarWidth = sbWidth;
        GradientDrawable thumb = new GradientDrawable();
        thumb.setColor(0x80ffffff);
        mScrollbar = thumb;
        sv.setForeground(thumb);

        setContentView(sv, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT
        ));
        wins = new ArrayList<>();
        sa = new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, wins);
        sa.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        ActionBar act = getActionBar();
        act.setDisplayShowTitleEnabled(false);
        act.setNavigationMode(ActionBar.NAVIGATION_MODE_LIST);
        act.setListNavigationCallbacks(sa, this);
        sv.getHolder().addCallback(this);
        sv.setOnGenericMotionListener(this);
        sv.setOnTouchListener(this);
        registerForContextMenu(sv);
        fromCreate = true;
    }

    public final void sendDelayed(final long timer, long timeout) {
        if (timeout < 0) {
            sv.postOnAnimation(new Runnable(){
                public void run() {
                    MainHandler.nativeRun(timer);
                }
            });
        } else {
            Message msg = Message.obtain(hand, MainHandler.HD_NRUN, timer);
            hand.sendMessageDelayed(msg, timeout);
        }
    }

    @Override
    public native boolean onCreateOptionsMenu(Menu menu);

    private void notifyOrientation(Configuration conf) {
        View v = getWindow().getDecorView();
        v.setSystemUiVisibility(
            conf.orientation == Configuration.ORIENTATION_LANDSCAPE ?
            (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN)
            : 0
        );
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig)
    {
        super.onConfigurationChanged(newConfig);
        notifyOrientation(newConfig);
    }

    @Override
    public void surfaceChanged(SurfaceHolder p1, int p2, int p3, int p4) {
        mViewWidth = p3;
        mViewHeight = p4;
        nativeOnWindowChanged(getActionBar().getSelectedNavigationIndex());
    }

    @Override
    public void surfaceCreated(SurfaceHolder p1) {
        if (fromCreate) {
            String[] lcs;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                LocaleList lclst = getResources().getConfiguration().getLocales();
                final int siz = lclst.size();
                lcs = new String[siz];
                for (int i=0; i<siz; i++) {
                    Locale lc = lclst.get(i);
                    lcs[i] = lc.getLanguage()+"_"+lc.getCountry();
                }
            } else {
                lcs = new String[1];
                Locale lc = Locale.getDefault();
                lcs[0] = lc.getLanguage()+"_"+lc.getCountry();
            }
            nativeInit(p1.getSurface(), getAssets(), lcs);
            fromCreate = false;
        } else {
            nativeSetSurface(p1.getSurface());
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder p1) {
    }

    @Override
    public void surfaceRedrawNeeded(SurfaceHolder p1) {
    }

    public void popup(long pmenu) {
        this.pmenu = pmenu;
        if (currX >= 0 && currY >= 0)
            sv.showContextMenu(currX, currY);
        else
            sv.showContextMenu();
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        nativeOnCreateContextMenu(menu, pmenu);
    }

    @Override
    public void onContextMenuClosed(Menu menu)
    {
        nativeOnContextMenuClosed(pmenu);
        pmenu = 0;
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        return onOptionsItemSelected(item);
    }

    @Override
    protected void onDestroy() {
        nativeClear();
        super.onDestroy();
    }

    public void showEditor(float x, float y, float fontHeight, float minWidth, boolean isMono, String text) {
        EditText ed = new EditText(this);
        if (isMono) ed.setTypeface(Typeface.MONOSPACE);
        ed.setImeOptions(EditorInfo.IME_ACTION_DONE);
        ed.setText(text);
        ed.setOnEditorActionListener(this);
        ed.setTextColor(0xff000000);
        ed.setFocusable(true);
        PopupWindow pw = new PopupWindow(
            ed,
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT,
            true);
        this.pw = pw;
        pw.setBackgroundDrawable(new ColorDrawable(0xffffffff));
        pw.setAnimationStyle(0);
        pw.setOutsideTouchable(false);
        pw.setTouchable(true);
        pw.setTouchModal(false);
        pw.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        pw.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        pw.setOnDismissListener(this);
        float density = getDensity();
        int xi = (int)(x * density)-ed.getPaddingLeft();
        int yi = (int)(y * density)+getActionBar().getHeight();
        pw.showAtLocation(getWindow().getDecorView(), Gravity.TOP|Gravity.LEFT, xi, yi);
        ed.requestFocus();
    }

    // ---- Scrollbar geometry (called from C++ via JNI) ----

    // Stores the scrollbar configuration and redraws the thumb.
    // All geometry arguments are in normalized scrollbar units.
    protected void setScrollbar(double min, double max, double page, double pos, boolean visible) {
        mScrollMin = min;
        mScrollMax = max;
        mScrollPage = page;
        mScrollPos = pos;
        mScrollVisible = visible;
        layoutScrollbar();
    }

    // Updates the LayerDrawable item (thumb) geometry: its horizontal width
    // (via the right inset, keeping it pinned to the right gravity), and its
    // vertical start/length via top/bottom insets. Hidden when not visible.
    private void layoutScrollbar() {
        if (mScrollbar == null) return;
        if (!mScrollVisible || mViewHeight <= 0 ||
            mScrollMax <= mScrollMin) {
            mScrollbar.setAlpha(0);
            return;
        }
        mScrollbar.setAlpha(0x80);

        int height = mViewHeight;

        double range = Math.max(1e-6, mScrollMax - mScrollMin);
        // Thumb length is proportional to the page size within the range.
        int thumbH = (int)(mScrollPage / (mScrollMax - mScrollMin) * height);

        double ratio = (mScrollPos - mScrollMin) / range;
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        int top = (int)(ratio * height);
        int bottom = top + thumbH;
        if (bottom > height) bottom = height;

        // Pin the thumb to the right edge; its width is controlled by the
        // right inset relative to the right gravity of the layer.
        mScrollbar.setBounds(mViewWidth-mScrollbarWidth, top, mViewWidth, bottom);
        mScrollbar.invalidateSelf();
    }

    // Callback from C++ that the scroll position was adjusted by the user.
    private native void nativeOnScrollbarAdjusted(double pos);

    @Override
    public void onDismiss() {
        PopupWindow pw = this.pw;
        if (pw != null) {
            nativeOnEditorDone(((EditText)pw.getContentView()).getText().toString());
        }
    }

    public void hideEditor() {
        PopupWindow pw = this.pw;
        this.pw = null;
        if (pw != null) {
            pw.dismiss();
        }
    }

    public boolean isEditorVisible() {
        return pw != null && pw.isShowing();
    }

    boolean showing = false;
    int ret = BTN_CANCEL;

    public int showMessageDialog(String title, String msg, int btn) {
        if (showing) {
            return BTN_CANCEL;
        }
        showing = true;
        AlertDialog.Builder bd = new AlertDialog.Builder(this);
        bd.setTitle(title);
        bd.setMessage(msg);
        bd.setOnCancelListener(this);
        bd.setOnDismissListener(this);
        if ((btn&(BTN_OK|BTN_YES)) != 0) {
            bd.setPositiveButton((btn&BTN_YES)!=0?R.string.yes:android.R.string.ok, this);
        }
        if ((btn&(BTN_NO|BTN_CANCEL)) != 0) {
            bd.setNegativeButton((btn&BTN_NO)!=0?R.string.no:android.R.string.cancel, this);
        }
        if (btn == (BTN_CANCEL|BTN_YES|BTN_NO)) {
            bd.setNeutralButton(android.R.string.cancel, this);
        }
        ret = btn;
        bd.create().show();
        try {Looper.loop();} catch (RuntimeException re) {}
        showing = false;
        return ret;
    }

    @Override
    public void onClick(DialogInterface p1, int p2) {
        int dret;
        switch (p2) {
            case DialogInterface.BUTTON_POSITIVE:
                dret = (ret&BTN_OK)==0?BTN_YES:BTN_OK;
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                dret = (ret&BTN_NO)==0?BTN_CANCEL:BTN_NO;
                break;
            case DialogInterface.BUTTON_NEUTRAL:
            default:
                dret = BTN_CANCEL;
                break;
        }
        ret = dret;
    }

    @Override
    public void onCancel(DialogInterface p1) {
        ret = BTN_CANCEL;
    }

    @Override
    public void onDismiss(DialogInterface p1) {
        hand.obtainMessage(MainHandler.HD_RET).sendToTarget();
    }

    private String fname;
    public String showFileDialog(boolean isSave, String title, String[] exts) {
        if (showing) return null;
        showing = true;
        Intent it = new Intent(isSave?Intent.ACTION_CREATE_DOCUMENT:Intent.ACTION_OPEN_DOCUMENT);
        it.setType("*/*");
        it.addCategory(Intent.CATEGORY_OPENABLE);
        if (isSave && title != null) {
            it.putExtra(Intent.EXTRA_TITLE, title);
        }
        startActivityForResult(it, 0);
        fname = null;
        try { Looper.loop(); } catch (RuntimeException re) {}
        showing = false;
        String fnam = fname;
        if (fnam == null) return null;
        // Apply filters
        int pt = fnam.lastIndexOf(".");
        int l = exts == null ? 0 : exts.length;
        if (pt >= 0 && l > 0) {
            String ext = fnam.substring(pt+1, fnam.length());
            int i=0;
            for (; i<l ;i++) {
                if (ext.equals(exts[i])) {
                    break;
                }
            }
            if (i==l && showMessageDialog("Warning",
                    "Not recognized file extension ."+ext+", open anyway?",
                    BTN_OK|BTN_CANCEL)!=BTN_OK) {
                fnam = null;
            }
        }
        return fnam;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 0) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                // Persist the URI grant so the document remains accessible
                // after the process is killed or the device is rebooted
                // (e.g. from the recent-files list or autosave).
                int flags = data.getFlags()
                    & (Intent.FLAG_GRANT_READ_URI_PERMISSION
                       | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                if (flags != 0) {
                    getContentResolver().takePersistableUriPermission(data.getData(), flags);
                }
                fname = data.getDataString();
            } else {
                fname = null;
            }
            hand.obtainMessage(MainHandler.HD_RET).sendToTarget();
        }
    }

    /**
     * Opens a Storage Access Framework content URI via ContentResolver.
     * Called from native code (Platform::OpenFile). The returned file
     * descriptor is detached from its ParcelFileDescriptor, so ownership
     * passes to the caller, which must close it.
     *
     * @param uri  the content:// URI to open
     * @param mode one of "r", "w", "wt", "wa", "rw", "rwt"
     * @return a raw file descriptor, or -1 on failure
     */
    public int openContentFile(String uri, String mode) {
        try {
            ParcelFileDescriptor pfd =
                getContentResolver().openFileDescriptor(Uri.parse(uri), mode);
            if (pfd == null) return -1;
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "openContentFile(" + uri + ", " + mode + ") failed", e);
            return -1;
        }
    }

    /**
     * Deletes a Storage Access Framework content URI via DocumentsContract.
     * Called from native code (Platform::RemoveFile).
     *
     * @param uri the content:// URI to delete
     * @return true on success
     */
    public boolean deleteContentFile(String uri) {
        try {
            return DocumentsContract.deleteDocument(getContentResolver(), Uri.parse(uri));
        } catch (Exception e) {
            Log.e(TAG, "deleteContentFile(" + uri + ") failed", e);
            return false;
        }
    }

    /**
     * Returns the absolute path of the app's private files directory.
     * Called from native code; used to store autosave/backup companions
     * of Storage Access Framework documents.
     */
    public String getInternalStoragePath() {
        return getFilesDir().getAbsolutePath();
    }

    public void openUrl(String str) {
        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(str)));
    }

    public void setFullScreen(boolean fs) {
        getWindow().setFlags(fs ? WindowManager.LayoutParams.FLAG_FULLSCREEN : 0, WindowManager.LayoutParams.FLAG_FULLSCREEN);
    }

    public float getDensity() {
        return getResources().getDisplayMetrics().density;
    }

    @Override
    public boolean onGenericMotion(View v, MotionEvent ev) {
        float density = getDensity();
        float dist = ev.getAxisValue(MotionEvent.AXIS_VSCROLL)/density;
        v.postOnAnimation(new Motion(ev.getAction(), ev.getX()/density, ev.getY()/density, dist, ev.getButtonState(), ev.getMetaState()));
        return true;
    }

    @Override
    public native boolean onOptionsItemSelected(MenuItem item);

    @Override
    public boolean onTouch(View v, MotionEvent ev) {
        float density = getDensity();
        handleScrollbarTouch(ev, density);

        if (mScrollDragging) {
            return true;
        }

        int act = ev.getActionMasked(), meta=ev.getMetaState();
        int count = ev.getPointerCount();
        int bstat;
        float x = ev.getX(), y = ev.getY();
        if (ev.isFromSource(InputDevice.SOURCE_MOUSE)) {
            bstat = ev.getButtonState();
            // scrcpy --mouse=uhid would loss the buttonstate when action up
            if (act == MotionEvent.ACTION_DOWN)
                mouseDown = bstat;
            else if (act == MotionEvent.ACTION_UP || act == MotionEvent.ACTION_CANCEL) {
                bstat = mouseDown;
                mouseDown = 0;
            }
            currX = x;
            currY = y;
        } else {
            bstat = count == 1 ? MotionEvent.BUTTON_PRIMARY : count == 2 ?MotionEvent.BUTTON_SECONDARY : MotionEvent.BUTTON_TERTIARY;
            currX = -1.f;
            currY = -1.f;
        }
        final double dist;
        if (count == 2) {
            float x1 = ev.getX(0), x2 = ev.getX(1);
            float y1 = ev.getY(0), y2 = ev.getY(1);
            dist = Math.hypot(x1-x2,y1-y2)/density;
        } else {
            dist = 0.;
        }
        v.postOnAnimation(new Motion(act, x/density, y/density, dist, bstat, meta));
        return true;
    }

    // Adjusts the scroll position when the user touches or drags the scrollbar
    // thumb/track, and refreshes the drawn thumb to follow the finger.
    private void handleScrollbarTouch(MotionEvent ev, float density) {
        double height = mViewHeight/density;
        if (!mScrollVisible || height <= 0) return;
        float y = ev.getY() / density;
        double range = Math.max(1e-6, mScrollMax - mScrollMin);
        double scale = mScrollPage / (mScrollMax - mScrollMin);
        int thumbH = (int)(height * scale);

        int act = ev.getActionMasked();
        if (act == MotionEvent.ACTION_DOWN) {
            // Begin dragging if the touch is on the thumb or in the scrollbar
            // track area on the right edge.
            int sbLeft = mViewWidth - mScrollbarWidth - (int)(SCROLLBAR_TRACK_PADDING*density);
            if (xInScrollbar(ev, sbLeft) &&
                (y >= 0 && y <= height)) {
                mScrollDragging = true;
                nativeOnScrollbarAdjusted((y - thumbH * 0.5)*range/height);
            }
        } else if (act == MotionEvent.ACTION_MOVE && mScrollDragging) {
            nativeOnScrollbarAdjusted((y - thumbH * 0.5)*range/height);
        } else if (act == MotionEvent.ACTION_UP || act == MotionEvent.ACTION_CANCEL) {
            mScrollDragging = false;
        }
    }

    private boolean xInScrollbar(MotionEvent ev, int sbLeft) {
        return ev.getX() >= sbLeft && ev.getX() <= mViewWidth;
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent ev) {
        if (!super.onKeyDown(keyCode, ev))
            sv.postOnAnimation(new Key(ev));
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent ev) {
        if (!super.onKeyUp(keyCode, ev))
            sv.postOnAnimation(new Key(ev));
        return true;
    }

    public void onWinAdded(boolean isTop) {
        int count = sa.getCount();
        sa.add("#"+count);
        sa.notifyDataSetChanged();
        if (isTop)
            getActionBar().setSelectedNavigationItem(count);
    }

    public void setWinTitle(int winId, String title) {
        wins.set(winId, title);
        sa.notifyDataSetChanged();
    }

    @Override
    public boolean onNavigationItemSelected(int p1, long p2) {
        nativeOnWindowChanged(p1);
        invalidateOptionsMenu();
        return true;
    }

    @Override
    public boolean onEditorAction(TextView p1, int p2, KeyEvent p3) {
        nativeOnEditorDone(p1.getText().toString());
        return true;
    }

    private void toast(String tst) {
        Toast.makeText(this, tst, 0).show();
    }

    public static String[] getSysFonts() {
        Set<Font> fonts = SystemFonts.getAvailableFonts();
        String[] arr = new String[fonts.size()];
        int i=0;
        for (Font f:fonts) {
            arr[i++] = f.getFile().getPath();
        }
        return arr;
    }

    private native void nativeInit(Surface suf, AssetManager amgr, String[] strs);
    private native void nativeClear();
    private native void nativeSetSurface(Surface suf);
    protected static native boolean nativeOnMotionEvent(int action, float x, float y, double dist, int button, int metastate);
    protected static native boolean nativeOnKeyEvent(int keystate, int keyCode, int metastate);
    private static native void nativeOnWindowChanged(int winId);
    private static native void nativeOnEditorDone(String text);
    private static native void nativeOnCreateContextMenu(Menu menu, long pemnu);
    private static native void nativeOnContextMenuClosed(long pmenu);

    static class Motion implements Runnable {
        int act, button, metastate;
        float x, y;
        double dist;
        public Motion(int ac, float x, float y, double dis, int btn, int mt) {
            act = ac;
            this.x = x;
            this.y = y;
            dist = dis;
            button = btn;
            metastate = mt;
        }
        public void run() {
            nativeOnMotionEvent(act, x, y, dist, button, metastate);
        }
    }

    static class Key implements Runnable {
        int act, keyenc, meta;
        public Key(KeyEvent ke) {
            act = ke.getAction();
            int kcode = ke.getKeyCode();
            if (kcode >= KeyEvent.KEYCODE_F1 && kcode <= KeyEvent.KEYCODE_F12)
                keyenc = -kcode;
            else
                keyenc = ke.getUnicodeChar(0);
            meta = ke.getMetaState();
        }
        public void run() {
            nativeOnKeyEvent(act, keyenc, meta);
        }
    }

    public void finish() {
        super.finish();
    }
}
