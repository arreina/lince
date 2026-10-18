// 🐆 Lince para Android: la actividad del juego. Es la de SDL2 (SDLActivity),
// que carga libSDL2.so y libmain.so y llama a SDL_main, con un arreglo: al
// volver a la app (tras ir a otra) Android vuelve a enseñar las barras del
// sistema, y SDL ya no las esconde (al minimizarse deja de estar a pantalla
// completa y no lo recupera). Un juego de Lince en el móvil va siempre a
// pantalla completa: aquí se esconden otra vez cada vez que recupera el foco.
package org.lince;

import android.os.Build;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import org.libsdl.app.SDLActivity;

public class LinceActivity extends SDLActivity {
    private final Runnable esconderBarras = new Runnable() {
        @Override
        public void run() {
            mFullscreenModeActive = true;         /* y SDL las vuelve a esconder si salen */
            if (Build.VERSION.SDK_INT >= 30) {
                WindowInsetsController c = getWindow().getInsetsController();
                if (c != null) {
                    c.hide(WindowInsets.Type.systemBars());
                    c.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            }
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    };

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            View v = getWindow().getDecorView();
            v.removeCallbacks(esconderBarras);
            v.postDelayed(esconderBarras, 300);
        }
    }
}
